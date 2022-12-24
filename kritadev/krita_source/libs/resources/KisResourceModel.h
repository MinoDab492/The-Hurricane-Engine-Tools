/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISRESOURCEMODEL_H
#define KISRESOURCEMODEL_H

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

#include <kritaresources_export.h>

#include <KoResource.h>
#include <KisTag.h>


/**
 * KisAbstractResourceModel defines the interface for accessing resources
 * that is used in KisResourceModel and the various filter/proxy models
 */
class KRITARESOURCES_EXPORT KisAbstractResourceModel {

public:

    /**
     * @brief The Columns enum indexes the columns in the model. To get
     * the thumbnail for a particular resource, create the index with
     * QModelIndex(row, Thumbnail).
     */
    enum Columns {
        Id = 0,
        StorageId,
        Name,
        Filename,
        Tooltip,
        Thumbnail,
        Status,
        Location,
        ResourceType,
        Tags,
        MD5,
        /// A larger thumbnail for displaying in a tooltip. 200x200 or so.
        LargeThumbnail,
        /// A dirty resource is one that has been modified locally but not saved
        Dirty,
        /// MetaData is a map of key, value pairs that is associated with this resource
        MetaData,
        /// Whether the current resource is active
        ResourceActive,
        /// Whether the current resource's storage is active
        StorageActive,
    };

    virtual ~KisAbstractResourceModel(){}

    /**
     * @brief resourceForIndex returns a properly versioned and id'ed resource object
     */
    virtual KoResourceSP resourceForIndex(QModelIndex index = QModelIndex()) const = 0;

    /**
     * @brief indexFromResource
     * @param resource
     * @return
     */
    virtual QModelIndex indexForResource(KoResourceSP resource) const = 0;

    /**
     * @brief indexFromResource
     * @param resourceId resource id for which we want to get an index
     * @return
     */
    virtual QModelIndex indexForResourceId(int resourceId) const = 0;

    /**
     * @brief setResourceActive changes 'active' state of the resource
     * @param index the index of the resource
     * @param value new 'active' state of the resource
     * @return true if the new state has been assigned successfully
     */
    virtual bool setResourceActive(const QModelIndex &index, bool value) = 0;

    /**
     * A convenience function to put a resource into inactive state
     */
    inline bool setResourceInactive(const QModelIndex &index) {
        return setResourceActive(index, false);
    }

    /**
     * @brief importResourceFile
     * @param filename
     * @return
     */
    virtual KoResourceSP importResourceFile(const QString &filename, const bool allowOverwrite, const QString &storageId = QString("")) = 0;

    /**
     * @brief importResource imports a resource from a QIODevice
     *
     * Importing a resource from a binary blob is the only way to guarantee that its
     * MD5 checksum is kept persistent. The underlying storage will just copy bytes
     * into its location.
     *
     * @param filename file name of the resource if preset. File name may be used
     *                 for addressing the resource, so it is usually preferred to
     *                 preserve it.
     *
     * @param device device where the resource should be read from
     *
     * @return the loaded resource object
     */
    virtual KoResourceSP importResource(const QString &filename, QIODevice *device, const bool allowOverwrite, const QString &storageId = QString("")) = 0;

    /**
     * @brief importWillOverwriteResource checks is importing a resource with this filename will overwrite anything
     *
     * If this funciton returns true, then importResource() is guaranteed to
     * fail with 'allowOverwrite' set to false.
     *
     * @param filename file name of the resource if preset. File name may be used
     *                 for addressing the resource, so it is usually preferred to
     *                 preserve it.
     *
     * @return true if the some existing will be overwritten while importing
     */
    virtual bool importWillOverwriteResource(const QString &fileName, const QString &storageLocation = QString()) const = 0;

    /**
     * @brief exportResource exports a resource into a QIODevice
     *
     * Exporting a resource as a binary blob is the only way to guarantee that its
     * MD5 checksum is kept persistent. The underlying storage will just copy bytes
     * into the device without doing any conversions
     *
     * @param resource the resource to be exported
     *
     * @param device device where the resource should be written to
     *
     * @return true if export operation has been successful
     */
    virtual bool exportResource(KoResourceSP resource, QIODevice *device) = 0;

    /**
     * @brief addResource adds the given resource to the database and storage. If the resource
     * already exists in the given storage with md5, filename or name, the existing resource
     * will be updated instead. If the existing resource was inactive, it will be actived
     * (undeleted).
     *
     * @param resource the resource itself
     * @param storageId the id of the storage (could be "memory" for temporary
     * resources, the document's storage id for document storages or empty to save
     * to the default resources folder
     * @return true if adding the resource succeeded.
     */
    virtual bool addResource(KoResourceSP resource, const QString &storageId = QString("")) = 0;

