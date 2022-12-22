/*
 *  SPDX-FileCopyrightText: 2011 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_mimedata.h"
#include "kis_config.h"
#include "kis_node.h"
#include "kis_paint_device.h"
#include "kis_shared_ptr.h"
#include "kis_image.h"
#include "kis_layer.h"
#include "kis_shape_layer.h"
#include "kis_paint_layer.h"
#include "kis_clone_layer.h"
#include "KisDocument.h"
#include "kis_shape_controller.h"
#include "KisPart.h"
#include "kis_layer_utils.h"
#include "kis_generator_registry.h"
#include "KisGlobalResourcesInterface.h"
#include "kis_filter_configuration.h"
#include "kis_generator_layer.h"
#include "kis_selection.h"
#include "kis_node_insertion_adapter.h"
#include "kis_dummies_facade_base.h"
#include "kis_node_dummies_graph.h"
#include "KisImportExportManager.h"
#include "KisImageBarrierLockerWithFeedback.h"

#include <KoProperties.h>
#include <KoStore.h>
#include <KoColorProfile.h>
#include <KoColorSpaceRegistry.h>

#include <QApplication>
#include <QImage>
#include <QByteArray>
#include <QBuffer>
#include <QDomDocument>
#include <QDomElement>
#include <QDesktopWidget>
#include <QDir>

namespace {
KisNodeSP safeCopyNode(KisNodeSP node, bool detachClones = true) {
    KisCloneLayerSP cloneLayer = dynamic_cast<KisCloneLayer*>(node.data());
    return cloneLayer && detachClones ?
        KisNodeSP(cloneLayer->reincarnateAsPaintLayer()) : node->clone();
}
}


KisMimeData::KisMimeData(QList<KisNodeSP> nodes, KisImageSP image, bool forceCopy)
    : QMimeData()
    , m_nodes(nodes)
    , m_forceCopy(forceCopy)
    , m_image(image)
{
    Q_ASSERT(m_nodes.size() > 0);
}

void KisMimeData::deepCopyNodes()
{
    KisNodeList newNodes;

    {
        KisImageBarrierLockerWithFeedbackAllowNull locker(m_image);
        Q_FOREACH (KisNodeSP node, m_nodes) {
            KisNodeSP newNode = safeCopyNode(node);
            newNode->setImage(nullptr);

            newNodes << newNode;
        }
    }

    m_nodes = newNodes;
    m_image = 0;
}

QList<KisNodeSP> KisMimeData::nodes() const
{
    return m_nodes;
}

QStringList KisMimeData::formats () const
{
    QStringList f = QMimeData::formats();
    if (m_nodes.size() > 0) {
        f << "application/x-qt-image"
          << "application/zip"
          << "application/x-krita-node-internal-pointer";
    }
    return f;
}

KisDocument *createDocument(QList<KisNodeSP> nodes, KisImageSP srcImage)
{
    KisDocument *doc = KisPart::instance()->createTemporaryDocument();
    QRect rc;
    Q_FOREACH (KisNodeSP node, nodes) {
        if (node) {
            rc |= node->exactBounds();
        }
    }

    QRect offset(0, 0, rc.width(), rc.height());
    rc |= offset;

    if (rc.isEmpty() && srcImage) {
        rc = srcImage->bounds();
    }

    KisImageSP image = new KisImage(0, rc.width(), rc.height(), nodes.first()->colorSpace(), nodes.first()->name());
    image->setAllowMasksOnRootNode(true);

    if (srcImage) {
        image->setResolution(srcImage->xRes(), srcImage->yRes());
    }

    {
        KisImageBarrierLockerWithFeedbackAllowNull locker(srcImage);
        Q_FOREACH (KisNodeSP node, nodes) {
            KisNodeSP clonedNode = safeCopyNode(node);
            /// HACK ALERT: here we just initilize parent image link
            ///             and skip initilizing shapeController!
            ///             Ideally, we should call initializeExternalNode()
            ///             instead.
            image->addNode(clonedNode);
            KIS_SAFE_ASSERT_RECOVER(clonedNode->image() == KisImageWSP(image)) {
                clonedNode->setImage(image);
            }
        }
    }

    doc->setCurrentImage(image);

    return doc;
}

QByteArray serializeToByteArray(QList<KisNodeSP> nodes, KisImageSP srcImage)
{
    QScopedPointer<KisDocument> doc(createDocument(nodes, srcImage));
    QByteArray result = doc->serializeToNativeByteArray();

    // avoid a sanity check failure caused by the fact that the image outlives
    // the document (and it does)
    doc->setCurrentImage(0);

    return result;
}

QVariant KisMimeData::retrieveData(const QString &mimetype, QVariant::Type preferredType) const
{
    /**
     * HACK ALERT:
     *
     * Sometimes Qt requests the data *after* destruction of Krita,
     * we cannot load the nodes in that case, because we need signals
     * and timers. So we just skip serializing.
     */
    if (!QApplication::instance()) return QVariant();


    Q_ASSERT(m_nodes.size() > 0);

    if (mimetype == "application/x-qt-image") {
        KisConfig cfg(true);

        QScopedPointer<KisDocument> doc(createDocument(m_nodes, m_image));

        return doc->image()->projection()->convertToQImage(cfg.displayProfile(QApplication::desktop()->screenNumber(qApp->activeWindow())),
                                                           KoColorConversionTransformation::internalRenderingIntent(),
                                                           KoColorConversionTransformation::internalConversionFlags());
    }
    else if (mimetype == "application/zip") {

        QByteArray ba = serializeToByteArray(m_nodes, m_image);
        return ba;

    }
    else if (mimetype == "application/x-krita-node-internal-pointer") {

        QDomDocument doc("krita_internal_node_pointer");
        QDomElement root = doc.createElement("pointer");
        root.setAttribute("application_pid", (qint64)QApplication::applicationPid());
        root.setAttribute("force_copy", m_forceCopy);
        root.setAttribute("image_pointer_value", (qint64)m_image.data());
        doc.appendChild(root);

        Q_FOREACH (KisNodeSP node, m_nodes) {
            QDomElement element = doc.createElement("node");
            element.setAttribute("pointer_value", (qint64)node.data());
            root.appendChild(element);
        }

        return doc.toByteArray();

    }
    else {
        return QMimeData::retrieveData(mimetype, preferredType);
    }
}

