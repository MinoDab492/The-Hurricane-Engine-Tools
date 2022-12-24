/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2011 Sven Langkamp <sven.langkamp@gmail.com>
 *  SPDX-FileCopyrightText: 2015 Moritz Molch <kde@moritzmolch.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_color_input.h"

#include <KoConfig.h>
#ifdef HAVE_OPENEXR
#include <half.h>
#endif

#include <cmath>

#include <kis_debug.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include <klocalizedstring.h>

#include <KoChannelInfo.h>
#include <KoColor.h>
#include <KoColorSlider.h>
#include <KoColorSpace.h>
#include <KisHsvColorSlider.h>

#include "kis_double_parse_spin_box.h"
#include "kis_int_parse_spin_box.h"
#include "kis_signals_blocker.h"

KisColorInput::KisColorInput(QWidget* parent, const KoChannelInfo* channelInfo, KoColor* color, KoColorDisplayRendererInterface *displayRenderer, bool usePercentage) :
    QWidget(parent), m_channelInfo(channelInfo), m_color(color), m_displayRenderer(displayRenderer),
    m_usePercentage(usePercentage)
{
}

void KisColorInput::init()
{
    QHBoxLayout* m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0,0,0,0);
    m_layout->setSpacing(1);

    QLabel* m_label = new QLabel(i18n("%1:", m_channelInfo->name()), this);
    m_layout->addWidget(m_label);

    m_colorSlider = new KoColorSlider(Qt::Horizontal, this, m_displayRenderer);
    m_colorSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addWidget(m_colorSlider);

    QWidget* m_input = createInput();
    m_layout->addWidget(m_input);
}

KisIntegerColorInput::KisIntegerColorInput(QWidget* parent, const KoChannelInfo* channelInfo, KoColor* color, KoColorDisplayRendererInterface *displayRenderer, bool usePercentage) :
    KisColorInput(parent, channelInfo, color, displayRenderer, usePercentage)
{
    init();
}

void KisIntegerColorInput::setValue(int v)
{
    quint8* data = m_color->data() + m_channelInfo->pos();
    switch (m_channelInfo->channelValueType()) {
    case KoChannelInfo::UINT8:
        *(reinterpret_cast<quint8*>(data)) = v;
        break;
    case KoChannelInfo::UINT16:
        *(reinterpret_cast<quint16*>(data)) = v;
        break;
    case KoChannelInfo::UINT32:
        *(reinterpret_cast<quint32*>(data)) = v;
        break;
    default:
        Q_ASSERT(false);
    }
    emit(updated());
}

void KisIntegerColorInput::update()
{
    KoColor min = *m_color;
    KoColor max = *m_color;
    quint8* data = m_color->data() + m_channelInfo->pos();
    quint8* dataMin = min.data() + m_channelInfo->pos();
    quint8* dataMax = max.data() + m_channelInfo->pos();
    m_intNumInput->blockSignals(true);
    m_colorSlider->blockSignals(true);
    switch (m_channelInfo->channelValueType()) {
    case KoChannelInfo::UINT8:
        if (m_usePercentage) {
            m_intNumInput->setMaximum(100);
            m_intNumInput->setValue(round(*(reinterpret_cast<quint8*>(data))*1.0 / 255.0 * 100.0));
        } else {
            m_intNumInput->setMaximum(0xFF);
            m_intNumInput->setValue(*(reinterpret_cast<quint8*>(data)));
        }
        m_colorSlider->setValue(*(reinterpret_cast<quint8*>(data)));
        *(reinterpret_cast<quint8*>(dataMin)) = 0x0;
        *(reinterpret_cast<quint8*>(dataMax)) = 0xFF;
        break;
    case KoChannelInfo::UINT16:
        if (m_usePercentage) {
            m_intNumInput->setMaximum(100);
            m_intNumInput->setValue(round(*(reinterpret_cast<quint16*>(data))*1.0 / 65535.0 * 100.0));
        } else {
            m_intNumInput->setMaximum(0xFFFF);
            m_intNumInput->setValue(*(reinterpret_cast<quint16*>(data)));
        }
        m_colorSlider->setValue(*(reinterpret_cast<quint16*>(data)));
        *(reinterpret_cast<quint16*>(dataMin)) = 0x0;
        *(reinterpret_cast<quint16*>(dataMax)) = 0xFFFF;
        break;
    case KoChannelInfo::UINT32:
        if (m_usePercentage) {
            m_intNumInput->setMaximum(100);
            m_intNumInput->setValue(round(*(reinterpret_cast<quint32*>(data))*1.0 / 4294967295.0 * 100.0));
        } else {
            m_intNumInput->setMaximum(0xFFFF);
            m_intNumInput->setValue(*(reinterpret_cast<quint32*>(data)));
        }
        m_colorSlider->setValue(*(reinterpret_cast<quint32*>(data)));
        *(reinterpret_cast<quint32*>(dataMin)) = 0x0;
        *(reinterpret_cast<quint32*>(dataMax)) = 0xFFFFFFFF;
        break;
    default:
        Q_ASSERT(false);
    }
    m_colorSlider->setColors(min, max);
    m_intNumInput->blockSignals(false);
    m_colorSlider->blockSignals(false);
}

