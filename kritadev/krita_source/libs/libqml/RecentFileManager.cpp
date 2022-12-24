/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2012 Boudewijn Rempt <boud@kogmbh.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "RecentFileManager.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

#include <kconfiggroup.h>
#include <kconfig.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>

// Much of this is a gui-less clone of KRecentFilesAction, so the format of
// storing recent files is compatible.
class RecentFileManager::Private {
public:
    Private()
    {
        KConfigGroup grp = KSharedConfig::openConfig()->group("RecentFiles");
        maxItems = grp.readEntry("maxRecentFileItems", 100);

        loadEntries(grp);
    }

    void loadEntries(const KConfigGroup &grp)
    {
        recentFiles.clear();
        recentFilesIndex.clear();

        QString value;
        QString nameValue;
        QUrl url;

        KConfigGroup cg = grp;

        if (cg.name().isEmpty()) {
            cg = KConfigGroup(cg.config(), "RecentFiles");
        }

        // read file list
        for (int i = 0; i < maxItems; ++i) {
            value = cg.readPathEntry(QString("File%1").arg(i+1), QString());
            if (value.isEmpty()) {
                continue;
            }
            url = QUrl::fromUserInput(value);

            // krita sketch only handles local files
            if (!url.isLocalFile())
                continue;

            // Don't restore if file doesn't exist anymore
            if (!QFile::exists(url.toLocalFile())) {
                continue;
            }

#ifdef Q_OS_WIN
            value = QDir::toNativeSeparators(value);
#endif

            // Don't restore where the url is already known (eg. broken config)
            if (recentFiles.contains(value)) {
                continue;
            }

            nameValue = cg.readPathEntry(QString("Name%1").arg(i+1), url.fileName());

            if (!value.isNull()) {
                recentFilesIndex << nameValue;
                recentFiles << value;
           }
        }
    }

    void saveEntries(const KConfigGroup &grp)
    {
        KConfigGroup cg = grp;

        if (cg.name().isEmpty()) {
            cg = KConfigGroup(cg.config(), "RecentFiles");
        }
        cg.deleteGroup();

        cg.writeEntry("maxRecentFileItems", maxItems);

        // write file list
        for (int i = 0; i < recentFilesIndex.size(); ++i) {
            cg.writePathEntry(QString("File%1").arg(i+1), recentFiles[i]);
            cg.writePathEntry(QString("Name%1").arg(i+1), recentFilesIndex[i]);
        }
    }

    int maxItems;
    QStringList recentFilesIndex;
    QStringList recentFiles;
};

RecentFileManager::RecentFileManager(QObject *parent)
    : QObject(parent)
    , d(new Private())
{
}

RecentFileManager::~RecentFileManager()
{
    delete d;
}

QStringList RecentFileManager::recentFileNames() const
{
    return d->recentFilesIndex;
}

QStringList RecentFileManager::recentFiles() const
{
    return d->recentFiles;
}

void RecentFileManager::addRecent(const QString &_url)
{
    if (d->maxItems <= 0) {
        return;
    }

    if (d->recentFiles.size() > d->maxItems) {
        d->recentFiles.removeLast();
        d->recentFilesIndex.removeLast();
    }

    QString localFile = QDir::toNativeSeparators(_url);
    QString fileName  = QFileInfo(_url).fileName();

    if (d->recentFiles.contains(localFile)) {
        d->recentFiles.removeAll(localFile);
    }

    if (d->recentFilesIndex.contains(fileName)) {
        d->recentFilesIndex.removeAll(fileName);
    }

    d->recentFiles.insert(0, localFile);
    d->recentFilesIndex.insert(0, fileName);

    d->saveEntries(KSharedConfig::openConfig()->group("RecentFiles"));
    emit recentFilesListChanged();
}

int RecentFileManager::size()
{
    return d->recentFiles.size();
}

QString RecentFileManager::recentFile(int index) const
{
    if (index < d->recentFiles.size()) {
        return d->recentFiles.at(index);
    }
    return QString();
}

QString RecentFileManager::recentFileName(int index) const
{
    if (index < d->recentFilesIndex.size()) {
        return d->recentFilesIndex.at(index);
    }
    return QString();
}
