/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SLIDER_BASED_PAINTOP_PROPERTY_H
#define __KIS_SLIDER_BASED_PAINTOP_PROPERTY_H

#include "kis_uniform_paintop_property.h"


/**
 * This is a general class for the properties that can be represented
 * in the GUI as an integer or double slider. The GUI representation
 * creates a slider and connects it to this property using all the
 * information contained in it.
 *
 * Methods of this property basically copy the methods of
 * Kis{,Double}SliderSpinbox
 */

template<typename T>
class KRITAIMAGE_EXPORT_TEMPLATE KisSliderBasedPaintOpProperty
    : public KisUniformPaintOpProperty
{
public:
    KisSliderBasedPaintOpProperty(Type type, SubType subType, const KoID &id, KisPaintOpSettingsRestrictedSP settings, QObject *parent);

    KisSliderBasedPaintOpProperty(Type type, const KoID &id, KisPaintOpSettingsRestrictedSP settings, QObject *parent);

    KisSliderBasedPaintOpProperty(const KoID &id, KisPaintOpSettingsRestrictedSP settings, QObject *parent);

    T min() const;
    T max() const;
    void setRange(T min, T max);

    T singleStep() const;
    void setSingleStep(T value);
    T pageStep() const;
    void setPageStep(T value);

    qreal exponentRatio() const;
    void setExponentRatio(qreal value);
    int decimals() const;
    void setDecimals(int value);

    QString suffix() const;
    void setSuffix(QString value);

private:
    T m_min;
    T m_max;

    T m_singleStep;
    T m_pageStep;
    qreal m_exponentRatio;

    int m_decimals;
    QString m_suffix;
};

#include "kis_callback_based_paintop_property.h"

extern template class KisSliderBasedPaintOpProperty<int>;
extern template class KisSliderBasedPaintOpProperty<qreal>;
extern template class KisCallbackBasedPaintopProperty<KisSliderBasedPaintOpProperty<int>>;
extern template class KisCallbackBasedPaintopProperty<KisSliderBasedPaintOpProperty<qreal>>;

using KisIntSliderBasedPaintOpProperty = KisSliderBasedPaintOpProperty<int>;
using KisDoubleSliderBasedPaintOpProperty =
    KisSliderBasedPaintOpProperty<qreal>;

using KisIntSliderBasedPaintOpPropertyCallback =
    KisCallbackBasedPaintopProperty<KisSliderBasedPaintOpProperty<int>>;
using KisDoubleSliderBasedPaintOpPropertyCallback =
    KisCallbackBasedPaintopProperty<KisSliderBasedPaintOpProperty<qreal>>;

#endif /* __KIS_SLIDER_BASED_PAINTOP_PROPERTY_H */
