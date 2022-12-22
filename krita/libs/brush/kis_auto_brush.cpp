/*
 *  SPDX-FileCopyrightText: 2004, 2007-2009 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2012 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_auto_brush.h"

#include <kis_debug.h>
#include <math.h>

#include <QPainterPath>
#include <QRect>
#include <QDomElement>
#include <QtConcurrentMap>
#include <QByteArray>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>

#include <KoColor.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>

#include <kis_datamanager.h>
#include <kis_fixed_paint_device.h>
#include <kis_paint_device.h>
#include <brushengine/kis_paint_information.h>
#include <kis_mask_generator.h>
#include <kis_boundary.h>
#include <brushengine/kis_paintop_lod_limitations.h>
#include <kis_brush_mask_applicator_base.h>
#include "kis_algebra_2d.h"
#include <KisOptimizedBrushOutline.h>

#if defined(_WIN32) || defined(_WIN64)
#include <stdlib.h>
#define srand48 srand
inline double drand48()
{
    return double(rand()) / RAND_MAX;
}
#endif

struct KisAutoBrush::Private {
    Private()
        : randomness(0)
        , density(1.0)
        , idealThreadCountCached(1)
    {}

    Private(const Private &rhs)
        : shape(rhs.shape->clone())
        , randomness(rhs.randomness)
        , density(rhs.density)
        , idealThreadCountCached(rhs.idealThreadCountCached)
    {
    }


    QScopedPointer<KisMaskGenerator> shape;
    qreal randomness;
    qreal density;
    int idealThreadCountCached;
};

KisAutoBrush::KisAutoBrush(KisMaskGenerator* as, qreal angle, qreal randomness, qreal density)
    : KisBrush(),
      d(new Private)
{
    d->shape.reset(as);
    d->randomness = randomness;
    d->density = density;
    d->idealThreadCountCached = QThread::idealThreadCount();
    setBrushType(MASK);

    {
        /**
         * Here is a two-stage process of initialization of the brush size.
         * It is done so for the backward compatibility reasons when the size
         * was set to the size of brushTipImage(), which initialization is now
         * skipped for efficiency reasons.
         */

        setWidth(qMax(qreal(1.0), d->shape->width()));
        setHeight(qMax(qreal(1.0), d->shape->height()));

        const int width = maskWidth(KisDabShape(), 0.0, 0.0, KisPaintInformation());
        const int height = maskHeight(KisDabShape(), 0.0, 0.0, KisPaintInformation());

        setWidth(qMax(1, width));
        setHeight(qMax(1, height));
    }

    // We don't initialize setBrushTipImage(), bacause
    // auto brush doesn't use image pyramid. And generation
    // of a full-scaled QImage may cause a significant delay
    // in the beginning of the stroke

    setAngle(angle);
    setImage(createBrushPreview(128));
}

KisAutoBrush::~KisAutoBrush()
{
}

bool KisAutoBrush::isEphemeral() const
{
    return true;
}

bool KisAutoBrush::loadFromDevice(QIODevice *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(dev);
    Q_UNUSED(resourcesInterface);
    return false;
}

bool KisAutoBrush::saveToDevice(QIODevice *dev) const
{
    Q_UNUSED(dev);
    return false;
}

bool KisAutoBrush::isPiercedApprox() const
{
    bool result = false;

    if (d->shape->id() == SoftId.id()) {
        result = d->shape->valueAt(0,0) > 0.05 * 255;
    }

    return result;
}

KisFixedPaintDeviceSP KisAutoBrush::outlineSourceImage() const
{
    KisFixedPaintDeviceSP dev;
    KisDabShape inverseTransform(1.0 / scale(), 1.0, -angle());

    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    dev = new KisFixedPaintDevice(cs);
    mask(dev, KoColor(Qt::black, cs), inverseTransform, KisPaintInformation());

    return dev;
}

qreal KisAutoBrush::userEffectiveSize() const
{
    return d->shape->diameter();
}

void KisAutoBrush::setUserEffectiveSize(qreal value)
{
    d->shape->setDiameter(value);
}

KisAutoBrush::KisAutoBrush(const KisAutoBrush& rhs)
    : KisBrush(rhs)
    , d(new Private(*rhs.d))
{
}

KoResourceSP KisAutoBrush::clone() const
{
    return KoResourceSP(new KisAutoBrush(*this));
}

/* It's difficult to predict the mask height exactly when there are
 * more than 2 spikes, so we return an upperbound instead. */
static KisDabShape lieAboutDabShape(KisDabShape const& shape, int spikes)
{
    return spikes > 2 ? KisDabShape(shape.scale(), 1.0, shape.rotation()) : shape;
}

qint32 KisAutoBrush::maskHeight(KisDabShape const& shape,
    qreal subPixelX, qreal subPixelY, const KisPaintInformation& info) const
{
    return KisBrush::maskHeight(
        lieAboutDabShape(shape, maskGenerator()->spikes()), subPixelX, subPixelY, info);
}

