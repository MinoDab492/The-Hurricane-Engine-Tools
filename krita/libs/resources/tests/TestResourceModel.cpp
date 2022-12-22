/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestResourceModel.h"

#include <simpletest.h>
#include <QStandardPaths>
#include <QDir>
#include <QVersionNumber>
#include <QDirIterator>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QtSql>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KisResourceModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestResourceModel::initTestCase()
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
        qWarning() << "Could not initialize KisResourceCacheDb on" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QVERIFY(KisResourceCacheDb::isValid());

    KisResourceLocator::LocatorError r = m_locator->initialize(m_srcLocation);
    if (!m_locator->errorMessages().isEmpty()) {
        qDebug() << m_locator->errorMessages();
    }

    QVERIFY(r == KisResourceLocator::LocatorError::Ok);
    QVERIFY(QDir(m_dstLocation).exists());
}

void TestResourceModel::testWithTagModelTester()
{
    KisResourceModel model(m_resourceType);
    auto tester = new QAbstractItemModelTester(&model);
    Q_UNUSED(tester);
}


void TestResourceModel::testRowCount()
{
    QSqlQuery q;
    QVERIFY(q.prepare("SELECT count(*)\n"
                      "FROM   resources\n"
                      ",      resource_types\n"
                      "WHERE  resources.resource_type_id = resource_types.id\n"
                      "AND    resource_types.name = :resource_type"));
    q.bindValue(":resource_type", m_resourceType);
    QVERIFY(q.exec());
    q.first();
    int rowCount = q.value(0).toInt();
    QVERIFY(rowCount == 3);
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);
    QCOMPARE(resourceModel.rowCount(), rowCount);
}

void TestResourceModel::testData()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    QStringList resourceNames;

    for (int i = 0; i < resourceModel.rowCount(); ++i)  {
        QVariant v = resourceModel.data(resourceModel.index(i, KisAbstractResourceModel::Name), Qt::DisplayRole);
        resourceNames << v.toString();
    }

    QVERIFY(resourceNames.contains("test0.kpp"));
    QVERIFY(resourceNames.contains("test1.kpp"));
    QVERIFY(resourceNames.contains("test2.kpp"));
}


void TestResourceModel::testResourceForIndex()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(0, 0));
    QVERIFY(resource);
    QVERIFY(resource->resourceId() > -1);
}

void TestResourceModel::testIndexFromResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(1, 0));
    QModelIndex idx = resourceModel.indexForResource(resource);
    QVERIFY(idx.row() == 1);
    QVERIFY(idx.column() == 0);
}

void TestResourceModel::testSetInactiveByIndex()
{
    KisResourceModel resourceModel(m_resourceType);
    int resourceCount = resourceModel.rowCount();
    KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(0, 0));
    bool r = resourceModel.setResourceInactive(resourceModel.index(0, 0));
    QVERIFY(r);
    QCOMPARE(resourceCount - 1, resourceModel.rowCount());
    QVERIFY(!resourceModel.indexForResource(resource).isValid());
    QVERIFY(!resourceModel.resourceForId(resource->resourceId()));
    QVERIFY(resourceModel.resourcesForName(resource->name()).isEmpty());
    // verify that all mapped resources are still reachable by id
    for (int i = 0; i < resourceCount - 1; i++) {
        KoResourceSP resource2 = resourceModel.resourceForIndex(resourceModel.index(i, 0));
        QVERIFY(resource2);
        QVERIFY(resourceModel.resourceForId(resource2->resourceId()));
    }
}

void TestResourceModel::testImportResourceFile()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    QTemporaryFile f(QDir::tempPath() + "/testresourcemodel-testimportresourcefile-XXXXXX.kpp");
    f.open();
    f.write("0");
    f.close();

    int resourceCount = resourceModel.rowCount();
    bool r = resourceModel.importResourceFile(f.fileName(), false);
    QVERIFY(r);
    QCOMPARE(resourceModel.rowCount(), resourceCount + 1);
}

void TestResourceModel::testAddResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    int resourceCount = resourceModel.rowCount();
    KoResourceSP resource(new DummyResource("dummy.kpp"));
    resource->setValid(true);
    bool r = resourceModel.addResource(resource);
    QVERIFY(r);
    QCOMPARE(resourceCount + 1, resourceModel.rowCount());
}

void TestResourceModel::testAddTemporaryResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    int startResourceCount = resourceModel.rowCount();
    KoResourceSP resource(new DummyResource("temporaryResource.kpp"));
    resource->setValid(true);
    bool r = resourceModel.addResource(resource, "memory");
    QVERIFY(r);
    QCOMPARE(startResourceCount + 1, resourceModel.rowCount());
}

