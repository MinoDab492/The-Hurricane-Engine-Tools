/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISTAGFILTERRESOURCEPROXYMODEL_H
#define KISTAGFILTERRESOURCEPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QObject>

#include "KoResource.h"
#include "KisResourceModel.h"
#include "KisTag.h"
#include "KisTagModel.h"
#include "KisTagResourceModel.h"

#include "kritaresources_export.h"

/**
 * @brief The KisTagFilterResourceProxyModel class filters the resources by tag or resource name
 */
class KRITARESOURCES_EXPORT KisTagFilterResourceProxyModel
    : public QSortFilterProxyModel
    , public KisAbstractResourceModel
    , public KisAbstractResourceFilterInterface
{
    Q_OBJECT
public:

    KisTagFilterResourceProxyModel(const QString &resourceType, QObject *parent = 0);
    ~KisTagFilterResourceProxyModel() override;

    void setResourceFilter(ResourceFilter filter) override;
    void setStorageFilter(StorageFilter filter) override;

    // To be used if we need an extra proxy model, like for
    void setResourceModel(KisResourceModel *resourceModel);

    // KisAbstractResourceModel interface

    KoResourceSP resourceForIndex(QModelIndex index = QModelIndex()) const override;
    QModelIndex indexForResource(KoResourceSP resource) const override;
    QModelIndex indexForResourceId(int resourceId) const override;
    bool setResourceActive(const QModelIndex &index, bool value) override;
    KoResourceSP importResourceFile(const QString &filename, const bool allowOverwrite, const QString &storageId = QString()) override;
    KoResourceSP importResource(const QString &filename, QIODevice *device, const bool allowOverwrite, const QString &storageId = QString()) override;
    bool importWillOverwriteResource(const QString &fileName, const QString &storageLocation) const override;
    bool exportResource(KoResourceSP resource, QIODevice *device) override;
    bool addResource(KoResourceSP resource, const QString &storageId = QString()) override;
    bool updateResource(KoResourceSP resource) override;
    bool reloadResource(KoResourceSP resource) override;
    bool renameResource(KoResourceSP resource, const QString &name) override;
    bool setResourceMetaData(KoResourceSP resource, QMap<QString, QVariant> metadata) override;


    /**
     * @brief setMetaDataFilter provides a set of metadata to filter on, for instance
     * by paintop id category.
     * @param metaDataMap
     */
    void setMetaDataFilter(QMap<QString, QVariant> metaDataMap);

    /**
     * @brief setTagFilter sets the tag to filter with
     * @param tag a valid tag with a valid id, or 0 to clear the filter
     */
    void setTagFilter(const KisTagSP tag);


    void setStorageFilter(bool useFilter, int storageId);

    /**
     * @brief setResourceFilter sets the resource to filter with
     * @param resource a valid resource with a valid id, or 0 to clear the filter
     */
    void setResourceFilter(const KoResourceSP resource);

    void setSearchText(const QString& seatchText);

    void setFilterInCurrentTag(bool filterInCurrentTag);

    bool tagResources(const KisTagSP tag, const QVector<int> &resourceIds);
    bool untagResources(const KisTagSP tag, const QVector<int> &resourceIds);
    int isResourceTagged(const KisTagSP tag, const int resourceId);

Q_SIGNALS:

    void beforeFilterChanges();
    void afterFilterChanged();

protected:

    bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    void updateTagFilter();

private:
    struct Private;
    Private *const d;

    Q_DISABLE_COPY(KisTagFilterResourceProxyModel)
};

#endif // KISTAGFILTERRESOURCEPROXYMODEL_H