    /**
     * @brief updateResource creates a new version ofthe resource in the storage and
     * in the database. This will also set the resource to active if it was inactive.
     *
     * Note: if the storage does not support versioning, updating the resource will fail.
     *
     * @param resource
     * @return true if the resource was succesfull updated,
     */
    virtual bool updateResource(KoResourceSP resource) = 0;

    /**
     * @brief reloadResource
     * @param resource
     * @return
     */
    virtual bool reloadResource(KoResourceSP resource) = 0;

    /**
     * @brief renameResource name the given resource. The resource will have its
     * name field reset, will be saved to the storage and there will be a new
     * version created in the database.
     * @param resource The resource to rename
     * @param name The new name
     * @return true if the operation succeeded.
     */
    virtual bool renameResource(KoResourceSP resource, const QString &name) = 0;

    /**
     * @brief setResourceMetaData
     * @param metadata
     * @return
     */
    virtual bool setResourceMetaData(KoResourceSP resource, QMap<QString, QVariant> metadata) = 0;
};

class KRITARESOURCES_EXPORT KisAbstractResourceFilterInterface
{
public:
    virtual ~KisAbstractResourceFilterInterface() {}

    enum ResourceFilter {
        ShowInactiveResources = 0,
        ShowActiveResources,
        ShowAllResources
    };

    enum StorageFilter {
        ShowInactiveStorages = 0,
        ShowActiveStorages,
        ShowAllStorages
    };
public:

    /**
     * Select status of the resources that should be shown
     */
    virtual void setResourceFilter(ResourceFilter filter) = 0;

    /**
     * Select status of the storages that should be shown
     */
    virtual void setStorageFilter(StorageFilter filter) = 0;
};

/**
 * @brief The KisAllresourcesModel class provides access to the cache database
 * for a particular resource type. Instances should be retrieved using
 * KisResourceModelProvider. All resources are part of this model, active and
 * inactive, from all storages, active and inactive.
 */
class KRITARESOURCES_EXPORT KisAllResourcesModel : public QAbstractTableModel, public KisAbstractResourceModel
{
    Q_OBJECT

private:
    friend class KisResourceModelProvider;
    friend class KisResourceModel;
    friend class KisResourceQueryMapper;
    KisAllResourcesModel(const QString &resourceType, QObject *parent = 0);

public:

    ~KisAllResourcesModel() override;

// QAbstractItemModel API

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

// Resources API

    KoResourceSP resourceForIndex(QModelIndex index = QModelIndex()) const override;
    QModelIndex indexForResource(KoResourceSP resource) const override;
    QModelIndex indexForResourceId(int resourceId) const override;
    bool setResourceActive(const QModelIndex &index, bool value) override;
    KoResourceSP importResourceFile(const QString &filename, const bool allowOverwrite, const QString &storageId = QString("")) override;
    KoResourceSP importResource(const QString &filename, QIODevice *device, const bool allowOverwrite, const QString &storageId = QString("")) override;
    bool importWillOverwriteResource(const QString &fileName, const QString &storageLocation = QString()) const override;
    bool exportResource(KoResourceSP resource, QIODevice *device) override;
    bool addResource(KoResourceSP resource, const QString &storageId = QString("")) override;
    bool updateResource(KoResourceSP resource) override;
    bool reloadResource(KoResourceSP resource) override;
    bool renameResource(KoResourceSP resource, const QString &name) override;
    bool setResourceMetaData(KoResourceSP resource, QMap<QString, QVariant> metadata) override;

private Q_SLOTS:

    void storageActiveStateChanged(const QString &location);

    /**
     * A special connection for KisResourceLocator, which can import
     * a resource on its own (all other places are supposed to do that
     * via KisResourceModel). This call is needed to make sure the
     * internal query in the model is reset.
     *
     * WARNING: the resource is expected to be added to the **end**
     * of the model, that is, its resourceId is expected to be greater
     * than any existing resource.
     */
    void beginExternalResourceImport(const QString &resourceType, int numResources);

    /**
     * \see beginExternalResourceImport
     */
    void endExternalResourceImport(const QString &resourceType);

    /**
     * A special connection for KisResourceLocator, which can remove the resource
     * while importing something with overwrite. In such a case the locator will
     * emit both, remove and insert signals for both the resources.
     */
    void beginExternalResourceRemove(const QString &resourceType, const QVector<int> &resourceId);