QWidget* KisIntegerColorInput::createInput()
{
    m_intNumInput = new KisIntParseSpinBox(this);
    m_intNumInput->setMinimum(0);
    m_colorSlider->setMinimum(0);

    if (m_usePercentage) {
        m_intNumInput->setSuffix(i18n("%"));
    } else {
        m_intNumInput->setSuffix("");
    }

    switch (m_channelInfo->channelValueType()) {
    case KoChannelInfo::UINT8:
        if (m_usePercentage) {
            m_intNumInput->setMaximum(100);
        } else {
            m_intNumInput->setMaximum(0xFF);
        }
        m_colorSlider->setMaximum(0xFF);
        break;
    case KoChannelInfo::UINT16:
        if (m_usePercentage) {
            m_intNumInput->setMaximum(100);
        } else {
            m_intNumInput->setMaximum(0xFFFF);
        }
        m_colorSlider->setMaximum(0xFFFF);
        break;
    case KoChannelInfo::UINT32:
        if (m_usePercentage) {
            m_intNumInput->setMaximum(100);
        } else {
            m_intNumInput->setMaximum(0xFFFFFFFF);
        }
        m_colorSlider->setMaximum(0xFFFFFFFF);
        break;
    default:
        Q_ASSERT(false);
    }
    connect(m_colorSlider, SIGNAL(valueChanged(int)), this, SLOT(onColorSliderChanged(int)));
    connect(m_intNumInput, SIGNAL(valueChanged(int)), this, SLOT(onNumInputChanged(int)));
    return m_intNumInput;
}

void KisIntegerColorInput::setPercentageWise(bool val)
{
    m_usePercentage = val;

    if (m_usePercentage) {
        m_intNumInput->setSuffix(i18n("%"));
    } else {
        m_intNumInput->setSuffix("");
    }
}

void KisIntegerColorInput::onColorSliderChanged(int val)
{
    m_intNumInput->blockSignals(true);
    if (m_usePercentage) {
        switch (m_channelInfo->channelValueType()) {
        case KoChannelInfo::UINT8:
            m_intNumInput->setValue(round((val*1.0) / 255.0 * 100.0));
            break;
        case KoChannelInfo::UINT16:
            m_intNumInput->setValue(round((val*1.0) / 65535.0 * 100.0));
            break;
        case KoChannelInfo::UINT32:
            m_intNumInput->setValue(round((val*1.0) / 4294967295.0 * 100.0));
            break;
        default:
            Q_ASSERT(false);
        }
    } else {
        m_intNumInput->setValue(val);
    }
    m_intNumInput->blockSignals(false);
    setValue(val);
}

void KisIntegerColorInput::onNumInputChanged(int val)
{
    m_colorSlider->blockSignals(true);
    if (m_usePercentage) {
        switch (m_channelInfo->channelValueType()) {
        case KoChannelInfo::UINT8:
            m_colorSlider->setValue((val*1.0)/100.0 * 255.0);
            m_colorSlider->blockSignals(false);
            setValue((val*1.0)/100.0 * 255.0);
            break;
        case KoChannelInfo::UINT16:
            m_colorSlider->setValue((val*1.0)/100.0 * 65535.0);
            m_colorSlider->blockSignals(false);
            setValue((val*1.0)/100.0 * 65535.0);
            break;
        case KoChannelInfo::UINT32:
            m_colorSlider->setValue((val*1.0)/100.0 * 4294967295.0);
            m_colorSlider->blockSignals(false);
            setValue((val*1.0)/100.0 * 4294967295.0);
            break;
        default:
            Q_ASSERT(false);
        }
    } else {
        m_colorSlider->setValue(val);
        m_colorSlider->blockSignals(false);
        setValue(val);
    }
}

KisFloatColorInput::KisFloatColorInput(QWidget* parent, const KoChannelInfo* channelInfo, KoColor* color, KoColorDisplayRendererInterface *displayRenderer, bool usePercentage) :
    KisColorInput(parent, channelInfo, color, displayRenderer, usePercentage)
{
    init();
}

