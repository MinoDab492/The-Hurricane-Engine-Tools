/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_uniform_paintop_property.h"

#include <QVariant>
#include "kis_debug.h"
#include "kis_paintop_settings.h"

struct KisUniformPaintOpProperty::Private
{
    Private(Type _type, SubType _subType, const KoID &_id, KisPaintOpSettingsSP _settings)
        : type(_type)
        , subType(_subType)
        , id(_id)
        , settings(_settings)
        , isReadingValue(false)
        , isWritingValue(false)
    {
    }

    Type type;
    SubType subType;
    KoID id;

    QVariant value;

    KisPaintOpSettingsSP settings;
    bool isReadingValue;
    bool isWritingValue;
};

KisUniformPaintOpProperty::KisUniformPaintOpProperty(Type type, SubType subType, const KoID &id, KisPaintOpSettingsRestrictedSP settings, QObject *parent)
    : QObject(parent)
    , m_d(new Private(type, subType, id, settings))
{
}

KisUniformPaintOpProperty::KisUniformPaintOpProperty(Type type, const KoID &id, KisPaintOpSettingsRestrictedSP settings, QObject *parent)
    : QObject(parent)
    , m_d(new Private(type, SubType_None, id, settings))
{
}

KisUniformPaintOpProperty::KisUniformPaintOpProperty(const KoID &id, KisPaintOpSettingsRestrictedSP settings, QObject *parent)
    : QObject(parent)
    , m_d(new Private(Bool, SubType_None, id, settings))
{
}

KisUniformPaintOpProperty::~KisUniformPaintOpProperty()
{
}

QString KisUniformPaintOpProperty::id() const
{
    return m_d->id.id();
}

QString KisUniformPaintOpProperty::name() const
{
    return m_d->id.name();
}

KisUniformPaintOpProperty::Type KisUniformPaintOpProperty::type() const
{
    return m_d->type;
}

KisUniformPaintOpProperty::SubType KisUniformPaintOpProperty::subType() const
{
    return m_d->subType;
}

QVariant KisUniformPaintOpProperty::value() const
{
    return m_d->value;
}

QWidget *KisUniformPaintOpProperty::createPropertyWidget()
{
    return nullptr;
}

void KisUniformPaintOpProperty::setValue(const QVariant &value)
{
    if (m_d->value == value) return;
    m_d->value = value;

    emit valueChanged(value);

    if (!m_d->isReadingValue) {
        m_d->isWritingValue = true;
        writeValueImpl();
        m_d->isWritingValue = false;
    }
}

void KisUniformPaintOpProperty::requestReadValue()
{
    if (m_d->isWritingValue) return;

    m_d->isReadingValue = true;
    readValueImpl();
    m_d->isReadingValue = false;
}

KisPaintOpSettingsSP KisUniformPaintOpProperty::settings() const
{
    // correct conversion weak-to-strong shared pointer
    return m_d->settings ? m_d->settings : KisPaintOpSettingsSP();
}

bool KisUniformPaintOpProperty::isVisible() const
{
    return true;
}

void KisUniformPaintOpProperty::readValueImpl()
{
}

void KisUniformPaintOpProperty::writeValueImpl()
{
}

#include "kis_callback_based_paintop_property_impl.h"

template class KRITAIMAGE_EXPORT_INSTANCE
    KisCallbackBasedPaintopProperty<KisUniformPaintOpProperty>;