    /**
     * \see beginExternalResourceRemove
     */
    void endExternalResourceRemove(const QString &resourceType);

    /**
     * A special connection for KisResourceLocator, which is triggered when the
     * resource changes its 'active' state
     */
    void slotResourceActiveStateChanged(const QString &resourceType, int resourceId);

public:

    KoResourceSP resourceForId(int id) const;

    /**
     * @brief resourceExists checks whether there is a resource with, in order,
     * the given md5, the filename or the resource name.
     */
    bool resourceExists(const QString &md5, const QString &filename, const QString &name);

    /**
     * resourceForFilename returns the first resource with the given filename that
     * is active and is in an active store. Note that the filename does not include
     * a path to the storage, and if there are resources with the same filename
     * in several active storages, only one resource is returned.
     *
     * @param filename the filename we're looking for
     * @param checkDependentResources: check whether we should try to find a resource embedded
     * in a resource that's not been loaded yet in the metadata table.
     * @return a resource if one is found, or 0 if none are found
     */
    QVector<KoResourceSP> resourcesForFilename(QString filename) const;

    /**
     * resourceForName returns the first resource with the given name that
     * is active and is in an active store. Note that if there are resources
     * with the same name in several active storages, only one resource
     * is returned.
     *
     * @return a resource if one is found, or 0 if none are found
     */
    QVector<KoResourceSP> resourcesForName(const QString &name) const;
    QVector<KoResourceSP> resourcesForMD5(const QString &md5sum) const;
    QVector<KisTagSP> tagsForResource(int resourceId) const;

private:

    bool resetQuery();

    struct Private;
    Private *const d;

};

/**
 * @brief The KisResourceModel class provides the main access to resources. It is possible
 * to filter the resources returned by the active status flag of the resources and the
 * storages
 */
class KRITARESOURCES_EXPORT KisResourceModel : public QSortFilterProxyModel, public KisAbstractResourceModel, public KisAbstractResourceFilterInterface
{
    Q_OBJECT

public:

    KisResourceModel(const QString &type, QObject *parent = 0);
    ~KisResourceModel() override;


    void setResourceFilter(ResourceFilter filter) override;
    void setStorageFilter(StorageFilter filter) override;

    void showOnlyUntaggedResources(bool showOnlyUntagged);

public:

    KoResourceSP resourceForIndex(QModelIndex index = QModelIndex()) const override;
    QModelIndex indexForResource(KoResourceSP resource) const override;
    QModelIndex indexForResourceId(int resourceId) const override;
    bool setResourceActive(const QModelIndex &index, bool value) override;
    KoResourceSP importResourceFile(const QString &filename, const bool allowOverwrite, const QString &storageId = QString("")) override;
    KoResourceSP importResource(const QString &filename, QIODevice *device, const bool allowOverwrite, const QString &storageId = QString("")) override;
    bool importWillOverwriteResource(const QString &fileName, const QString &storageLocation = QString()) const override;
    bool exportResource(KoResourceSP resource, QIODevice *device) override;
    bool addResource(KoResourceSP resource, const QString &storageId = QString("")) override;
    bool updateResource(KoResourceSP resource) override;
    bool reloadResource(KoResourceSP resource) override;
    bool renameResource(KoResourceSP resource, const QString &name) override;
    bool setResourceMetaData(KoResourceSP resource, QMap<QString, QVariant> metadata) override;

public:

    KoResourceSP resourceForId(int id) const;

    /**
     * resourceForFilename returns the resources with the given filename that
     * fit the current filters. Note that the filename does not include
     * a path to the storage.
     *
     * @return a resource if one is found, or 0 if none are found
     */
    QVector<KoResourceSP> resourcesForFilename(QString fileName) const;

    /**
     * resourceForName returns the resources with the given name that
     * fit the current filters.
     *
     * @return a resource if one is found, or 0 if none are found
     */
    QVector<KoResourceSP> resourcesForName(QString name) const;
    QVector<KoResourceSP> resourcesForMD5(const QString md5sum) const;
    QVector<KisTagSP> tagsForResource(int resourceId) const;

protected:

    bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    QVector<KoResourceSP> filterByColumn(const QString filter, KisAllResourcesModel::Columns column) const;
    bool filterResource(const QModelIndex &idx) const;

    struct Private;
    Private *const d;

    Q_DISABLE_COPY(KisResourceModel)

};



#endif // KISRESOURCEMODEL_H
