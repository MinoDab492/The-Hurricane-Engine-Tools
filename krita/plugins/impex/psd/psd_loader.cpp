/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "psd_loader.h"

#include <QApplication>

#include <QFileInfo>
#include <QStack>

#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoCompositeOp.h>
#include <KoUnit.h>

#include <kis_annotation.h>
#include <kis_types.h>
#include <kis_paint_layer.h>
#include <KisDocument.h>
#include <kis_image.h>
#include <kis_group_layer.h>
#include <kis_paint_device.h>
#include <kis_transaction.h>
#include <kis_transparency_mask.h>
#include <kis_generator_layer.h>
#include <kis_generator_registry.h>

#include <kis_asl_layer_style_serializer.h>
#include <asl/kis_asl_xml_parser.h>
#include "KisResourceServerProvider.h"

#include "psd.h"
#include "psd_header.h"
#include "psd_colormode_block.h"
#include "psd_utils.h"
#include "psd_resource_section.h"
#include "psd_layer_section.h"
#include "psd_resource_block.h"
#include "psd_image_data.h"
#include "kis_image_barrier_locker.h"
#include "KisEmbeddedResourceStorageProxy.h"

PSDLoader::PSDLoader(KisDocument *doc)
    : m_image(0)
    , m_doc(doc)
    , m_stop(false)
{
}

PSDLoader::~PSDLoader()
{
}