void KisFloatColorInput::setValue(double v)
{
    quint8* data = m_color->data() + m_channelInfo->pos();
    switch (m_channelInfo->channelValueType()) {
#ifdef HAVE_OPENEXR
    case KoChannelInfo::FLOAT16:
        *(reinterpret_cast<half*>(data)) = v;
        break;
#endif
    case KoChannelInfo::FLOAT32:
        *(reinterpret_cast<float*>(data)) = v;
        break;
    default:
        Q_ASSERT(false);
    }
    emit(updated());
}

QWidget* KisFloatColorInput::createInput()
{
    m_dblNumInput = new KisDoubleParseSpinBox(this);
    m_dblNumInput->setMinimum(0);
    m_dblNumInput->setMaximum(1.0);
    connect(m_colorSlider, SIGNAL(valueChanged(int)), this, SLOT(sliderChanged(int)));
    connect(m_dblNumInput, SIGNAL(valueChanged(double)), this, SLOT(setValue(double)));
    m_dblNumInput->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_dblNumInput->setMinimumWidth(60);
    m_dblNumInput->setMaximumWidth(60);
    
    quint8* data = m_color->data() + m_channelInfo->pos();
    qreal value = 1.0;

    switch (m_channelInfo->channelValueType()) {
#ifdef HAVE_OPENEXR
    case KoChannelInfo::FLOAT16:
        value = *(reinterpret_cast<half*>(data));
        break;
#endif
    case KoChannelInfo::FLOAT32:
        value = *(reinterpret_cast<float*>(data));
        break;
    default:
        Q_ASSERT(false);
    }
    m_dblNumInput->setValue(value);

    return m_dblNumInput;
}

void KisFloatColorInput::sliderChanged(int i)
{
    const qreal floatRange = m_maxValue - m_minValue;
    m_dblNumInput->setValue(m_minValue + (i / 255.0) * floatRange);
}

void KisFloatColorInput::update()
{
    KoColor min = *m_color;
    KoColor max = *m_color;
    quint8* data = m_color->data() + m_channelInfo->pos();
    quint8* dataMin = min.data() + m_channelInfo->pos();
    quint8* dataMax = max.data() + m_channelInfo->pos();

    qreal value = 1.0;
    m_minValue = m_displayRenderer->minVisibleFloatValue(m_channelInfo);
    m_maxValue = m_displayRenderer->maxVisibleFloatValue(m_channelInfo);
    m_dblNumInput->blockSignals(true);
    m_colorSlider->blockSignals(true);

    switch (m_channelInfo->channelValueType()) {
#ifdef HAVE_OPENEXR
    case KoChannelInfo::FLOAT16:
        value = *(reinterpret_cast<half*>(data));
        m_minValue = qMin(value, m_minValue);
        m_maxValue = qMax(value, m_maxValue);
        *(reinterpret_cast<half*>(dataMin)) = m_minValue;
        *(reinterpret_cast<half*>(dataMax)) = m_maxValue;
        break;
#endif
    case KoChannelInfo::FLOAT32:
        value = *(reinterpret_cast<float*>(data));
        m_minValue = qMin(value, m_minValue);
        m_maxValue = qMax(value, m_maxValue);
        *(reinterpret_cast<float*>(dataMin)) = m_minValue;
        *(reinterpret_cast<float*>(dataMax)) = m_maxValue;
        break;
    default:
        Q_ASSERT(false);
    }

    m_dblNumInput->setMinimum(m_minValue);
    m_dblNumInput->setMaximum(m_maxValue);

    // ensure at least 3 significant digits are always shown
    int newPrecision = 2 + qMax(qreal(0.0), std::ceil(-std::log10(m_maxValue)));
    if (newPrecision != m_dblNumInput->decimals()) {
        m_dblNumInput->setDecimals(newPrecision);
        m_dblNumInput->updateGeometry();
    }
    m_dblNumInput->setValue(value);

    m_colorSlider->setColors(min, max);

    const qreal floatRange = m_maxValue - m_minValue;
    m_colorSlider->setValue((value - m_minValue) / floatRange * 255);
    m_dblNumInput->blockSignals(false);
    m_colorSlider->blockSignals(false);
}