void KisMimeData::initializeExternalNode(KisNodeSP *node,
                                         KisImageSP srcImage,
                                         KisImageSP dstImage,
                                         KisShapeController *shapeController)
{
    KisShapeLayer *shapeLayer = dynamic_cast<KisShapeLayer*>(node->data());
    if (shapeLayer) {
        // attach the layer to a new shape controller
        KisShapeLayer *shapeLayer2 = new KisShapeLayer(*shapeLayer, shapeController);

        if (srcImage
            && (!qFuzzyCompare(dstImage->xRes(), srcImage->xRes())
                || !qFuzzyCompare(dstImage->yRes(), srcImage->yRes()))) {

            const QTransform t = QTransform::fromScale(srcImage->xRes() / dstImage->xRes(),
                                                       srcImage->yRes() / dstImage->yRes());

            shapeLayer2->setTransformation(shapeLayer2->transformation() * t);
        }

        *node = shapeLayer2;
    }
}

QList<KisNodeSP> KisMimeData::tryLoadInternalNodes(const QMimeData *data,
                                                   KisImageSP image,
                                                   KisShapeController *shapeController,
                                                   bool /* IN-OUT */ &copyNode)
{
    QList<KisNodeSP> nodes;
    bool forceCopy = false;
    KisImageSP sourceImage;

    // Qt 4.7 and Qt 5.5 way
    const KisMimeData *mimedata = qobject_cast<const KisMimeData*>(data);
    if (mimedata) {
        nodes = mimedata->nodes();
        forceCopy = mimedata->m_forceCopy;
        sourceImage = mimedata->m_image;
    }

    // Qt 4.8 way
    if (nodes.isEmpty() && data->hasFormat("application/x-krita-node-internal-pointer")) {
        QByteArray nodeXml = data->data("application/x-krita-node-internal-pointer");

        QDomDocument doc;
        doc.setContent(nodeXml);

        QDomElement element = doc.documentElement();
        qint64 pid = element.attribute("application_pid").toLongLong();
        forceCopy = element.attribute("force_copy").toInt();
        qint64 imagePointerValue = element.attribute("image_pointer_value").toLongLong();
        sourceImage = reinterpret_cast<KisImage*>(imagePointerValue);

        if (pid == QApplication::applicationPid()) {

            QDomNode n = element.firstChild();
            while (!n.isNull()) {
                QDomElement e = n.toElement();
                if (!e.isNull()) {
                    qint64 pointerValue = e.attribute("pointer_value").toLongLong();
                    if (pointerValue) {
                        nodes << reinterpret_cast<KisNode*>(pointerValue);
                    }
                }
                n = n.nextSibling();
            }
        }
    }

    if (!nodes.isEmpty() && (forceCopy || copyNode || sourceImage != image)) {
        KisImageBarrierLockerWithFeedbackAllowNull locker(sourceImage);

        QList<KisNodeSP> clones;
        Q_FOREACH (KisNodeSP node, nodes) {
            node = safeCopyNode(node, sourceImage != image);
            if ((forceCopy || copyNode) && sourceImage == image) {
                KisLayerUtils::addCopyOfNameTag(node);
            }
            initializeExternalNode(&node, sourceImage, image, shapeController);
            clones << node;
        }
        nodes = clones;
        copyNode = true;
    }
    return nodes;
}

