/*
 * SPDX-FileCopyrightText: 2006-2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoPluginLoader.h"

#include <KoJsonTrader.h>

#include <QJsonObject>
#include <QPluginLoader>

#include "kis_debug.h"

#include <kis_debug.h>

#include <KConfig>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KPluginFactory>

class Q_DECL_HIDDEN KoPluginLoader::Private
{
public:
    QStringList loadedServiceTypes;
};

KoPluginLoader::KoPluginLoader()
        : d(new Private())
{
}

KoPluginLoader::~KoPluginLoader()
{
    delete d;
}

Q_GLOBAL_STATIC(KoPluginLoader, pluginLoaderInstance)

KoPluginLoader* KoPluginLoader::instance()
{
    return pluginLoaderInstance();
}

void KoPluginLoader::load(const QString & serviceType, const QString & versionString, const PluginsConfig &config, QObject* owner, bool cache)
{

    // Don't load the same plugins again
    if (cache && d->loadedServiceTypes.contains(serviceType)) {
        return;
    }
    d->loadedServiceTypes << serviceType;
    QString query = QString::fromLatin1("(Type == 'Service')");
    if (!versionString.isEmpty()) {
        query += QString::fromLatin1(" and (%1)").arg(versionString);
    }

    QList<KoJsonTrader::Plugin> offers = KoJsonTrader::instance()->query(serviceType, QString());

    QList<KoJsonTrader::Plugin> plugins;

    bool configChanged = false;
    QList<QString> blacklist; // what we will save out afterwards
    if (config.whiteList && config.blacklist && config.group) {
        dbgPlugins << "Loading" << serviceType << "with checking the config";
        KConfigGroup configGroup(KSharedConfig::openConfig(), config.group);
        QList<QString> whiteList = configGroup.readEntry(config.whiteList, config.defaults);
        QList<QString> knownList;

        // if there was no list of defaults; all plugins are loaded.
        const bool firstStart = !config.defaults.isEmpty() && !configGroup.hasKey(config.whiteList);
        knownList = configGroup.readEntry(config.blacklist, knownList);
        if (firstStart) {
            configChanged = true;
        }
        Q_FOREACH (const KoJsonTrader::Plugin &loader, offers) {
            QJsonObject json = loader.metaData().value("MetaData").toObject();
            if (json.contains("KPlugin")) {
                json = json.value("KPlugin").toObject();
            }
            const QString pluginName = json.value("Id").toString();
            if (pluginName.isEmpty()) {
                qWarning() << "Loading plugin" << loader.fileName() << "failed, has no X-KDE-PluginInfo-Name.";
                continue;
            }

            if (whiteList.contains(pluginName)) {
                plugins.append(loader);
            }
            else if (!firstStart && !knownList.contains(pluginName)) { // also load newly installed plugins.
                plugins.append(loader);
                configChanged = true;
            }
            else {
                blacklist << pluginName;
            }
        }
    } else {
        plugins = offers;
    }

    QMap<QString, KoJsonTrader::Plugin> serviceNames;
    Q_FOREACH (const KoJsonTrader::Plugin &loader, plugins) {
        if (serviceNames.contains(loader.fileName())) { // duplicate
            QJsonObject json2 = loader.metaData().value("MetaData").toObject();
            QVariant pluginVersion2 = json2.value("X-Flake-PluginVersion").toVariant();
            if (pluginVersion2.isNull()) { // just take the first one found...
                continue;
            }
            KoJsonTrader::Plugin currentLoader = serviceNames.value(loader.fileName());
            QJsonObject json = currentLoader.metaData().value("MetaData").toObject();
            QVariant pluginVersion = json.value("X-Flake-PluginVersion").toVariant();
            if (!(pluginVersion.isNull() || pluginVersion.toInt() < pluginVersion2.toInt())) {
                continue; // replace the old one with this one, since its newer.
            }
        }
        serviceNames.insert(loader.fileName(), loader);
    }

    QList<QString> whiteList;
    Q_FOREACH (const QString &serviceName, serviceNames.keys()) {
        dbgPlugins << "loading" << serviceName;
        KoJsonTrader::Plugin loader = serviceNames[serviceName];
        KPluginFactory *factory = qobject_cast<KPluginFactory *>(loader.instance());
        QObject *plugin = 0;
        if (factory) {
            plugin = factory->create<QObject>(owner ? owner : this, QVariantList());
        }
        if (plugin) {
            QJsonObject json = loader.metaData().value("MetaData").toObject();
            json = json.value("KPlugin").toObject();
            const QString pluginName = json.value("Id").toString();
            whiteList << pluginName;
            dbgPlugins << "\tLoaded plugin" << loader.fileName() << owner;
            if (!owner) {
                delete plugin;
            }
        } else {
            qWarning() << "Loading plugin" << loader.fileName() << "failed, " << loader.errorString();
        }
    }

    if (configChanged && config.whiteList && config.blacklist && config.group) {
        KConfigGroup configGroup(KSharedConfig::openConfig(), config.group);
        configGroup.writeEntry(config.whiteList, whiteList);
        configGroup.writeEntry(config.blacklist, blacklist);
    }
}
