/*
 *  SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_AUTOBRUSH_RESOURCE_H_
#define _KIS_AUTOBRUSH_RESOURCE_H_

#include "kritabrush_export.h"

#include <KoResource.h>

#include "kis_brush.h"

#include <QScopedPointer>

class KisMaskGenerator;

/**
 * XXX: docs!
 */
class BRUSH_EXPORT KisAutoBrush : public KisBrush
{

public:

    KisAutoBrush(KisMaskGenerator *as, qreal angle, qreal randomness, qreal density = 1.0);
    KisAutoBrush(const KisAutoBrush &rhs);
    KisAutoBrush &operator=(const KisAutoBrush &rhs) = delete;
    KoResourceSP clone() const override;

    ~KisAutoBrush() override;

    bool isEphemeral() const override;
    bool loadFromDevice(QIODevice *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(QIODevice *dev) const override;

    bool isPiercedApprox() const override;

public:

    qreal userEffectiveSize() const override;
    void setUserEffectiveSize(qreal value) override;

    qint32 maskWidth(KisDabShape const& shape, qreal subPixelX, qreal subPixelY,
        const KisPaintInformation& info) const override;
    qint32 maskHeight(KisDabShape const& shape, qreal subPixelX, qreal subPixelY,
        const KisPaintInformation& info) const override;
    QSizeF characteristicSize(KisDabShape const&) const override;

    KisFixedPaintDeviceSP paintDevice(const KoColorSpace*,
            KisDabShape const&,
            const KisPaintInformation&,
            double = 0, double = 0) const override {
        return 0; // The autobrush does NOT support images!
    }

    void generateMaskAndApplyMaskOrCreateDab(KisFixedPaintDeviceSP dst,
        KisBrush::ColoringInformation* src,
        KisDabShape const&,
        const KisPaintInformation& info,
        double subPixelX = 0, double subPixelY = 0,
        qreal softnessFactor = DEFAULT_SOFTNESS_FACTOR,
        qreal lightnessStrength = DEFAULT_LIGHTNESS_STRENGTH) const override;

    KisOptimizedBrushOutline outline(bool forcePreciseOutline = false) const override;

    void notifyBrushIsGoingToBeClonedForStroke() override;

    void coldInitBrush() override;
    KisFixedPaintDeviceSP outlineSourceImage() const override;

public:

    void toXML(QDomDocument& , QDomElement&) const override;
    const KisMaskGenerator* maskGenerator() const;
    qreal randomness() const;
    qreal density() const;

    void lodLimitations(KisPaintopLodLimitations *l) const override;

    bool supportsCaching() const override;
private:

    QImage createBrushPreview(int maxSize = -1);

private:
    struct Private;
    const QScopedPointer<Private> d;
};
#endif // _KIS_AUTOBRUSH_RESOURCE_H_