QList<KisNodeSP> KisMimeData::loadNonNativeNodes(const QMimeData *data,
                                                 KisImageWSP image)
{
    bool doRecenter = false;
    QList<KisNodeSP> nodes;

    if (nodes.isEmpty() && (data->hasFormat("application/x-color") || data->hasFormat("krita/x-colorsetentry"))) {
        QColor color = data->hasColor() ? qvariant_cast<QColor>(data->colorData()) : QColor(255, 0, 255);
        if (!data->hasColor() && data->hasFormat("krita/x-colorsetentry")) {
            QByteArray byteData = data->data("krita/x-colorsetentry");
            KisSwatch s = KisSwatch::fromByteArray(byteData);
            color = s.color().toQColor();
        }
        KisGeneratorSP generator = KisGeneratorRegistry::instance()->value("color");
        KisFilterConfigurationSP defaultConfig = generator->factoryConfiguration(KisGlobalResourcesInterface::instance());
        defaultConfig->setProperty("color", color);
        defaultConfig->createLocalResourcesSnapshot(KisGlobalResourcesInterface::instance());

        if (image) {
            KisGeneratorLayerSP fillLayer = new KisGeneratorLayer(image, image->nextLayerName(i18n("Fill Layer")),
                                                                  defaultConfig, image->globalSelection());
            nodes << fillLayer;
        }
    }

    if (nodes.isEmpty() && data->hasImage()) {
        QImage qimage = qvariant_cast<QImage>(data->imageData());

        KisPaintDeviceSP device = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());
        device->convertFromQImage(qimage, 0);

        if (image) {
            nodes << new KisPaintLayer(image.data(), image->nextLayerName(), OPACITY_OPAQUE_U8, device);
        }

        doRecenter = true;
    }

    if (!nodes.isEmpty() && doRecenter) {
        const QRect imageBounds = image->bounds();

        Q_FOREACH (KisNodeSP node, nodes) {
            const QRect layerBounds = node->projection()->exactBounds();
            if (doRecenter) {
                QPoint pt = imageBounds.center() - layerBounds.center();
                node->setX(pt.x());
                node->setY(pt.y());
            }
        }
    }

    return nodes;
}

QMimeData* KisMimeData::mimeForLayers(const KisNodeList &nodes, KisImageSP image, bool forceCopy)
{
    KisNodeList inputNodes = nodes;
    KisNodeList sortedNodes;
    KisLayerUtils::sortMergableNodes(image->root(), inputNodes, sortedNodes);
    if (sortedNodes.isEmpty()) return 0;

    KisMimeData* data = new KisMimeData(sortedNodes, image, forceCopy);
    return data;
}

QMimeData* KisMimeData::mimeForLayersDeepCopy(const KisNodeList &nodes, KisImageSP image, bool forceCopy)
{
    KisNodeList inputNodes = nodes;
    KisNodeList sortedNodes;
    KisLayerUtils::sortMergableNodes(image->root(), inputNodes, sortedNodes);
    if (sortedNodes.isEmpty()) return 0;

    KisMimeData* data = new KisMimeData(sortedNodes, image, forceCopy);
    data->deepCopyNodes();
    return data;
}

