/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisMaskingBrushOptionProperties.h"

#include <KoResourceLoadResult.h>
#include "kis_brush_option.h"
#include <brushengine/KisPaintopSettingsIds.h>

#include <KoCompositeOpRegistry.h>
#include <kis_image_config.h>

KisMaskingBrushOptionProperties::KisMaskingBrushOptionProperties()
    : compositeOpId(COMPOSITE_MULT)

{
}

void KisMaskingBrushOptionProperties::write(KisPropertiesConfiguration *setting, qreal masterBrushSize) const
{
    setting->setProperty(KisPaintOpUtils::MaskingBrushEnabledTag, isEnabled);
    setting->setProperty(KisPaintOpUtils::MaskingBrushCompositeOpTag, compositeOpId);
    setting->setProperty(KisPaintOpUtils::MaskingBrushUseMasterSizeTag, useMasterSize);

    const qreal currentSize =
        theoreticalMaskingBrushSize ?
        *theoreticalMaskingBrushSize :
        brush ? brush->userEffectiveSize() : 1.0;

    const qreal masterSizeCoeff =
        masterBrushSize > 0 ? currentSize / masterBrushSize : 1.0;

    setting->setProperty(KisPaintOpUtils::MaskingBrushMasterSizeCoeffTag, masterSizeCoeff);

    // TODO: skip saving in some cases?
    // if (!isEnabled) return;

    if (brush) {
        KisPropertiesConfigurationSP embeddedConfig = new KisPropertiesConfiguration();

        {
            KisBrushOptionProperties option;
            option.setBrush(brush);
            option.writeOptionSetting(embeddedConfig);
        }

        setting->setPrefixedProperties(KisPaintOpUtils::MaskingBrushPresetPrefix, embeddedConfig);
    }
}

QList<KoResourceLoadResult> KisMaskingBrushOptionProperties::prepareLinkedResources(const KisPropertiesConfigurationSP settings, KisResourcesInterfaceSP resourcesInterface)
{
    KisBrushOptionProperties option;
    return option.prepareLinkedResources(settings, resourcesInterface);
}

void KisMaskingBrushOptionProperties::read(const KisPropertiesConfiguration *setting, qreal masterBrushSize, KisResourcesInterfaceSP resourcesInterface, KoCanvasResourcesInterfaceSP canvasResourcesInterface)
{
    isEnabled = setting->getBool(KisPaintOpUtils::MaskingBrushEnabledTag);
    compositeOpId = setting->getString(KisPaintOpUtils::MaskingBrushCompositeOpTag, COMPOSITE_MULT);
    useMasterSize = setting->getBool(KisPaintOpUtils::MaskingBrushUseMasterSizeTag, true);

    KisPropertiesConfigurationSP embeddedConfig = new KisPropertiesConfiguration();
    setting->getPrefixedProperties(KisPaintOpUtils::MaskingBrushPresetPrefix, embeddedConfig);

    KisBrushOptionProperties option;
    option.readOptionSetting(embeddedConfig, resourcesInterface, canvasResourcesInterface);

    brush = option.brush();
    theoreticalMaskingBrushSize = boost::none;

    if (brush && useMasterSize) {
        const qreal masterSizeCoeff = setting->getDouble(KisPaintOpUtils::MaskingBrushMasterSizeCoeffTag, 1.0);
        qreal size = masterSizeCoeff * masterBrushSize;

        const qreal maxMaskingBrushSize = KisImageConfig(true).maxMaskingBrushSize();

        if (size > maxMaskingBrushSize) {
            theoreticalMaskingBrushSize = size;
            size = maxMaskingBrushSize;
        }

        brush->setUserEffectiveSize(size);
    }
}