qint32 KisAutoBrush::maskWidth(KisDabShape const& shape,
    qreal subPixelX, qreal subPixelY, const KisPaintInformation& info) const
{
    return KisBrush::maskWidth(
        lieAboutDabShape(shape, maskGenerator()->spikes()), subPixelX, subPixelY, info);
}

QSizeF KisAutoBrush::characteristicSize(KisDabShape const& shape) const
{
    return KisBrush::characteristicSize(lieAboutDabShape(shape, maskGenerator()->spikes()));
}


inline void fillPixelOptimized_4bytes(quint8 *color, quint8 *buf, int size)
{
    /**
     * This version of filling uses low granularity of data transfers
     * (32-bit chunks) and internal processor's parallelism. It reaches
     * 25% better performance in KisStrokeBenchmark in comparison to
     * per-pixel memcpy version (tested on Sandy Bridge).
     */

    int block1 = size / 8;
    int block2 = size % 8;

    quint32 *src = reinterpret_cast<quint32*>(color);
    quint32 *dst = reinterpret_cast<quint32*>(buf);

    // check whether all buffers are 4 bytes aligned
    // (uncomment if experience some problems)
    // Q_ASSERT(((qint64)src & 3) == 0);
    // Q_ASSERT(((qint64)dst & 3) == 0);

    for (int i = 0; i < block1; i++) {
        *dst = *src;
        *(dst + 1) = *src;
        *(dst + 2) = *src;
        *(dst + 3) = *src;
        *(dst + 4) = *src;
        *(dst + 5) = *src;
        *(dst + 6) = *src;
        *(dst + 7) = *src;

        dst += 8;
    }

    for (int i = 0; i < block2; i++) {
        *dst = *src;
        dst++;
    }
}

inline void fillPixelOptimized_general(quint8 *color, quint8 *buf, int size, int pixelSize)
{
    /**
     * This version uses internal processor's parallelism and gives
     * 20% better performance in KisStrokeBenchmark in comparison to
     * per-pixel memcpy version (tested on Sandy Bridge (+20%) and
     * on Merom (+10%)).
     */

    int block1 = size / 8;
    int block2 = size % 8;

    for (int i = 0; i < block1; i++) {
        quint8 *d1 = buf;
        quint8 *d2 = buf + pixelSize;
        quint8 *d3 = buf + 2 * pixelSize;
        quint8 *d4 = buf + 3 * pixelSize;
        quint8 *d5 = buf + 4 * pixelSize;
        quint8 *d6 = buf + 5 * pixelSize;
        quint8 *d7 = buf + 6 * pixelSize;
        quint8 *d8 = buf + 7 * pixelSize;

        for (int j = 0; j < pixelSize; j++) {
            *(d1 + j) = color[j];
            *(d2 + j) = color[j];
            *(d3 + j) = color[j];
            *(d4 + j) = color[j];
            *(d5 + j) = color[j];
            *(d6 + j) = color[j];
            *(d7 + j) = color[j];
            *(d8 + j) = color[j];
        }

        buf += 8 * pixelSize;
    }

    for (int i = 0; i < block2; i++) {
        memcpy(buf, color, pixelSize);
        buf += pixelSize;
    }
}