KisHexColorInput::KisHexColorInput(QWidget* parent, KoColor* color, KoColorDisplayRendererInterface *displayRenderer, bool usePercentage, bool usePreview) :
    KisColorInput(parent, 0, color, displayRenderer, usePercentage)
{
    QHBoxLayout* m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0,0,0,0);
    m_layout->setSpacing(1);

    QLabel* m_label = new QLabel(i18n("Color name:"), this);
    m_label->setMinimumWidth(50);
    m_layout->addWidget(m_label);

    QWidget* m_input = createInput();
    m_input->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    if(usePreview) {
        m_colorPreview = new QLabel("");
        m_colorPreview->setMinimumWidth(30);
        m_layout->addWidget(m_colorPreview);
    }

    m_layout->addWidget(m_input);
}

void KisHexColorInput::setValue()
{
    QString valueString = m_hexInput->text();
    valueString.remove(QChar('#'));

    QList<KoChannelInfo*> channels = m_color->colorSpace()->channels();
    channels = KoChannelInfo::displayOrderSorted(channels);
    Q_FOREACH (KoChannelInfo* channel, channels) {
        if (channel->channelType() == KoChannelInfo::COLOR) {
            Q_ASSERT(channel->channelValueType() == KoChannelInfo::UINT8);
            quint8* data = m_color->data() + channel->pos();

            int value = valueString.left(2).toInt(0, 16);
            *(reinterpret_cast<quint8*>(data)) = value;
            valueString.remove(0, 2);
        }
    }
    emit(updated());
}

void KisHexColorInput::update()
{
    QString hexString("#");

    QList<KoChannelInfo*> channels = m_color->colorSpace()->channels();
    channels = KoChannelInfo::displayOrderSorted(channels);
    Q_FOREACH (KoChannelInfo* channel, channels) {
        if (channel->channelType() == KoChannelInfo::COLOR) {
            Q_ASSERT(channel->channelValueType() == KoChannelInfo::UINT8);
            quint8* data = m_color->data() + channel->pos();
            hexString.append(QString("%1").arg(*(reinterpret_cast<quint8*>(data)), 2, 16, QChar('0')));
        }
    }
    m_hexInput->setText(hexString);
    if( m_colorPreview) {
        m_colorPreview->setStyleSheet(QString("background-color: %1").arg(m_displayRenderer->toQColor(*m_color).name()));
    }
}

QWidget* KisHexColorInput::createInput()
{
    m_hexInput = new QLineEdit(this);
    m_hexInput->setAlignment(Qt::AlignRight);

    int digits = 2*m_color->colorSpace()->colorChannelCount();
    QString pattern = QString("#?[a-fA-F0-9]{%1,%2}").arg(digits).arg(digits);
    m_hexInput->setValidator(new QRegExpValidator(QRegExp(pattern), this));
    connect(m_hexInput, SIGNAL(editingFinished()), this, SLOT(setValue()));
    return m_hexInput;
}


KisHsvColorInput::KisHsvColorInput(QWidget *parent, KoColor *color)
    : QWidget(parent)
    , m_color(color)
    , m_hSlider(nullptr)
    , m_sSlider(nullptr)
    , m_vSlider(nullptr)
    , m_hInput(nullptr)
    , m_sInput(nullptr)
    , m_vInput(nullptr)
    , m_h(0)
    , m_s(0)
    , m_v(0)
{

    QLabel *labels[3];
    KisHsvColorSlider *sliders[3];
    KisDoubleParseSpinBox *inputs[3];
    const char *labelNames[3] = { "H:", "S:", "V:" };
    qreal maxValues[3] = { 360, 100, 100 };
    int labelWidth = 0;

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);

    for (int i = 0; i < 3; i++) {
        // Slider layout
        QHBoxLayout *sliderLayout = new QHBoxLayout();
        sliderLayout->setContentsMargins(0,0,0,0);
        sliderLayout->setSpacing(1);

        // Label
        QLabel *label = new QLabel(i18n(labelNames[i]), this);
        sliderLayout->addWidget(label);

        // Slider itself
        KisHsvColorSlider *slider = new KisHsvColorSlider(Qt::Horizontal, this);
        slider->setMinimum(0);
        slider->setMaximum(maxValues[i]);
        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sliderLayout->addWidget(slider);

        // Input box
        KisDoubleParseSpinBox *input = new KisDoubleParseSpinBox(this);
        input->setMinimum(0);
        input->setMaximum(maxValues[i]);

        input->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        input->setMinimumWidth(60);
        input->setMaximumWidth(60);

        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sliderLayout->addWidget(input);

        mainLayout->addLayout(sliderLayout);

        // Record max label width
        labelWidth = qMax(labelWidth, label->sizeHint().width());

        sliders[i] = slider;
        inputs[i] = input;
        labels[i] = label;
    }

    // Align the labels
    for (int i = 0; i < 3; i++) {
        labels[i]->setMinimumWidth(labelWidth);
    }

    // Connect slots
    connect(sliders[0], SIGNAL(valueChanged(int)), this, SLOT(hueSliderChanged(int)));
    connect(inputs[0], SIGNAL(valueChanged(double)), this, SLOT(setHue(double)));
    connect(sliders[1], SIGNAL(valueChanged(int)), this, SLOT(saturationSliderChanged(int)));
    connect(inputs[1], SIGNAL(valueChanged(double)), this, SLOT(setSaturation(double)));
    connect(sliders[2], SIGNAL(valueChanged(int)), this, SLOT(valueSliderChanged(int)));
    connect(inputs[2], SIGNAL(valueChanged(double)), this, SLOT(setValue(double)));

    m_hSlider = sliders[0];
    m_sSlider = sliders[1];
    m_vSlider = sliders[2];

    m_hInput = inputs[0];
    m_sInput = inputs[1];
    m_vInput = inputs[2];

    // Set initial values
    QColor c = m_color->toQColor();
    c.getHsvF(&m_h, &m_s, &m_v);
    m_hInput->setValue(m_h);
    m_sInput->setValue(m_s);
    m_vInput->setValue(m_v);

    // Update sliders
    QColor minC, maxC;
    minC.setHsvF(0, 1, 1);
    maxC.setHsvF(1, 1, 1);
    m_hSlider->setColors(minC, maxC);
    m_hSlider->setCircularHue(true);

    recolorSliders();
}