KisImportExportErrorCode PSDLoader::decode(QIODevice &io)
{
    // open the file

    dbgFile << "pos:" << io.pos();

    PSDHeader header;
    if (!header.read(io)) {
        dbgFile << "failed reading header: " << header.error;
        return ImportExportCodes::FileFormatIncorrect;
    }

    dbgFile << header;
    dbgFile << "Read header. pos:" << io.pos();

    PSDColorModeBlock colorModeBlock(header.colormode);
    if (!colorModeBlock.read(io)) {
        dbgFile << "failed reading colormode block: " << colorModeBlock.error;
        return ImportExportCodes::FileFormatIncorrect;
    }

    dbgFile << "Read color mode block. pos:" << io.pos();

    PSDImageResourceSection resourceSection;
    if (!resourceSection.read(io)) {
        dbgFile << "failed image reading resource section: " << resourceSection.error;
        return ImportExportCodes::FileFormatIncorrect;
    }
    dbgFile << "Read image resource section. pos:" << io.pos();

    PSDLayerMaskSection layerSection(header);
    if (!layerSection.read(io)) {
        dbgFile << "failed reading layer/mask section: " << layerSection.error;
        return ImportExportCodes::FileFormatIncorrect;
    }
    dbgFile << "Read layer/mask section. " << layerSection.nLayers << "layers. pos:" << io.pos();

    // Done reading, except possibly for the image data block, which is only relevant if there
    // are no layers.

    // Get the right colorspace
    QPair<QString, QString> colorSpaceId = psd_colormode_to_colormodelid(header.colormode,
                                                                         header.channelDepth);
    if (colorSpaceId.first.isNull()) {
        dbgFile << "Unsupported colorspace" << header.colormode << header.channelDepth;
        return ImportExportCodes::FormatColorSpaceUnsupported;
    }

    // Get the icc profile from the image resource section
    const KoColorProfile* profile = 0;
    if (resourceSection.resources.contains(PSDImageResourceSection::ICC_PROFILE)) {
        ICC_PROFILE_1039 *iccProfileData = dynamic_cast<ICC_PROFILE_1039*>(resourceSection.resources[PSDImageResourceSection::ICC_PROFILE]->resource);
        if (iccProfileData ) {
            profile = KoColorSpaceRegistry::instance()->createColorProfile(colorSpaceId.first,
                                                                           colorSpaceId.second,
                                                                           iccProfileData->icc);
            dbgFile  << "Loaded ICC profile" << profile->name();
            delete resourceSection.resources.take(PSDImageResourceSection::ICC_PROFILE);
        }
    }

    // Create the colorspace
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->colorSpace(colorSpaceId.first, colorSpaceId.second, profile);
    if (!cs) {
        return ImportExportCodes::FormatColorSpaceUnsupported;
    }

    // Creating the KisImage
    QFile *file = dynamic_cast<QFile *>(&io);
    QString name = file ? file->fileName() : "Imported";
    m_image = new KisImage(m_doc->createUndoStore(),  header.width, header.height, cs, name);
    Q_CHECK_PTR(m_image);
    KisImageBarrierLocker locker(m_image);

    // set the correct resolution
    if (resourceSection.resources.contains(PSDImageResourceSection::RESN_INFO)) {
        RESN_INFO_1005 *resInfo = dynamic_cast<RESN_INFO_1005*>(resourceSection.resources[PSDImageResourceSection::RESN_INFO]->resource);
        if (resInfo) {
            // check resolution size is not zero
            if (resInfo->hRes * resInfo->vRes > 0)
                m_image->setResolution(POINT_TO_INCH(resInfo->hRes), POINT_TO_INCH(resInfo->vRes));
            // let's skip the unit for now; we can only set that on the KisDocument, and krita doesn't use it.
            delete resourceSection.resources.take(PSDImageResourceSection::RESN_INFO);
        }
    }

    // Preserve all the annotations
    Q_FOREACH (PSDResourceBlock *resourceBlock, resourceSection.resources.values()) {
        m_image->addAnnotation(resourceBlock);
    }

    // Preserve the duotone colormode block for saving back to psd
    if (header.colormode == DuoTone) {
        KisAnnotationSP annotation = new KisAnnotation("DuotoneColormodeBlock",
                                                       i18n("Duotone Colormode Block"),
                                                       colorModeBlock.data);
        m_image->addAnnotation(annotation);
    }

    // Load embedded patterns early for fill layers.

    const QVector<QDomDocument> &embeddedPatterns =
        layerSection.globalInfoSection.embeddedPatterns;

    const QString storageLocation = m_doc->embeddedResourcesStorageId();

    KisEmbeddedResourceStorageProxy resourceProxy(storageLocation);

    KisAslLayerStyleSerializer serializer;
    if (!embeddedPatterns.isEmpty()) {
        Q_FOREACH (const QDomDocument &doc, embeddedPatterns) {
            serializer.registerPSDPattern(doc);
        }
        Q_FOREACH (KoPatternSP pattern, serializer.patterns()) {
            if (pattern && pattern->valid()) {
                resourceProxy.addResource(pattern);
                dbgFile << "Loaded embedded pattern: " << pattern->name();
            }
            else {
                qWarning() << "Invalid or empty pattern" << pattern;
            }
        }
    }

    // Read the projection into our single layer. Since we only read the projection when
    // we have just one layer, we don't need to later on apply the alpha channel of the
    // first layer to the projection if the number of layers is negative/
    // See https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_16000.
    if (layerSection.nLayers == 0) {
        dbgFile << "Position" << io.pos() << "Going to read the projection into the first layer, which Photoshop calls 'Background'";

        KisPaintLayerSP layer = new KisPaintLayer(m_image, i18n("Background"), OPACITY_OPAQUE_U8);

        PSDImageData imageData(&header);
        imageData.read(io, layer->paintDevice());

        m_image->addNode(layer, m_image->rootLayer());

        // Only one layer, the background layer, so we're done.
        return ImportExportCodes::OK;
    }

    // More than one layer, so now construct the Krita image from the info we read.

    QStack<KisGroupLayerSP> groupStack;
    groupStack.push(m_image->rootLayer());

    /**
     * PSD has a weird "optimization": if a group layer has only one
     * child layer, it omits it's 'psd_bounding_divider' section. So
     * fi you ever see an unbalanced layers group in PSD, most
     * probably, it is just a single layered group.
     */
    KisNodeSP lastAddedLayer;

    typedef QPair<QDomDocument, KisLayerSP> LayerStyleMapping;
    QVector<LayerStyleMapping> allStylesXml;
    using namespace std::placeholders;

    // read the channels for the various layers
    for(int i = 0; i < layerSection.nLayers; ++i) {

        PSDLayerRecord* layerRecord = layerSection.layers.at(i);
        dbgFile << "Going to read channels for layer" << i << layerRecord->layerName;
        KisLayerSP newLayer;
        if (layerRecord->infoBlocks.keys.contains("lsct") &&
            layerRecord->infoBlocks.sectionDividerType != psd_other) {

            if (layerRecord->infoBlocks.sectionDividerType == psd_bounding_divider && !groupStack.isEmpty()) {
                KisGroupLayerSP groupLayer = new KisGroupLayer(m_image, "temp", OPACITY_OPAQUE_U8);
                m_image->addNode(groupLayer, groupStack.top());
                groupStack.push(groupLayer);
                newLayer = groupLayer;
            }
            else if ((layerRecord->infoBlocks.sectionDividerType == psd_open_folder ||
                      layerRecord->infoBlocks.sectionDividerType == psd_closed_folder) &&
                     (groupStack.size() > 1 || (lastAddedLayer && !groupStack.isEmpty()))) {
                KisGroupLayerSP groupLayer;

                if (groupStack.size() <= 1) {
                    groupLayer = new KisGroupLayer(m_image, "temp", OPACITY_OPAQUE_U8);
                    m_image->addNode(groupLayer, groupStack.top());
                    m_image->moveNode(lastAddedLayer, groupLayer, KisNodeSP());
                } else {
                    groupLayer = groupStack.pop();
                }

                const QDomDocument &styleXml = layerRecord->infoBlocks.layerStyleXml;

                if (!styleXml.isNull()) {
                    allStylesXml << LayerStyleMapping(styleXml, groupLayer);
                }

                groupLayer->setName(layerRecord->layerName);
                groupLayer->setVisible(layerRecord->visible);

                QString compositeOp = psd_blendmode_to_composite_op(layerRecord->infoBlocks.sectionDividerBlendMode);

                // Krita doesn't support pass-through blend
                // mode. Instead it is just a property of a group
                // layer, so flip it
                if (compositeOp == COMPOSITE_PASS_THROUGH) {
                    compositeOp = COMPOSITE_OVER;
                    groupLayer->setPassThroughMode(true);
                }

                groupLayer->setCompositeOpId(compositeOp);

                newLayer = groupLayer;
            } else {
                /**
                 * In some files saved by PS CS6 the group layer sections seem
                 * to be unbalanced.  I don't know why it happens because the
                 * reporter didn't provide us an example file. So here we just
                 * check if the new layer was created, and if not, skip the
                 * initialization of masks.
                 *
                 * See bug: 357559
                 */

                warnKrita << "WARNING: Provided PSD has unbalanced group "
                          << "layer markers. Some masks and/or layers can "
                          << "be lost while loading this file. Please "
                          << "report a bug to Krita developers and attach "
                          << "this file to the bugreport\n"
                          << "    " << ppVar(layerRecord->layerName) << "\n"
                          << "    " << ppVar(layerRecord->infoBlocks.sectionDividerType) << "\n"
                          << "    " << ppVar(groupStack.size());
                continue;
            }
        } else {
            KisLayerSP layer;
            if (!layerRecord->infoBlocks.fillConfig.isNull()) {
                KisFilterConfigurationSP cfg;
                QDomDocument fillConfig;
                KisAslCallbackObjectCatcher catcher;
                if (layerRecord->infoBlocks.fillType == psd_fill_gradient) {
                    cfg = KisGeneratorRegistry::instance()->value("gradient")->defaultConfiguration(resourceProxy.resourcesInterface());

                    psd_layer_gradient_fill fill;
                    fill.imageWidth = m_image->width();
                    fill.imageHeight = m_image->height();
                    catcher.subscribeGradient("/null/Grad", std::bind(&psd_layer_gradient_fill::setGradient, &fill, _1));
                    catcher.subscribeBoolean("/null/Dthr", std::bind(&psd_layer_gradient_fill::setDither, &fill, _1));
                    catcher.subscribeBoolean("/null/Rvrs", std::bind(&psd_layer_gradient_fill::setReverse, &fill, _1));
                    catcher.subscribeUnitFloat("/null/Angl", "#Ang", std::bind(&psd_layer_gradient_fill::setAngle, &fill, _1));
                    catcher.subscribeEnum("/null/Type", "GrdT", std::bind(&psd_layer_gradient_fill::setType, &fill, _1));
                    catcher.subscribeBoolean("/null/Algn", std::bind(&psd_layer_gradient_fill::setAlignWithLayer, &fill, _1));
                    catcher.subscribeUnitFloat("/null/Scl ", "#Prc", std::bind(&psd_layer_gradient_fill::setScale, &fill, _1));
                    catcher.subscribePoint("/null/Ofst", std::bind(&psd_layer_gradient_fill::setOffset, &fill, _1));
                    KisAslXmlParser parser;
                    parser.parseXML(layerRecord->infoBlocks.fillConfig, catcher);
                    fillConfig = fill.getFillLayerConfig();

                } else if (layerRecord->infoBlocks.fillType == psd_fill_pattern) {
                    cfg = KisGeneratorRegistry::instance()->value("pattern")->defaultConfiguration(resourceProxy.resourcesInterface());

                    psd_layer_pattern_fill fill;
                    catcher.subscribeUnitFloat("/null/Angl", "#Ang", std::bind(&psd_layer_pattern_fill::setAngle, &fill, _1));
                    catcher.subscribeUnitFloat("/null/Scl ", "#Prc", std::bind(&psd_layer_pattern_fill::setScale, &fill, _1));
                    catcher.subscribeBoolean("/null/Algn", std::bind(&psd_layer_pattern_fill::setAlignWithLayer, &fill, _1));
                    catcher.subscribePoint("/null/phase", std::bind(&psd_layer_pattern_fill::setOffset, &fill, _1));
                    catcher.subscribePatternRef("/null/Ptrn", std::bind(&psd_layer_pattern_fill::setPatternRef, &fill, _1, _2));

                    KisAslXmlParser parser;
                    parser.parseXML(layerRecord->infoBlocks.fillConfig, catcher);
                    fillConfig = fill.getFillLayerConfig();

                } else {
                    cfg = KisGeneratorRegistry::instance()->value("color")->defaultConfiguration(resourceProxy.resourcesInterface());

                    psd_layer_solid_color fill;
                    fill.cs = m_image->colorSpace();
                    catcher.subscribeColor("/null/Clr ", std::bind(&psd_layer_solid_color::setColor, &fill, _1));
                    KisAslXmlParser parser;
                    parser.parseXML(layerRecord->infoBlocks.fillConfig, catcher);

                    fillConfig = fill.getFillLayerConfig();
                }
                cfg->fromXML(fillConfig.firstChildElement());
                cfg->createLocalResourcesSnapshot();
                KisGeneratorLayerSP genlayer = new KisGeneratorLayer(m_image, layerRecord->layerName, cfg, m_image->globalSelection());
                genlayer->setFilter(cfg);
                layer = genlayer;

            } else {
                layer = new KisPaintLayer(m_image, layerRecord->layerName, layerRecord->opacity);
                if (!layerRecord->readPixelData(io, layer->paintDevice())) {
                    dbgFile << "failed reading channels for layer: " << layerRecord->layerName << layerRecord->error;
                    return ImportExportCodes::FileFormatIncorrect;
                }
            }
            layer->setCompositeOpId(psd_blendmode_to_composite_op(layerRecord->blendModeKey));

            layer->setColorLabelIndex(layerRecord->labelColor);

            const QDomDocument &styleXml = layerRecord->infoBlocks.layerStyleXml;

            if (!styleXml.isNull()) {
                allStylesXml << LayerStyleMapping(styleXml, layer);
            }

            if (!groupStack.isEmpty()) {
                m_image->addNode(layer, groupStack.top());
            }
            else {
                m_image->addNode(layer, m_image->root());
            }
            layer->setVisible(layerRecord->visible);
            newLayer = layer;

        }

        Q_FOREACH (ChannelInfo *channelInfo, layerRecord->channelInfoRecords) {
            if (channelInfo->channelId < -1) {
                const KisGeneratorLayer *fillLayer = qobject_cast<KisGeneratorLayer *>(newLayer.data());
                if (fillLayer) {
                    if (!layerRecord->readMask(io, fillLayer->paintDevice(), channelInfo)) {
                        dbgFile << "failed reading masks for generator layer: " << layerRecord->layerName << layerRecord->error;
                    }
                } else {
                    KisTransparencyMaskSP mask = new KisTransparencyMask(m_image, i18n("Transparency Mask"));
                    mask->initSelection(newLayer);
                    if (!layerRecord->readMask(io, mask->paintDevice(), channelInfo)) {
                        dbgFile << "failed reading masks for layer: " << layerRecord->layerName << layerRecord->error;
                    }
                    m_image->addNode(mask, newLayer);
                }
            }
        }

        lastAddedLayer = newLayer;
    }

    if (!allStylesXml.isEmpty()) {
        Q_FOREACH (const LayerStyleMapping &mapping, allStylesXml) {

            serializer.readFromPSDXML(mapping.first);

            if (serializer.styles().size() == 1) {
                KisPSDLayerStyleSP layerStyle = serializer.styles().first();
                KisLayerSP layer = mapping.second;

                Q_FOREACH (KoAbstractGradientSP gradient, serializer.gradients()) {
                    if (gradient && gradient->valid()) {
                        resourceProxy.addResource(gradient);
                    }
                    else {
                        qWarning() << "Invalid or empty gradient" << gradient;
                    }
                }

                layerStyle->setName(layer->name());
                layerStyle->setResourcesInterface(resourceProxy.detachedResourcesInterface());
                if (!layerStyle->uuid().isNull()) {
                    layerStyle->setUuid(QUuid::createUuid());
                }
                layerStyle->setValid(true);

                resourceProxy.addResource(layerStyle);

                layer->setLayerStyle(layerStyle->cloneWithResourcesSnapshot(layerStyle->resourcesInterface(), 0));
            } else {
                warnKrita << "WARNING: Couldn't read layer style!" << ppVar(serializer.styles());
            }

        }
    }

    return ImportExportCodes::OK;
}

KisImportExportErrorCode PSDLoader::buildImage(QIODevice &io)
{
    return decode(io);
}


KisImageSP PSDLoader::image()
{
    return m_image;
}

void PSDLoader::cancel()
{
    m_stop = true;
}


