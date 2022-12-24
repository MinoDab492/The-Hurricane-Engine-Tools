/*
 * SPDX-FileCopyrightText: 2019 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestStorageFilterProxyModel.h"

#include <simpletest.h>
#include <QStandardPaths>
#include <QDir>
#include <QVersionNumber>
#include <QDirIterator>
#include <QSqlError>
#include <QSqlQuery>
#include <QAbstractItemModelTester>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KisResourceModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>

#include <KisStorageFilterProxyModel.h>


#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestStorageFilterProxyModel::initTestCase()
{
    ResourceTestHelper::initTestDb();
    ResourceTestHelper::createDummyLoaderRegistry();

    m_srcLocation = QString(FILES_DATA_DIR);
    QVERIFY2(QDir(m_srcLocation).exists(), m_srcLocation.toUtf8());

    m_dstLocation = ResourceTestHelper::filesDestDir();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);

    KConfigGroup cfg(KSharedConfig::openConfig(), "");
    cfg.writeEntry(KisResourceLocator::resourceLocationKey, m_dstLocation);

    m_locator = KisResourceLocator::instance();

    if (!KisResourceCacheDb::initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))) {
        qDebug() << "Could not initialize KisResourceCacheDb on" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QVERIFY(KisResourceCacheDb::isValid());

    KisResourceLocator::LocatorError r = m_locator->initialize(m_srcLocation);
    if (!m_locator->errorMessages().isEmpty()) qDebug() << m_locator->errorMessages();

    QVERIFY(r == KisResourceLocator::LocatorError::Ok);
    QVERIFY(QDir(m_dstLocation).exists());
}

void TestStorageFilterProxyModel::testWithTagModelTester()
{
    KisStorageFilterProxyModel model;
    model.setSourceModel(KisStorageModel::instance());
    auto tester = new QAbstractItemModelTester(&model);
    Q_UNUSED(tester);
}


void TestStorageFilterProxyModel::testFilterByName()
{
    QScopedPointer<KisStorageFilterProxyModel> proxyModel(new KisStorageFilterProxyModel());
    proxyModel->setSourceModel(KisStorageModel::instance());

    QString fileName = "test1";

    proxyModel->setFilter(KisStorageFilterProxyModel::ByStorageType, fileName);

}

void TestStorageFilterProxyModel::testFilterByType()
{
    QScopedPointer<KisStorageFilterProxyModel> proxyModel(new KisStorageFilterProxyModel());
    proxyModel->setSourceModel(KisStorageModel::instance());
    proxyModel->setFilter(KisStorageFilterProxyModel::ByStorageType,
                          QStringList()
                          << KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType::Bundle)
                          << KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType::Folder));

}

void TestStorageFilterProxyModel::testFilterByActive()
{
    QScopedPointer<KisStorageFilterProxyModel> proxyModel(new KisStorageFilterProxyModel());
    proxyModel->setSourceModel(KisStorageModel::instance());
    proxyModel->setFilter(KisStorageFilterProxyModel::ByStorageType, true);
}


void TestStorageFilterProxyModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}


SIMPLE_TEST_MAIN(TestStorageFilterProxyModel)