void KisHsvColorInput::sendUpdate()
{
    {
        KisSignalsBlocker blocker(
            m_hSlider, m_sSlider, m_vSlider
        );
        m_hSlider->setValue(m_h * 360);
        m_sSlider->setValue(m_s * 100);
        m_vSlider->setValue(m_v * 100);
    }

    recolorSliders();

    QColor c;
    c.setHsvF(m_h, m_s, m_v);

    m_color->fromQColor(c);
    emit(updated());
}

void KisHsvColorInput::setHue(double x)
{
    if (x < 0) {
        x = 0;
    }

    if (x > 360) {
        x = 360;
    }

    m_h = x / 360;
    sendUpdate();
}

void KisHsvColorInput::setSaturation(double x)
{
    if (x < 0) {
        x = 0;
    }

    if (x > 100) {
        x = 100;
    }

    m_s = x / 100;
    sendUpdate();
}

void KisHsvColorInput::setValue(double x)
{
    if (x < 0) {
        x = 0;
    }

    if (x > 100) {
        x = 100;
    }

    m_v = x / 100;
    sendUpdate();
}

void KisHsvColorInput::hueSliderChanged(int i)
{
    m_hInput->setValue(i);
}

void KisHsvColorInput::saturationSliderChanged(int i)
{
    m_sInput->setValue(i);
}

void KisHsvColorInput::valueSliderChanged(int i)
{
    m_vInput->setValue(i);
}

void KisHsvColorInput::recolorSliders() {
    // Update sliders
    QColor minC, maxC;
    minC.setHsvF(m_h, 0, m_v);
    maxC.setHsvF(m_h, 1, m_v);
    m_sSlider->setColors(minC, maxC);

    minC.setHsvF(m_h, m_s, 0);
    maxC.setHsvF(m_h, m_s, 1);
    m_vSlider->setColors(minC, maxC);
}

void KisHsvColorInput::update()
{
    KisSignalsBlocker blocker(
        m_hInput, m_sInput, m_vInput,
        m_hSlider, m_sSlider, m_vSlider
    );

    // Check if it is the same color we have
    QColor current;
    current.setHsvF(m_h, m_s, m_v);
    QColor theirs = m_color->toQColor();

    // Truncate to integer for this check
    if (!(current.red() == theirs.red() && current.green() == theirs.green() && current.blue() == theirs.blue())) {
        // Apply the update
        qreal theirH;
        theirs.getHsvF(&theirH, &m_s, &m_v);

        // Don't jump the Hue slider around to 0 if it is currently on 360
        const qreal EPSILON = 1e-6;
        if (!((1.0 - m_h) < EPSILON && (theirH - 0.0) < EPSILON)) {
            m_h = theirH;
        }

        m_hInput->setValue(m_h * 360);
        m_sInput->setValue(m_s * 100);
        m_vInput->setValue(m_v * 100);

        recolorSliders();

        // Update slider positions
        m_hSlider->setValue(m_h * 360);
        m_sSlider->setValue(m_s * 100);
        m_vSlider->setValue(m_v * 100);
    }
}