void KisAutoBrush::generateMaskAndApplyMaskOrCreateDab(KisFixedPaintDeviceSP dst,
        KisBrush::ColoringInformation* coloringInformation,
        KisDabShape const& shape,
        const KisPaintInformation& info,
        double subPixelX , double subPixelY, qreal softnessFactor, qreal lightnessStrength) const
{
    Q_UNUSED(info);
    Q_UNUSED(lightnessStrength);

    // Generate the paint device from the mask
    const KoColorSpace* cs = dst->colorSpace();
    quint32 pixelSize = cs->pixelSize();

    // mask dimension methods already includes KisBrush::angle()
    int dstWidth = maskWidth(shape, subPixelX, subPixelY, info);
    int dstHeight = maskHeight(shape, subPixelX, subPixelY, info);
    QPointF hotSpot = this->hotSpot(shape, info);

    // mask size and hotSpot function take the KisBrush rotation into account
    qreal angle = shape.rotation() + KisBrush::angle();

    // if there's coloring information, we merely change the alpha: in that case,
    // the dab should be big enough!
    if (coloringInformation) {
        // new bounds. we don't care if there is some extra memory occcupied.
        dst->setRect(QRect(0, 0, dstWidth, dstHeight));
        dst->lazyGrowBufferWithoutInitialization();
    }
    else {
        KIS_SAFE_ASSERT_RECOVER_RETURN(dst->bounds().width() >= dstWidth &&
                                       dst->bounds().height() >= dstHeight);
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN(coloringInformation);

    quint8* dabPointer = dst->data();

    quint8* color = 0;
    if (dynamic_cast<PlainColoringInformation*>(coloringInformation)) {
        color = const_cast<quint8*>(coloringInformation->color());
    }

    double centerX = hotSpot.x() - 0.5 + subPixelX;
    double centerY = hotSpot.y() - 0.5 + subPixelY;

    d->shape->setSoftness(softnessFactor); // softness must be set first
    d->shape->setScale(shape.scaleX(), shape.scaleY());

    if (!color) {
        for (int y = 0; y < dstHeight; y++) {
            for (int x = 0; x < dstWidth; x++) {
                memcpy(dabPointer, coloringInformation->color(), pixelSize);
                coloringInformation->nextColumn();
                dabPointer += pixelSize;
            }
            coloringInformation->nextRow();
        }
    }

    MaskProcessingData data(dst, cs, color,
                            d->randomness, d->density,
                            centerX, centerY,
                            angle);

    const QRect rect(0, 0, dstWidth, dstHeight);
    KisBrushMaskApplicatorBase *applicator = d->shape->applicator();
    applicator->initializeData(&data);
    applicator->process(rect);
}

void KisAutoBrush::notifyBrushIsGoingToBeClonedForStroke()
{
    // do nothing, since we don't use the pyramid!
}

void KisAutoBrush::coldInitBrush()
{
    generateOutlineCache();
}

void KisAutoBrush::toXML(QDomDocument& doc, QDomElement& e) const
{
    QDomElement shapeElt = doc.createElement("MaskGenerator");
    d->shape->toXML(doc, shapeElt);
    e.appendChild(shapeElt);
    e.setAttribute("type", "auto_brush");
    e.setAttribute("spacing", QString::number(spacing()));
    e.setAttribute("useAutoSpacing", QString::number(autoSpacingActive()));
    e.setAttribute("autoSpacingCoeff", QString::number(autoSpacingCoeff()));
    e.setAttribute("angle", QString::number(KisBrush::angle()));
    e.setAttribute("randomness", QString::number(d->randomness));
    e.setAttribute("density", QString::number(d->density));
    KisBrush::toXML(doc, e);
}

QImage KisAutoBrush::createBrushPreview(int maxSize)
{
    KisDabShape shape;

    int width = maskWidth(KisDabShape(), 0.0, 0.0, KisPaintInformation());
    int height = maskHeight(KisDabShape(), 0.0, 0.0, KisPaintInformation());

    QSize size(width, height);

    if (maxSize > 0 && KisAlgebra2D::maxDimension(size) > maxSize) {
        size.scale(128, 128, Qt::KeepAspectRatio);

        qreal scale = 1.0;

        if (width > height) {
            scale = qreal(size.width()) / width;
        } else {
            scale = qreal(size.height()) / height;
        }

        shape = KisDabShape(scale, 1.0, 0.0);
        width = maskWidth(shape, 0.0, 0.0, KisPaintInformation());
        height = maskHeight(shape, 0.0, 0.0, KisPaintInformation());
    }

    KisPaintInformation info(QPointF(width * 0.5, height * 0.5), 0.5, 0, 0, angle(), 0, 0, 0, 0);

    KisFixedPaintDeviceSP fdev = new KisFixedPaintDevice(KoColorSpaceRegistry::instance()->rgb8());
    fdev->setRect(QRect(0, 0, width, height));
    fdev->initialize();

    mask(fdev, KoColor(Qt::black, fdev->colorSpace()), shape, info);
    return fdev->convertToQImage(0);
}


const KisMaskGenerator* KisAutoBrush::maskGenerator() const
{
    return d->shape.data();
}

qreal KisAutoBrush::density() const
{
    return d->density;
}

qreal KisAutoBrush::randomness() const
{
    return d->randomness;
}

KisOptimizedBrushOutline KisAutoBrush::outline(bool forcePreciseOutline) const
{
    const bool requiresComplexOutline = d->shape->spikes() > 2;
    if (!requiresComplexOutline && !forcePreciseOutline) {
        QPainterPath path;
        QRectF brushBoundingbox(0, 0, width(), height());
        if (maskGenerator()->type() == KisMaskGenerator::CIRCLE) {
            path.addEllipse(brushBoundingbox);
        }
        else { // if (maskGenerator()->type() == KisMaskGenerator::RECTANGLE)
            path.addRect(brushBoundingbox);
        }

        return path;
    }

    return KisBrush::outline();
}

void KisAutoBrush::lodLimitations(KisPaintopLodLimitations *l) const
{
    KisBrush::lodLimitations(l);

    if (!qFuzzyCompare(density(), 1.0)) {
        l->limitations << KoID("auto-brush-density", i18nc("PaintOp instant preview limitation", "Brush Density recommended value 100.0"));
    }

    if (!qFuzzyCompare(randomness(), 0.0)) {
        l->limitations << KoID("auto-brush-randomness", i18nc("PaintOp instant preview limitation", "Brush Randomness recommended value 0.0"));
    }
}

bool KisAutoBrush::supportsCaching() const
{
    return qFuzzyCompare(density(), 1.0) && qFuzzyCompare(randomness(), 0.0);
}
