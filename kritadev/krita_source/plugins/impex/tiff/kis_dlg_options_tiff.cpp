/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_dlg_options_tiff.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QSlider>
#include <QStackedWidget>
#include <QApplication>

#include <kcombobox.h>
#include <klocalizedstring.h>

#include <KisImportExportFilter.h>
#include <KoChannelInfo.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpace.h>
#include <kis_config.h>
#include <kis_properties_configuration.h>

#include <config-tiff.h>

KisTIFFOptionsWidget::KisTIFFOptionsWidget(QWidget *parent)
    : KisConfigWidget(parent)
{
    setupUi(this);
    activated(0);
    connect(kComboBoxCompressionType, SIGNAL(activated(int)), this, SLOT(activated(int)));
    connect(flatten, SIGNAL(toggled(bool)), this, SLOT(flattenToggled(bool)));
    QApplication::restoreOverrideCursor();
    setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));

    // See KisTIFFOptions::fromProperties
    kComboBoxCompressionType->addItem(i18nc("TIFF options", "None"), 0);
    kComboBoxCompressionType->addItem(
        i18nc("TIFF options", "JPEG DCT compression"),
        1);
    kComboBoxCompressionType->addItem(i18nc("TIFF options", "Deflate (ZIP)"),
                                      2);
    kComboBoxCompressionType->addItem(
        i18nc("TIFF options", "Lempel-Ziv & Welch"),
        3);
    kComboBoxCompressionType->addItem(i18nc("TIFF options", "Pixar Log"), 4);

    connect(kComboBoxCompressionType,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            [&](int index) {
                const int deflate = kComboBoxCompressionType->findData(2);
                const int lzw = kComboBoxCompressionType->findData(3);
                kComboBoxPredictor->setEnabled(index == deflate
                                               || index == lzw);
            });

    kComboBoxPredictor->addItem(i18nc("TIFF options", "None"), 0);
    kComboBoxPredictor->addItem(
        i18nc("TIFF options", "Horizontal Differencing"),
        1);
    kComboBoxPredictor->addItem(
        i18nc("TIFF options", "Floating Point Horizontal Differencing"),
        2);
}

void KisTIFFOptionsWidget::setConfiguration(const KisPropertiesConfigurationSP cfg)
{
    kComboBoxCompressionType->setCurrentIndex(cfg->getInt("compressiontype", 0));
    activated(kComboBoxCompressionType->currentIndex());
    kComboBoxPredictor->setCurrentIndex(cfg->getInt("predictor", 0));
    alpha->setChecked(cfg->getBool("alpha", true));
#ifdef TIFF_CAN_WRITE_PSD_TAGS
    chkPhotoshop->setEnabled(true);
    chkPhotoshop->setChecked(cfg->getBool("saveAsPhotoshop", false));
    kComboBoxPSDCompressionType->setCurrentIndex(cfg->getInt("psdCompressionType", 0));
#else
    chkPhotoshop->setEnabled(false);
    chkPhotoshop->setChecked(false);
#endif
    flatten->setChecked(cfg->getBool("flatten", true));
    flattenToggled(flatten->isChecked());
    qualityLevel->setValue(cfg->getInt("quality", 80));
    compressionLevelDeflate->setValue(cfg->getInt("deflate", 6));
    compressionLevelPixarLog->setValue(cfg->getInt("pixarlog", 6));
    chkSaveProfile->setChecked(cfg->getBool("saveProfile", true));

    {
        const QString colorDepthId =
            cfg->getString(KisImportExportFilter::ColorDepthIDTag);

        if (colorDepthId == Float16BitsColorDepthID.id()
            || colorDepthId == Float32BitsColorDepthID.id()
            || colorDepthId == Float16BitsColorDepthID.id()) {
            kComboBoxPredictor->removeItem(1);
        } else {
            kComboBoxPredictor->removeItem(2);
        }

        if (colorDepthId != Integer8BitsColorDepthID.id()) {
            kComboBoxCompressionType->removeItem(
                kComboBoxCompressionType->findData(1));
        }
    }

    {
        const QString colorModelId =
            cfg->getString(KisImportExportFilter::ColorModelIDTag);

        if (colorModelId == CMYKAColorModelID.id()
            || colorModelId == YCbCrAColorModelID.id()) {
            alpha->setChecked(false);
            alpha->setEnabled(false);
        }
    }
}

KisPropertiesConfigurationSP KisTIFFOptionsWidget::configuration() const
{
    KisPropertiesConfigurationSP cfg(new KisPropertiesConfiguration());
    cfg->setProperty("compressiontype",
                     kComboBoxCompressionType->currentData());
    cfg->setProperty("predictor", kComboBoxPredictor->currentData());
    cfg->setProperty("alpha", alpha->isChecked());
    cfg->setProperty("saveAsPhotoshop", chkPhotoshop->isChecked());
    cfg->setProperty("psdCompressionType", kComboBoxPSDCompressionType->currentIndex());
    cfg->setProperty("flatten", flatten->isChecked());
    cfg->setProperty("quality", qualityLevel->value());
    cfg->setProperty("deflate", compressionLevelDeflate->value());
    cfg->setProperty("pixarlog", compressionLevelPixarLog->value());
    cfg->setProperty("saveProfile", chkSaveProfile->isChecked());

    return cfg;
}

void KisTIFFOptionsWidget::activated(int index)
{
    const int data = kComboBoxCompressionType->itemData(index).value<int>();
    switch (data) {
    case 1: // JPEG
        codecsOptionsStack->setCurrentIndex(1);
        break;
    case 2: // Deflate
        codecsOptionsStack->setCurrentIndex(2);
        break;
    case 4: // Pixar Log
        codecsOptionsStack->setCurrentIndex(3);
        break;
    default:
        codecsOptionsStack->setCurrentIndex(0);
    }
}

void KisTIFFOptionsWidget::flattenToggled(bool t)
{
    alpha->setEnabled(t);
    if (!t) {
        alpha->setChecked(true);
    }
    chkPhotoshop->setEnabled(!t);
    if (t) {
        chkPhotoshop->setChecked(false);
    }
}