void TestResourceModel::testAddDuplicatedResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    const int startResourceCount = resourceModel.rowCount();

    KoResourceSP resource(new DummyResource("duplicated_resource.kpp"));
    resource->setValid(true);
    bool r = resourceModel.addResource(resource); // first add to the folder storage

    QVERIFY(r);
    QCOMPARE(resourceModel.rowCount(), startResourceCount + 1);

    // Matching MD5, name and filename resources should be hidden -- BUG:445367
    // the copy of this resource has been added in testAddResource()
    resource.reset(new DummyResource("duplicated_resource.kpp"));
    resource->setValid(true);
    r = resourceModel.addResource(resource, "memory"); // then add to the temporary storage

    QVERIFY(r);
    QCOMPARE(resourceModel.rowCount(), startResourceCount + 1);
}

void TestResourceModel::testResourceForId()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(0, 0));
    QVERIFY(!resource.isNull());
    KoResourceSP resource2 = resourceModel.resourceForId(resource->resourceId());
    QVERIFY(!resource2.isNull());
    QCOMPARE(resource, resource2);
}

void TestResourceModel::testResourceForName()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(0, 0));
    QVERIFY(!resource.isNull());
    KoResourceSP resource2 = resourceModel.resourcesForName(resource->name()).first();
    QVERIFY(!resource2.isNull());
    QCOMPARE(resource, resource2);
}

void TestResourceModel::testResourceForFileName()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(0, 0));
    QVERIFY(!resource.isNull());
    KoResourceSP resource2 = resourceModel.resourcesForFilename(resource->filename()).first();
    QVERIFY(!resource2.isNull());
    QCOMPARE(resource, resource2);
}

void TestResourceModel::testResourceForMD5()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(0, 0));
    QVERIFY(!resource.isNull());
    QVector<KoResourceSP> v = resourceModel.resourcesForMD5(resource->md5Sum());
    KoResourceSP resource2 = v.first();
    QVERIFY(!resource2.isNull());
    QCOMPARE(resource->md5Sum(), resource2->md5Sum());
}

void TestResourceModel::testRenameResource()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(1, 0));
    QVERIFY(!resource.isNull());
    const QString name = resource->name();
    bool r = resourceModel.renameResource(resource, "A New Name");
    QVERIFY(r);
    QSqlQuery q;
    if (!q.prepare("SELECT name\n"
                   "FROM   resources\n"
                   "WHERE  id = :resource_id\n")) {
        qWarning() << "Could not prepare testRenameResource Query" << q.lastError();
    }

    q.bindValue(":resource_id", resource->resourceId());

    if (!q.exec()) {
        qWarning() << "Could not execute testRenameResource Query" << q.lastError();
    }

    q.first();
    QString newName = q.value(0).toString();
    QVERIFY(name != newName);
    QCOMPARE("A New Name", newName);
}

void TestResourceModel::testUpdateResource()
{
    int resourceId;
    {
        KisResourceModel resourceModel(m_resourceType);
        resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

        KoResourceSP resource = resourceModel.resourceForIndex(resourceModel.index(0, 0));
        QVERIFY(resource);
        resource.dynamicCast<DummyResource>()->setSomething("It's changed");
        resourceId = resource->resourceId();
        bool r = resourceModel.updateResource(resource);
        QVERIFY(r);
    }

    {
        // Check the resource itself
        KisResourceLocator::instance()->purge("");
        KoResourceSP resource = KisResourceLocator::instance()->resourceForId(resourceId);

        QVERIFY(resource);
        QCOMPARE(resource.dynamicCast<DummyResource>()->something(), "It's changed");
        QVERIFY(resource->resourceId() == resourceId);

        // Check the versions in the database
        QSqlQuery q;
        QVERIFY(q.prepare("SELECT count(*)\n"
                          "FROM   versioned_resources\n"
                          "WHERE  resource_id = :resource_id\n"));
        q.bindValue(":resource_id", resourceId);
        QVERIFY(q.exec());
        q.first();
        int rowCount = q.value(0).toInt();
        QCOMPARE(rowCount, 2);
    }
}

void TestResourceModel::testTwoExistingResourceModels()
{
    KisResourceModel resourceModel(m_resourceType);
    resourceModel.setResourceFilter(KisResourceModel::ShowAllResources);

    KisResourceModel resourceModelCopy(m_resourceType);
    resourceModelCopy.setResourceFilter(KisResourceModel::ShowAllResources);


    int resourceCount = resourceModel.rowCount();
    KoResourceSP resource(new DummyResource("dummy_1.kpp"));
    resource->setValid(true);


    bool r = resourceModel.addResource(resource);
    QVERIFY(r);


    // it only works if you uncomment this line
    // but it should work without it
    // resourceModelCopy.invalidate();

    QCOMPARE(resourceModel.rowCount(), resourceModelCopy.rowCount());
    QCOMPARE(resourceCount + 1, resourceModel.rowCount());
    QCOMPARE(resourceCount + 1, resourceModelCopy.rowCount());
}


void TestResourceModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}


SIMPLE_TEST_MAIN(TestResourceModel)