bool nodeAllowsAsChild(KisNodeSP parent, KisNodeList nodes)
{
    bool result = true;
    Q_FOREACH (KisNodeSP node, nodes) {
        if (!parent->allowAsChild(node) || !parent->isEditable(false)) {
            result = false;
            break;
        }
    }
    return result;
}

bool correctNewNodeLocation(KisNodeList nodes,
                            KisNodeDummy* &parentDummy,
                            KisNodeDummy* &aboveThisDummy)
{
    KisNodeSP parentNode = parentDummy->node();
    bool result = true;

    if(!nodeAllowsAsChild(parentDummy->node(), nodes) ||
            (parentDummy->node()->inherits("KisGroupLayer") && parentDummy->node()->collapsed())) {
        aboveThisDummy = parentDummy;
        parentDummy = parentDummy->parent();

        result = (!parentDummy) ? false :
            correctNewNodeLocation(nodes, parentDummy, aboveThisDummy);
    }

    return result;
}

KisNodeList KisMimeData::loadNodesFast(
    const QMimeData *data,
    KisImageSP image,
    KisShapeController *shapeController,
    bool &copyNode)
{
    QList<KisNodeSP> nodes =
        KisMimeData::tryLoadInternalNodes(data,
                                          image,
                                          shapeController,
                                          copyNode /* IN-OUT */);

    if (nodes.isEmpty()) {
        nodes = KisMimeData::loadNonNativeNodes(data, image);
        /**
         * Don't try to move a node originating from another image,
         * just copy it.
         */
        copyNode = true;
    }

    return nodes;
}

KisNodeList KisMimeData::loadNodesFastAndRecenter(const QPoint &preferredCenter, const QMimeData *data, KisImageSP image, KisShapeController *shapeController, bool &copyNode)
{
    KisNodeList nodes = loadNodesFast(data, image, shapeController, copyNode);

    Q_FOREACH (KisNodeSP node, nodes) {
        const QRect layerBounds = node->exactBounds();
        const QPoint offset = preferredCenter - layerBounds.center();

        node->setX(node->x() + offset.x());
        node->setY(node->y() + offset.y());
    }

    return nodes;
}

bool KisMimeData::insertMimeLayers(const QMimeData *data,
                                   KisImageSP image,
                                   KisShapeController *shapeController,
                                   KisNodeDummy *parentDummy,
                                   KisNodeDummy *aboveThisDummy,
                                   bool copyNode,
                                   KisNodeInsertionAdapter *nodeInsertionAdapter,
                                   bool changeOffset,
                                   QPointF offset)
{
    QList<KisNodeSP> nodes = loadNodesFast(data, image, shapeController, copyNode /* IN-OUT */);

    if (changeOffset) {
        Q_FOREACH (KisNodeSP node, nodes) {
            KisLayerUtils::recursiveApplyNodes(node, [offset] (KisNodeSP node){
                if (node->hasEditablePaintDevice()) {
                    KisPaintDeviceSP dev = node->paintDevice();
                    dev->moveTo(offset.x(), offset.y());
                }
            });
        }
    }

    if (nodes.isEmpty()) return false;

    bool result = true;

    if (!correctNewNodeLocation(nodes, parentDummy, aboveThisDummy)) {
        return false;
    }

    KIS_ASSERT_RECOVER(nodeInsertionAdapter) { return false; }

    Q_ASSERT(parentDummy);
    KisNodeSP aboveThisNode = aboveThisDummy ? aboveThisDummy->node() : 0;
    if (copyNode) {
        nodeInsertionAdapter->addNodes(nodes, parentDummy->node(), aboveThisNode);
    }
    else {
        Q_ASSERT(nodes.first()->graphListener() == image.data());
        nodeInsertionAdapter->moveNodes(nodes, parentDummy->node(), aboveThisNode);
    }

    const bool hasDelayedNodes =
        std::find_if(nodes.begin(), nodes.end(),
                     [] (KisNodeSP node) {
                         return bool(dynamic_cast<KisDelayedUpdateNodeInterface*>(node.data()));
                     }) != nodes.end();

    if (hasDelayedNodes) {
        /**
         * We have the node juggler running, so it will delay the update of the
         * generator layers that might be included into the paste. To avoid
         * that we should forcefully to make it stop
         */
        image->requestStrokeEnd();
    }

    return result;
}
