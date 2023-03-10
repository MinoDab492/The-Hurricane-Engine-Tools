/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KIS_PRESSURE_SHARPNESS_OPTION_WIDGET_H
#define KIS_PRESSURE_SHARPNESS_OPTION_WIDGET_H

#include "kis_curve_option_widget.h"

class KisSliderSpinBox;
class QCheckBox;

class PAINTOP_EXPORT KisPressureSharpnessOptionWidget : public KisCurveOptionWidget
{
    Q_OBJECT

public:
    KisPressureSharpnessOptionWidget();

    void readOptionSetting(const KisPropertiesConfigurationSP setting) override;

private Q_SLOTS:
    void setAlignOutlineToPixels(bool alignOutlineToPixels);
    void setThreshold(int threshold);

private:
    QCheckBox* m_alignOutline;
    KisSliderSpinBox* m_softenEdge;
};

#endif // KIS_PRESSURE_SHARPNESS_OPTION_WIDGET_H
