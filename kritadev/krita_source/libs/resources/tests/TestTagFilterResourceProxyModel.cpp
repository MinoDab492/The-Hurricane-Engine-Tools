/*
 * SPDX-FileCopyrightText: 2019 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestTagFilterResourceProxyModel.h"

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
#include <KisTagFilterResourceProxyModel.h>
#include <KisStorageModel.h>

#include <DummyResource.h>
#include <ResourceTestHelper.h>

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing installing resources"
#endif


void TestTagFilterResourceProxyModel::initTestCase()
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

void TestTagFilterResourceProxyModel::testWithTagModelTester()
{
    KisTagFilterResourceProxyModel model(m_resourceType);
    auto tester = new QAbstractItemModelTester(&model);
    Q_UNUSED(tester);
}


void TestTagFilterResourceProxyModel::testRowCount()
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
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    QCOMPARE(proxyModel.rowCount(), rowCount);
}

void TestTagFilterResourceProxyModel::testData()
{
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    KisResourceModel *resourceModel = qobject_cast<KisResourceModel*>(proxyModel.sourceModel());

    QStringList names = QStringList() << "test0.kpp"
                                      << "test1.kpp"
                                      << "test2.kpp";
    for (int i = 0; i < proxyModel.rowCount(); ++i)  {
        QVariant v = resourceModel->data(proxyModel.mapToSource(proxyModel.index(i, 0)), Qt::UserRole + KisAbstractResourceModel::Name);
        QVERIFY(names.contains(v.toString()));
    }
}


void TestTagFilterResourceProxyModel::testResource()
{
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    KisResourceModel *resourceModel = qobject_cast<KisResourceModel*>(proxyModel.sourceModel());

    KoResourceSP resource = resourceModel->resourceForIndex(proxyModel.mapToSource(proxyModel.index(0, 0)));
    QVERIFY(resource);
}

void TestTagFilterResourceProxyModel::testFilterByTag()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);

    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    QVERIFY(resource);

    KisTagSP tag = tagModel.tagForIndex(tagModel.index(2, 0));
    QVERIFY(tag);

    proxyModel.setTagFilter(tag);
    int rowCount = proxyModel.rowCount();

    proxyModel.tagResources(tag, QVector<int>() << resource->resourceId());
    QCOMPARE(proxyModel.rowCount(), rowCount + 1);

    proxyModel.untagResources(tag, QVector<int>() << resource->resourceId());
    QCOMPARE(proxyModel.rowCount(), rowCount);
}

void TestTagFilterResourceProxyModel::testFilterByResource()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);

    KisTagFilterResourceProxyModel proxyModel(m_resourceType);

    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();

    QVERIFY(resource);

    tagModel.addTag("testtag1", false, QVector<KoResourceSP>() << resource);
    tagModel.addTag("testtag2", false, QVector<KoResourceSP>() << resource);

    int rowCount = proxyModel.rowCount();

    proxyModel.setResourceFilter(resource);
    proxyModel.setFilterInCurrentTag(false);

    QCOMPARE(proxyModel.rowCount(), 2);

    proxyModel.setResourceFilter(0);
    QCOMPARE(proxyModel.rowCount(), rowCount);

}

void TestTagFilterResourceProxyModel::testFilterByString()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);

    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    proxyModel.setSearchText("test2");
    QCOMPARE(proxyModel.rowCount(), 1);

    KoResourceSP resource = resourceModel.resourcesForName("test2.kpp").first();
    QVERIFY(resource);

    KisTagSP tag = tagModel.tagForIndex(tagModel.index(2, 0));
    QVERIFY(tag);

    proxyModel.tagResources(tag, QVector<int>() << resource->resourceId());
    proxyModel.setTagFilter(tag);
    proxyModel.setFilterInCurrentTag(true);

    QCOMPARE(proxyModel.rowCount(), 1);
}

void TestTagFilterResourceProxyModel::testFilterByStorage()
{
    KisResourceModel resourceModel(ResourceType::PaintOpPresets);
    KisTagModel tagModel(ResourceType::PaintOpPresets);

    KisTagFilterResourceProxyModel proxyModel(m_resourceType);

    proxyModel.setFilterInCurrentTag(false);
    proxyModel.setStorageFilter(true, 1);
    proxyModel.setSearchText("");
    proxyModel.setMetaDataFilter(QMap<QString, QVariant>());
    proxyModel.setResourceFilter(0);

    QCOMPARE(proxyModel.rowCount(), 3);

}


void TestTagFilterResourceProxyModel::testDataWhenSwitchingBetweenTagAllAllUntagged()
{
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    KisResourceModel *resourceModel = qobject_cast<KisResourceModel*>(proxyModel.sourceModel());

    KoResourceSP resource = resourceModel->resourcesForName("test2.kpp").first();
    QModelIndex idx = proxyModel.indexForResource(resource);

    QVERIFY(idx.isValid());

    QString name = proxyModel.data(idx, Qt::UserRole + KisAbstractResourceModel::Name).toString();
    QCOMPARE(name, "test2.kpp");

    QImage thumbnail = proxyModel.data(idx, Qt::UserRole + KisAbstractResourceModel::Thumbnail).value<QImage>();
    QVERIFY(!thumbnail.isNull());

    proxyModel.setSearchText("test2");
    idx = proxyModel.indexForResource(resource);
}

void TestTagFilterResourceProxyModel::testResourceForIndex()
{
    KisTagModel tagModel(ResourceType::PaintOpPresets);
    KisTagFilterResourceProxyModel proxyModel(m_resourceType);
    KisResourceModel *resourceModel = qobject_cast<KisResourceModel*>(proxyModel.sourceModel());

    KoResourceSP resource = resourceModel->resourcesForName("test2.kpp").first();
    QVERIFY(resource);

    QModelIndex idx = proxyModel.indexForResource(resource);
    QVERIFY(idx.isValid());

    resource = proxyModel.resourceForIndex(idx);
    QVERIFY(resource);


    KisTagResourceModel tagResourceModel(ResourceType::PaintOpPresets);
    tagResourceModel.setResourcesFilter(QVector<KoResourceSP>() << resource);
    for (int i = 0; i < tagResourceModel.rowCount(); ++i) {
        KisTagSP tag = tagResourceModel.index(i, 0).data(Qt::UserRole + KisAllTagResourceModel::Tag).value<KisTagSP>();
        tagResourceModel.untagResources(tag, QVector<int>() << resource->resourceId());
    }

    KisTagSP tag = tagModel.tagForIndex(tagModel.index(3, 0));
    QVERIFY(tag);

    proxyModel.setTagFilter(tag);
    int rowCount = proxyModel.rowCount();

    QCOMPARE(rowCount, 0);

    proxyModel.tagResources(tag, QVector<int>() << resource->resourceId());

    QCOMPARE(proxyModel.rowCount(), 1);

    idx = proxyModel.index(0, 0);
    KoResourceSP resource2 = proxyModel.resourceForIndex(idx);

    QVERIFY(resource2);

}
void TestTagFilterResourceProxyModel::cleanupTestCase()
{
    ResourceTestHelper::rmTestDb();
    ResourceTestHelper::cleanDstLocation(m_dstLocation);
}

#include <sdk/tests/kistest.h>
KISTEST_MAIN(TestTagFilterResourceProxyModel)

