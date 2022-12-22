/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestResourceTypeModel.h"

#include <simpletest.h>
#include <QStandardPaths>
#include <QDir>
#include <QVersionNumber>
#include <QDirIterator>
#include <QSqlError>
#include <QSqlQuery>
#include <QtSql>
#include <QModelIndex>
#include <QAbstractItemModelTester>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KisResourceLoaderRegistry.h>
#include <KisResourceTypeModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>


#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestResourceTypeModel::initTestCase()
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

void TestResourceTypeModel::testWithTagModelTester()
{
    KisResourceTypeModel model;
    auto tester = new QAbstractItemModelTester(&model);
    Q_UNUSED(tester);
}


void TestResourceTypeModel::testRowCount()
{
    QSqlQuery q;
    QVERIFY(q.prepare("SELECT count(*)\n"
                      "FROM   resource_types"));
    QVERIFY(q.exec());
    q.first();
    int rowCount = q.value(0).toInt();
    QCOMPARE(rowCount, KisResourceLoaderRegistry::instance()->resourceTypes().count());

    KisResourceTypeModel typeModel;
    QCOMPARE(typeModel.rowCount(), rowCount);
}

void TestResourceTypeModel::testData()
{
    KisResourceTypeModel typeModel;
    for(int i = 0; i < typeModel.rowCount(); ++i) {

        QModelIndex idx = typeModel.index(0, KisResourceTypeModel::ResourceType);
//        qDebug() << "test" << idx.data(Qt::DisplayRole)
//                 << idx.data(Qt::UserRole + KisResourceTypeModel::Id)
//                 << idx.data(Qt::UserRole + KisResourceTypeModel::ResourceType)
//                 << idx.data(Qt::UserRole + KisResourceTypeModel::Name);

        QCOMPARE(ResourceName::resourceTypeToName(ResourceType::Brushes), idx.data(Qt::UserRole + KisResourceTypeModel::Name).toString());
    }
}


void TestResourceTypeModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}




SIMPLE_TEST_MAIN(TestResourceTypeModel)

