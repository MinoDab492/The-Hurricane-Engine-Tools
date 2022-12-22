/* This file is part of the Krita project
 *
 * SPDX-FileCopyrightText: 2014 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisMainWindow.h" // XXX: remove
#include <QMessageBox> // XXX: remove

#include <KisMimeDatabase.h>

#include <KoCanvasBase.h>
#include <KoColor.h>
#include <KoColorProfile.h>
#include <KoColorSpaceEngine.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoDocumentInfoDlg.h>
#include <KoDocumentInfo.h>
#include <KoUnit.h>
#include <KoID.h>
#include <KoProgressProxy.h>
#include <KoProgressUpdater.h>
#include <KoSelection.h>
#include <KoShape.h>
#include <KoShapeController.h>
#include <KoStore.h>
#include <KoUpdater.h>
#include <KoXmlWriter.h>
#include <KoStoreDevice.h>
#include <KoDialog.h>
#include <KisImportExportErrorCode.h>
#include <KoDocumentResourceManager.h>
#include <KoMD5Generator.h>
#include <KisResourceStorage.h>
#include <KisResourceLocator.h>
#include <KisResourceTypes.h>
#include <KisGlobalResourcesInterface.h>
#include <KisResourceLoaderRegistry.h>
#include <KisResourceModelProvider.h>
#include <KisResourceCacheDb.h>
#include <KoEmbeddedResource.h>
#include <KisUsageLogger.h>
#include <klocalizedstring.h>
#include "kis_scratch_pad.h"
#include <kis_debug.h>
#include <kis_generator_layer.h>
#include <kis_generator_registry.h>
#include <KisAutoSaveRecoveryDialog.h>
#include <kdesktopfile.h>
#include <kconfiggroup.h>
#include <kbackup.h>
#include <KisView.h>

#include <QTextBrowser>
#include <QApplication>
#include <QBuffer>
#include <QStandardPaths>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QRect>
#include <QScopedPointer>
#include <QSize>
#include <QStringList>
#include <QtGlobal>
#include <QTimer>
#include <QWidget>
#include <QFuture>
#include <QFutureWatcher>
#include <QUuid>

// Krita Image
#include <kis_image_animation_interface.h>
#include <kis_config.h>
#include <flake/kis_shape_layer.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_name_server.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <kis_selection.h>
#include <kis_fill_painter.h>
#include <kis_document_undo_store.h>
#include <kis_idle_watcher.h>
#include <kis_signal_auto_connection.h>
#include <kis_canvas_widget_base.h>
#include "kis_layer_utils.h"
#include "kis_selection_mask.h"

// Local
#include "KisViewManager.h"
#include "kis_clipboard.h"
#include "widgets/kis_custom_image_widget.h"
#include "canvas/kis_canvas2.h"
#include "flake/kis_shape_controller.h"
#include "widgets/kis_progress_widget.h"
#include "kis_canvas_resource_provider.h"
#include "KisResourceServerProvider.h"
#include "kis_node_manager.h"
#include "KisPart.h"
#include "KisApplication.h"
#include "KisDocument.h"
#include "KisImportExportManager.h"
#include "KisView.h"
#include "kis_grid_config.h"
#include "kis_guides_config.h"
#include "kis_image_barrier_lock_adapter.h"
#include "KisReferenceImagesLayer.h"
#include "dialogs/KisRecoverNamedAutosaveDialog.h"


#include <mutex>
#include "kis_config_notifier.h"
#include "kis_async_action_feedback.h"
#include "KisCloneDocumentStroke.h"

#include <kis_algebra_2d.h>
#include <KisMirrorAxisConfig.h>
#include <KisDecorationsWrapperLayer.h>
#include "kis_simple_stroke_strategy.h"

// Define the protocol used here for embedded documents' URL
// This used to "store" but QUrl didn't like it,
// so let's simply make it "tar" !
#define STORE_PROTOCOL "tar"
// The internal path is a hack to make QUrl happy and for document children
#define INTERNAL_PROTOCOL "intern"
#define INTERNAL_PREFIX "intern:/"
// Warning, keep it sync in koStore.cc

#include <unistd.h>

using namespace std;

namespace {
constexpr int errorMessageTimeout = 5000;
constexpr int successMessageTimeout = 1000;
}


/**********************************************************
 *
 * KisDocument
 *
 **********************************************************/

//static
QString KisDocument::newObjectName()
{
    static int s_docIFNumber = 0;
    QString name; name.setNum(s_docIFNumber++); name.prepend("document_");
    return name;
}


class UndoStack : public KUndo2Stack
{
public:
    UndoStack(KisDocument *doc)
        : KUndo2Stack(doc),
          m_doc(doc)
    {
    }

    void setIndex(int idx) override {
        m_postponedJobs.append({PostponedJob::SetIndex, idx});
        processPostponedJobs();
    }

    void notifySetIndexChangedOneCommand() override {
        KisImageWSP image = this->image();
        image->unlock();

        /**
         * Some very weird commands may emit blocking signals to
         * the GUI (e.g. KisGuiContextCommand). Here is the best thing
         * we can do to avoid the deadlock
         */
        while(!image->tryBarrierLock()) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    void undo() override {
        m_postponedJobs.append({PostponedJob::Undo, 0});
        processPostponedJobs();
    }


    void redo() override {
        m_postponedJobs.append({PostponedJob::Redo, 0});
        processPostponedJobs();
    }

private:
    KisImageWSP image() {
        KisImageWSP currentImage = m_doc->image();
        Q_ASSERT(currentImage);
        return currentImage;
    }

    void setIndexImpl(int idx) {
        KisImageWSP image = this->image();
        image->requestStrokeCancellation();
        if(image->tryBarrierLock()) {
            KUndo2Stack::setIndex(idx);
            image->unlock();
        }
    }

    void undoImpl() {
        KisImageWSP image = this->image();
        image->requestUndoDuringStroke();

        if (image->tryUndoUnfinishedLod0Stroke() == UNDO_OK) {
            return;
        }

        if(image->tryBarrierLock()) {
            KUndo2Stack::undo();
            image->unlock();
        }
    }

    void redoImpl() {
        KisImageWSP image = this->image();
        image->requestRedoDuringStroke();

        if(image->tryBarrierLock()) {
            KUndo2Stack::redo();
            image->unlock();
        }
    }

    void processPostponedJobs() {
        /**
         * Some undo commands may call QApplication::processEvents(),
         * see notifySetIndexChangedOneCommand(). That may cause
         * recursive calls to the undo stack methods when used from
         * the Undo History docker. Here we try to handle that gracefully
         * by accumulating all the requests and executing them at the
         * topmost level of recursion.
         */
        if (m_recursionCounter > 0) return;

        m_recursionCounter++;

        while (!m_postponedJobs.isEmpty()) {
            PostponedJob job = m_postponedJobs.dequeue();
            switch (job.type) {
            case PostponedJob::SetIndex:
                setIndexImpl(job.index);
                break;
            case PostponedJob::Redo:
                redoImpl();
                break;
            case PostponedJob::Undo:
                undoImpl();
                break;
            }
        }

        m_recursionCounter--;
    }

private:
    int m_recursionCounter = 0;

    struct PostponedJob {
        enum Type {
            Undo = 0,
            Redo,
            SetIndex
        };
        Type type = Undo;
        int index = 0;
    };
    QQueue<PostponedJob> m_postponedJobs;

    KisDocument *m_doc;
};

class Q_DECL_HIDDEN KisDocument::Private
{
public:
    Private(KisDocument *_q)
        : q(_q)
        , docInfo(new KoDocumentInfo(_q)) // deleted by QObject
        , importExportManager(new KisImportExportManager(_q)) // deleted manually
        , autoSaveTimer(new QTimer(_q))
        , undoStack(new UndoStack(_q)) // deleted by QObject
        , m_bAutoDetectedMime(false)
        , modified(false)
        , readwrite(true)
        , firstMod(QDateTime::currentDateTime())
        , lastMod(firstMod)
        , nserver(new KisNameServer(1))
        , imageIdleWatcher(2000 /*ms*/)
        , globalAssistantsColor(KisConfig(true).defaultAssistantsColor())
        , batchMode(false)
    {
        if (QLocale().measurementSystem() == QLocale::ImperialSystem) {
            unit = KoUnit::Inch;
        } else {
            unit = KoUnit::Centimeter;
        }
        connect(&imageIdleWatcher, SIGNAL(startedIdleMode()), q, SLOT(slotPerformIdleRoutines()));
    }

    Private(const Private &rhs, KisDocument *_q)
        : q(_q)
        , docInfo(new KoDocumentInfo(*rhs.docInfo, _q))
        , importExportManager(new KisImportExportManager(_q))
        , autoSaveTimer(new QTimer(_q))
        , undoStack(new UndoStack(_q))
        , nserver(new KisNameServer(*rhs.nserver))
        , preActivatedNode(0) // the node is from another hierarchy!
        , imageIdleWatcher(2000 /*ms*/)
    {
        copyFromImpl(rhs, _q, CONSTRUCT);
        connect(&imageIdleWatcher, SIGNAL(startedIdleMode()), q, SLOT(slotPerformIdleRoutines()));
    }

    ~Private() {
        // Don't delete m_d->shapeController because it's in a QObject hierarchy.
        delete nserver;
    }

    KisDocument *q = 0;
    KoDocumentInfo *docInfo = 0;

    KoUnit unit;

    KisImportExportManager *importExportManager = 0; // The filter-manager to use when loading/saving [for the options]

    QByteArray mimeType; // The actual mimeType of the document
    QByteArray outputMimeType; // The mimeType to use when saving

    QTimer *autoSaveTimer;
    QString lastErrorMessage; // see openFile()
    QString lastWarningMessage;
    int autoSaveDelay = 300; // in seconds, 0 to disable.
    bool modifiedAfterAutosave = false;
    bool isAutosaving = false;
    bool disregardAutosaveFailure = false;
    int autoSaveFailureCount = 0;

    KUndo2Stack *undoStack = 0;

    KisGuidesConfig guidesConfig;
    KisMirrorAxisConfig mirrorAxisConfig;

    bool m_bAutoDetectedMime = false; // whether the mimeType in the arguments was detected by the part itself
    QString m_path; // local url - the one displayed to the user.
    QString m_file; // Local file - the only one the part implementation should deal with.

    QMutex savingMutex;

    bool modified = false;
    bool readwrite = false;

    QDateTime firstMod;
    QDateTime lastMod;

    KisNameServer *nserver;

    KisImageSP image;
    KisImageSP savingImage;

    KisNodeWSP preActivatedNode;
    KisShapeController* shapeController = 0;
    KoShapeController* koShapeController = 0;
    KisIdleWatcher imageIdleWatcher;
    QScopedPointer<KisSignalAutoConnection> imageIdleConnection;

    QList<KisPaintingAssistantSP> assistants;

    StoryboardItemList m_storyboardItemList;
    QVector<StoryboardComment> m_storyboardCommentList;

    QColor globalAssistantsColor;

    KisGridConfig gridConfig;

    bool imageModifiedWithoutUndo = false;
    bool modifiedWhileSaving = false;
    QScopedPointer<KisDocument> backgroundSaveDocument;
    QPointer<KoUpdater> savingUpdater;
    QFuture<KisImportExportErrorCode> childSavingFuture;
    KritaUtils::ExportFileJob backgroundSaveJob;
    KisSignalAutoConnectionsStore referenceLayerConnections;

    bool isRecovered = false;

    bool batchMode { false };
    bool decorationsSyncingDisabled = false;
    bool wasStorageAdded = false;


    // Resources saved in the .kra document
    QString linkedResourcesStorageID;
    KisResourceStorageSP linkedResourceStorage;

    // Resources saved into other components of the kra file
    QString embeddedResourcesStorageID;
    KisResourceStorageSP embeddedResourceStorage;

    void syncDecorationsWrapperLayerState();

    void setImageAndInitIdleWatcher(KisImageSP _image) {
        image = _image;

        imageIdleWatcher.setTrackedImage(image);
    }

    void copyFrom(const Private &rhs, KisDocument *q);
    void copyFromImpl(const Private &rhs, KisDocument *q, KisDocument::CopyPolicy policy);

    void uploadLinkedResourcesFromLayersToStorage();
    KisDocument* lockAndCloneImpl(bool fetchResourcesFromLayers);

    void updateDocumentMetadataOnSaving(const QString &filePath, const QByteArray &mimeType);

    /// clones the palette list oldList
    /// the ownership of the returned KoColorSet * belongs to the caller
    class StrippedSafeSavingLocker;
};


void KisDocument::Private::syncDecorationsWrapperLayerState()
{
    if (!this->image || this->decorationsSyncingDisabled) return;

    KisImageSP image = this->image;
    KisDecorationsWrapperLayerSP decorationsLayer =
            KisLayerUtils::findNodeByType<KisDecorationsWrapperLayer>(image->root());

    const bool needsDecorationsWrapper =
            gridConfig.showGrid() || (guidesConfig.showGuides() && guidesConfig.hasGuides()) || !assistants.isEmpty();

    struct SyncDecorationsWrapperStroke : public KisSimpleStrokeStrategy {
        SyncDecorationsWrapperStroke(KisDocument *document, bool needsDecorationsWrapper)
            : KisSimpleStrokeStrategy(QLatin1String("sync-decorations-wrapper"),
                                      kundo2_noi18n("start-isolated-mode")),
              m_document(document),
              m_needsDecorationsWrapper(needsDecorationsWrapper)
        {
            this->enableJob(JOB_INIT, true, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
            setClearsRedoOnStart(false);
            setRequestsOtherStrokesToEnd(false);
        }

        void initStrokeCallback() override {
            KisDecorationsWrapperLayerSP decorationsLayer =
                    KisLayerUtils::findNodeByType<KisDecorationsWrapperLayer>(m_document->image()->root());

            if (m_needsDecorationsWrapper && !decorationsLayer) {
                m_document->image()->addNode(new KisDecorationsWrapperLayer(m_document));
            } else if (!m_needsDecorationsWrapper && decorationsLayer) {
                m_document->image()->removeNode(decorationsLayer);
            }
        }

    private:
        KisDocument *m_document = 0;
        bool m_needsDecorationsWrapper = false;
    };

    KisStrokeId id = image->startStroke(new SyncDecorationsWrapperStroke(q, needsDecorationsWrapper));
    image->endStroke(id);
}

void KisDocument::Private::copyFrom(const Private &rhs, KisDocument *q)
{
    copyFromImpl(rhs, q, KisDocument::REPLACE);
}

void KisDocument::Private::copyFromImpl(const Private &rhs, KisDocument *q, KisDocument::CopyPolicy policy)
{
    if (policy == REPLACE) {
        delete docInfo;
    }
    docInfo = (new KoDocumentInfo(*rhs.docInfo, q));
    unit = rhs.unit;
    mimeType = rhs.mimeType;
    outputMimeType = rhs.outputMimeType;

    if (policy == REPLACE) {
        q->setGuidesConfig(rhs.guidesConfig);
        q->setMirrorAxisConfig(rhs.mirrorAxisConfig);
        q->setModified(rhs.modified);
        q->setAssistants(KisPaintingAssistant::cloneAssistantList(rhs.assistants));
        q->setStoryboardItemList(StoryboardItem::cloneStoryboardItemList(rhs.m_storyboardItemList));
        q->setStoryboardCommentList(rhs.m_storyboardCommentList);
        q->setGridConfig(rhs.gridConfig);
    } else {
        // in CONSTRUCT mode, we cannot use the functions of KisDocument
        // because KisDocument does not yet have a pointer to us.
        guidesConfig = rhs.guidesConfig;
        mirrorAxisConfig = rhs.mirrorAxisConfig;
        modified = rhs.modified;
        assistants = KisPaintingAssistant::cloneAssistantList(rhs.assistants);
        m_storyboardItemList = StoryboardItem::cloneStoryboardItemList(rhs.m_storyboardItemList);
        m_storyboardCommentList = rhs.m_storyboardCommentList;
        gridConfig = rhs.gridConfig;
    }
    imageModifiedWithoutUndo = rhs.imageModifiedWithoutUndo;
    m_bAutoDetectedMime = rhs.m_bAutoDetectedMime;
    m_path = rhs.m_path;
    m_file = rhs.m_file;
    readwrite = rhs.readwrite;
    firstMod = rhs.firstMod;
    lastMod = rhs.lastMod;
    // XXX: the display properties will be shared between different snapshots
    globalAssistantsColor = rhs.globalAssistantsColor;
    batchMode = rhs.batchMode;


    if (rhs.linkedResourceStorage) {
        linkedResourceStorage = rhs.linkedResourceStorage->clone();
    }

    if (rhs.embeddedResourceStorage) {
        embeddedResourceStorage = rhs.embeddedResourceStorage->clone();
    }

}

class KisDocument::Private::StrippedSafeSavingLocker {
public:
    StrippedSafeSavingLocker(QMutex *savingMutex, KisImageSP image)
        : m_locked(false)
        , m_image(image)
        , m_savingLock(savingMutex)
        , m_imageLock(image, true)

    {
        /**
         * Initial try to lock both objects. Locking the image guards
         * us from any image composition threads running in the
         * background, while savingMutex guards us from entering the
         * saving code twice by autosave and main threads.
         *
         * Since we are trying to lock multiple objects, so we should
         * do it in a safe manner.
         */
        m_locked = std::try_lock(m_imageLock, m_savingLock) < 0;

        if (!m_locked) {
            m_image->requestStrokeEnd();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            // one more try...
            m_locked = std::try_lock(m_imageLock, m_savingLock) < 0;
        }
    }

    ~StrippedSafeSavingLocker() {
        if (m_locked) {
            m_imageLock.unlock();
            m_savingLock.unlock();
        }
    }

    bool successfullyLocked() const {
        return m_locked;
    }

private:
    bool m_locked;
    KisImageSP m_image;
    StdLockableWrapper<QMutex> m_savingLock;
    KisImageBarrierLockAdapter m_imageLock;
};

KisDocument::KisDocument(bool addStorage)
    : d(new Private(this))
{
    connect(KisConfigNotifier::instance(), SIGNAL(configChanged()), SLOT(slotConfigChanged()));
    connect(d->undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(slotUndoStackCleanChanged(bool)));
    connect(d->autoSaveTimer, SIGNAL(timeout()), this, SLOT(slotAutoSave()));
    setObjectName(newObjectName());


    if (addStorage) {
        d->linkedResourcesStorageID = QUuid::createUuid().toString();
        d->linkedResourceStorage.reset(new KisResourceStorage(d->linkedResourcesStorageID));
        KisResourceLocator::instance()->addStorage(d->linkedResourcesStorageID, d->linkedResourceStorage);

        d->embeddedResourcesStorageID = QUuid::createUuid().toString();
        d->embeddedResourceStorage.reset(new KisResourceStorage(d->embeddedResourcesStorageID));
        KisResourceLocator::instance()->addStorage(d->embeddedResourcesStorageID, d->embeddedResourceStorage);

        d->wasStorageAdded = true;
    }

    // preload the krita resources
    KisResourceServerProvider::instance();

    d->shapeController = new KisShapeController(d->nserver, d->undoStack, this);
    d->koShapeController = new KoShapeController(0, d->shapeController);
    d->shapeController->resourceManager()->setGlobalShapeController(d->koShapeController);

    slotConfigChanged();
}

KisDocument::KisDocument(const KisDocument &rhs, bool addStorage)
    : QObject(),
      d(new Private(*rhs.d, this))
{
    copyFromDocumentImpl(rhs, CONSTRUCT);

    if (addStorage) {
        KisResourceLocator::instance()->addStorage(d->linkedResourcesStorageID, d->linkedResourceStorage);
        KisResourceLocator::instance()->addStorage(d->embeddedResourcesStorageID, d->embeddedResourceStorage);
        d->wasStorageAdded = true;
    }
}

KisDocument::~KisDocument()
{
    // wait until all the pending operations are in progress
    waitForSavingToComplete();
    d->imageIdleWatcher.setTrackedImage(0);

    /**
     * Push a timebomb, which will try to release the memory after
     * the document has been deleted
     */
    KisPaintDevice::createMemoryReleaseObject()->deleteLater();

    d->autoSaveTimer->disconnect(this);
    d->autoSaveTimer->stop();

    delete d->importExportManager;

    // Despite being QObject they needs to be deleted before the image
    delete d->shapeController;

    delete d->koShapeController;

    if (d->image) {
        d->image->notifyAboutToBeDeleted();

        /**
         * WARNING: We should wait for all the internal image jobs to
         * finish before entering KisImage's destructor. The problem is,
         * while execution of KisImage::~KisImage, all the weak shared
         * pointers pointing to the image enter an inconsistent
         * state(!). The shared counter is already zero and destruction
         * has started, but the weak reference doesn't know about it,
         * because KisShared::~KisShared hasn't been executed yet. So all
         * the threads running in background and having weak pointers will
         * enter the KisImage's destructor as well.
         */

        d->image->requestStrokeCancellation();
        d->image->waitForDone();

        // clear undo commands that can still point to the image
        d->undoStack->clear();
        d->image->waitForDone();

        KisImageWSP sanityCheckPointer = d->image;
        Q_UNUSED(sanityCheckPointer);

        // The following line trigger the deletion of the image
        d->image.clear();

        // check if the image has actually been deleted
        KIS_SAFE_ASSERT_RECOVER_NOOP(!sanityCheckPointer.isValid());
    }
    if (d->wasStorageAdded) {
        if (KisResourceLocator::instance()->hasStorage(d->linkedResourcesStorageID)) {
            KisResourceLocator::instance()->removeStorage(d->linkedResourcesStorageID);
        }
        if (KisResourceLocator::instance()->hasStorage(d->embeddedResourcesStorageID)) {
            KisResourceLocator::instance()->removeStorage(d->embeddedResourcesStorageID);
        }
    }

    delete d;
}

QString KisDocument::embeddedResourcesStorageId() const
{
    return d->embeddedResourcesStorageID;
}

QString KisDocument::linkedResourcesStorageId() const
{
    return d->linkedResourcesStorageID;
}

KisDocument *KisDocument::clone(bool addStorage)
{
    return new KisDocument(*this, addStorage);
}

bool KisDocument::exportDocumentImpl(const KritaUtils::ExportFileJob &job, KisPropertiesConfigurationSP exportConfiguration, bool isAdvancedExporting)
{
    QFileInfo filePathInfo(job.filePath);

    if (filePathInfo.exists() && !filePathInfo.isWritable()) {
        slotCompleteSavingDocument(job, ImportExportCodes::NoAccessToWrite,
                                   i18n("%1 cannot be written to. Please save under a different name.", job.filePath),
                                   "");
        return false;
    }

    KisConfig cfg(true);
    if (cfg.backupFile() && filePathInfo.exists()) {

        QString backupDir;

        switch(cfg.readEntry<int>("backupfilelocation", 0)) {
        case 1:
            backupDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
            break;
        case 2:
            backupDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            break;
        default:
#ifdef Q_OS_ANDROID
            // We deal with URIs, there may or may not be a "directory"
            backupDir = KisAutoSaveRecoveryDialog::autoSaveLocation();
            QDir().mkpath(backupDir);
#endif

            // Do nothing: the empty string is user file location
            break;
        }

        int numOfBackupsKept = cfg.readEntry<int>("numberofbackupfiles", 1);
        QString suffix = cfg.readEntry<QString>("backupfilesuffix", "~");

        if (numOfBackupsKept == 1) {
            if (!KBackup::simpleBackupFile(job.filePath, backupDir, suffix)) {
                qWarning() << "Failed to create simple backup file!" << job.filePath << backupDir << suffix;
                KisUsageLogger::log(QString("Failed to create a simple backup for %1 in %2.").arg(job.filePath).arg(backupDir.isEmpty() ? "the same location as the file" : backupDir));
                return false;
            }
            else {
                KisUsageLogger::log(QString("Create a simple backup for %1 in %2.").arg(job.filePath).arg(backupDir.isEmpty() ? "the same location as the file" : backupDir));
            }
        }
        else if (numOfBackupsKept > 1) {
            if (!KBackup::numberedBackupFile(job.filePath, backupDir, suffix, numOfBackupsKept)) {
                qWarning() << "Failed to create numbered backup file!" << job.filePath << backupDir << suffix;
                KisUsageLogger::log(QString("Failed to create a numbered backup for %2.").arg(job.filePath).arg(backupDir.isEmpty() ? "the same location as the file" : backupDir));
                return false;
            }
            else {
                KisUsageLogger::log(QString("Create a simple backup for %1 in %2.").arg(job.filePath).arg(backupDir.isEmpty() ? "the same location as the file" : backupDir));
            }
        }
    }

    //KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!job.mimeType.isEmpty(), false);
    if (job.mimeType.isEmpty()) {
        KisImportExportErrorCode error = ImportExportCodes::FileFormatNotSupported;
        slotCompleteSavingDocument(job, error, error.errorMessage(), "");
        return false;

    }

    const QString actionName =
            job.flags & KritaUtils::SaveIsExporting ?
                i18n("Exporting Document...") :
                i18n("Saving Document...");

    KritaUtils::JobResult result =
            initiateSavingInBackground(actionName,
                                       this, SLOT(slotCompleteSavingDocument(KritaUtils::ExportFileJob, KisImportExportErrorCode, QString, QString)),
                                       job, exportConfiguration, isAdvancedExporting);

    if (result == KritaUtils::JobResult::Busy) {
        KisUsageLogger::log(QString("Failed to initiate saving %1 in background.").arg(job.filePath));
        slotCompleteSavingDocument(job, ImportExportCodes::Busy,
                                   i18n("Could not start saving %1. Wait until the current save operation has finished.", job.filePath),
                                   "");
        return false;
    }

    return (result == KritaUtils::JobResult::Success);
}

bool KisDocument::exportDocument(const QString &path, const QByteArray &mimeType, bool isAdvancedExporting, bool showWarnings, KisPropertiesConfigurationSP exportConfiguration)
{
    using namespace KritaUtils;

    SaveFlags flags = SaveIsExporting;
    if (showWarnings) {
        flags |= SaveShowWarnings;
    }

    KisUsageLogger::log(QString("Exporting Document: %1 as %2. %3 * %4 pixels, %5 layers, %6 frames, %7 framerate. Export configuration: %8")
                        .arg(path)
                        .arg(QString::fromLatin1(mimeType))
                        .arg(d->image->width())
                        .arg(d->image->height())
                        .arg(d->image->nlayers())
                        .arg(d->image->animationInterface()->totalLength())
                        .arg(d->image->animationInterface()->framerate())
                        .arg(exportConfiguration ? exportConfiguration->toXML() : "No configuration"));


    return exportDocumentImpl(KritaUtils::ExportFileJob(path,
                                                        mimeType,
                                                        flags),
                              exportConfiguration, isAdvancedExporting);
}

bool KisDocument::saveAs(const QString &_path, const QByteArray &mimeType, bool showWarnings, KisPropertiesConfigurationSP exportConfiguration)
{
    using namespace KritaUtils;

    KisUsageLogger::log(QString("Saving Document %9 as %1 (mime: %2). %3 * %4 pixels, %5 layers.  %6 frames, %7 framerate. Export configuration: %8")
                        .arg(_path)
                        .arg(QString::fromLatin1(mimeType))
                        .arg(d->image->width())
                        .arg(d->image->height())
                        .arg(d->image->nlayers())
                        .arg(d->image->animationInterface()->totalLength())
                        .arg(d->image->animationInterface()->framerate())
                        .arg(exportConfiguration ? exportConfiguration->toXML() : "No configuration")
                        .arg(path()));


    // Check whether it's an existing resource were are saving to
    if (resourceSavingFilter(_path, mimeType, exportConfiguration)) {
        return true;
    }

    return exportDocumentImpl(ExportFileJob(_path,
                                            mimeType,
                                            showWarnings ? SaveShowWarnings : SaveNone),
                              exportConfiguration);
}

bool KisDocument::save(bool showWarnings, KisPropertiesConfigurationSP exportConfiguration)
{
    return saveAs(path(), mimeType(), showWarnings, exportConfiguration);
}

QByteArray KisDocument::serializeToNativeByteArray()
{
    QBuffer buffer;

    QScopedPointer<KisImportExportFilter> filter(KisImportExportManager::filterForMimeType(nativeFormatMimeType(), KisImportExportManager::Export));
    filter->setBatchMode(true);
    filter->setMimeType(nativeFormatMimeType());

    Private::StrippedSafeSavingLocker locker(&d->savingMutex, d->image);
    if (!locker.successfullyLocked()) {
        return buffer.data();
    }

    d->savingImage = d->image;

    if (!filter->convert(this, &buffer).isOk()) {
        qWarning() << "serializeToByteArray():: Could not export to our native format";
    }

    return buffer.data();
}

class DlgLoadMessages : public KoDialog
{
public:
    DlgLoadMessages(const QString &title,
                    const QString &message,
                    const QStringList &warnings)
        : KoDialog(qApp->activeWindow())
    {
        setWindowTitle(title);
        QWidget *page = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(page);
        QHBoxLayout *hlayout = new QHBoxLayout();
        QLabel *labelWarning= new QLabel();
        labelWarning->setPixmap(KisIconUtils::loadIcon("warning").pixmap(32, 32));
        labelWarning->setAlignment(Qt::AlignTop);
        hlayout->addWidget(labelWarning);
        hlayout->addWidget(new QLabel(message), 1);
        layout->addLayout(hlayout);
        if (!warnings.isEmpty()) {
            QTextBrowser *browser = new QTextBrowser();
            QString warning = "<html><body><ul>";
            Q_FOREACH (const QString &w, warnings) {
                warning += "\n<li>" + w + "</li>";
            }
            warning += "</ul>";
            browser->setHtml(warning);
            browser->setMinimumHeight(200);
            browser->setMinimumWidth(400);
            layout->addWidget(browser);
        }
        setMainWidget(page);
        page->setParent(this);
        setButtons(KoDialog::Ok);
    }
};

void KisDocument::slotCompleteSavingDocument(const KritaUtils::ExportFileJob &job, KisImportExportErrorCode status, const QString &errorMessage, const QString &warningMessage)
{
    if (status.isCancelled())
        return;

    const QString fileName = QFileInfo(job.filePath).fileName();

    if (!status.isOk()) {
        emit statusBarMessage(i18nc("%1 --- failing file name, %2 --- error message",
                                    "Error during saving %1: %2",
                                    fileName,
                                    errorMessage), errorMessageTimeout);

        if (!fileBatchMode()) {
            DlgLoadMessages dlg(i18nc("@title:window", "Krita"),
                                i18n("Could not save %1.\nReason: %2", job.filePath, status.errorMessage()),
                                errorMessage.split("\n") + warningMessage.split("\n"));
            dlg.exec();
        }
    }
    else {
        if (!fileBatchMode() && !warningMessage.isEmpty()) {
            DlgLoadMessages dlg(i18nc("@title:window", "Krita"),
                                i18nc("dialog box shown to the user if there were warnings while saving the document, %1 is the file path",
                                      "%1 has been saved but is incomplete.\nThe following problems were encountered when saving:", job.filePath),
                                warningMessage.split("\n"));
            dlg.exec();
        }


        if (!(job.flags & KritaUtils::SaveIsExporting)) {
            const QString existingAutoSaveBaseName = localFilePath();
            const bool wasRecovered = isRecovered();

            d->updateDocumentMetadataOnSaving(job.filePath, job.mimeType);

            removeAutoSaveFiles(existingAutoSaveBaseName, wasRecovered);
        }

        emit completed();
        emit sigSavingFinished(job.filePath);

        emit statusBarMessage(i18n("Finished saving %1", fileName), successMessageTimeout);
    }
}

void KisDocument::Private::updateDocumentMetadataOnSaving(const QString &filePath, const QByteArray &mimeType)
{
    q->setPath(filePath);
    q->setLocalFilePath(filePath);
    q->setMimeType(mimeType);
    q->updateEditingTime(true);

    if (!modifiedWhileSaving) {
        /**
         * If undo stack is already clean/empty, it doesn't emit any
         * signals, so we might forget update document modified state
         * (which was set, e.g. while recovering an autosave file)
         */

        if (undoStack->isClean()) {
            q->setModified(false);
        } else {
            imageModifiedWithoutUndo = false;
            undoStack->setClean();
        }
    }
    q->setRecovered(false);
}

QByteArray KisDocument::mimeType() const
{
    return d->mimeType;
}

void KisDocument::setMimeType(const QByteArray & mimeType)
{
    d->mimeType = mimeType;
}

bool KisDocument::fileBatchMode() const
{
    return d->batchMode;
}

void KisDocument::setFileBatchMode(const bool batchMode)
{
    d->batchMode = batchMode;
}

void KisDocument::Private::uploadLinkedResourcesFromLayersToStorage()
{
    /// Fetch resources from KisAdjustmentLayer, KisFilterMask and
    /// KisGeneratorLayer and put them into the cloned storage. This must be
    /// done in the context of the GUI thread, otherwise we will not be able to
    /// access resources database

    KisDocument *doc = q;

    KisLayerUtils::recursiveApplyNodes(doc->image()->root(),
        [doc] (KisNodeSP node) {
            if (KisNodeFilterInterface *layer = dynamic_cast<KisNodeFilterInterface*>(node.data())) {
                KisFilterConfigurationSP filterConfig = layer->filter();
                if (!filterConfig) return;

                QList<KoResourceLoadResult> linkedResources = filterConfig->linkedResources(KisGlobalResourcesInterface::instance());

                Q_FOREACH (const KoResourceLoadResult &result, linkedResources) {
                    KIS_SAFE_ASSERT_RECOVER(result.type() != KoResourceLoadResult::EmbeddedResource) { continue; }

                    KoResourceSP resource = result.resource();

                    if (!resource) {
                        qWarning() << "WARNING: KisDocument::lockAndCloneForSaving failed to fetch a resource" << result.signature();
                        continue;
                    }

                    QBuffer buf;
                    buf.open(QBuffer::WriteOnly);

                    KisResourceModel model(resource->resourceType().first);
                    bool res = model.exportResource(resource, &buf);

                    buf.close();

                    if (!res) {
                        qWarning() << "WARNING: KisDocument::lockAndCloneForSaving failed to export resource" << result.signature();
                        continue;
                    }

                    buf.open(QBuffer::ReadOnly);

                    res = doc->d->linkedResourceStorage->importResource(resource->resourceType().first + "/" + resource->filename(), &buf);

                    buf.close();

                    if (!res) {
                        qWarning() << "WARNING: KisDocument::lockAndCloneForSaving failed to import resource" << result.signature();
                        continue;
                    }
                }

            }
    });
}

KisDocument *KisDocument::Private::lockAndCloneImpl(bool fetchResourcesFromLayers)
{
    // force update of all the asynchronous nodes before cloning
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    KisLayerUtils::forceAllDelayedNodesUpdate(image->root());

    KisMainWindow *window = KisPart::instance()->currentMainwindow();
    if (window) {
        if (window->viewManager()) {
            if (!window->viewManager()->blockUntilOperationsFinished(image)) {
                return 0;
            }
        }
    }

    Private::StrippedSafeSavingLocker locker(&savingMutex, image);
    if (!locker.successfullyLocked()) {
        return 0;
    }

    KisDocument *doc = new KisDocument(*this->q, false);

    if (fetchResourcesFromLayers) {
        doc->d->uploadLinkedResourcesFromLayersToStorage();
    }

    return doc;
}

KisDocument* KisDocument::lockAndCloneForSaving()
{
    return d->lockAndCloneImpl(true);
}

KisDocument *KisDocument::lockAndCreateSnapshot()
{
    return d->lockAndCloneImpl(false);
}

void KisDocument::copyFromDocument(const KisDocument &rhs)
{
    copyFromDocumentImpl(rhs, REPLACE);
}

void KisDocument::copyFromDocumentImpl(const KisDocument &rhs, CopyPolicy policy)
{
    if (policy == REPLACE) {
        d->decorationsSyncingDisabled = true;
        d->copyFrom(*(rhs.d), this);
        d->decorationsSyncingDisabled = false;

        d->undoStack->clear();
    } else {
        // in CONSTRUCT mode, d should be already initialized
        connect(KisConfigNotifier::instance(), SIGNAL(configChanged()), SLOT(slotConfigChanged()));
        connect(d->undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(slotUndoStackCleanChanged(bool)));
        connect(d->autoSaveTimer, SIGNAL(timeout()), this, SLOT(slotAutoSave()));

        d->shapeController = new KisShapeController(d->nserver, d->undoStack, this);
        d->koShapeController = new KoShapeController(0, d->shapeController);
        d->shapeController->resourceManager()->setGlobalShapeController(d->koShapeController);
    }

    setObjectName(rhs.objectName());

    slotConfigChanged();

    if (rhs.d->image) {
        if (policy == REPLACE) {
            d->image->barrierLock(/* readOnly = */ false);
            rhs.d->image->barrierLock(/* readOnly = */ true);
            d->image->copyFromImage(*(rhs.d->image));
            d->image->unlock();
            rhs.d->image->unlock();

            setCurrentImage(d->image, /* forceInitialUpdate = */ true);
        } else {
            // clone the image with keeping the GUIDs of the layers intact
            // NOTE: we expect the image to be locked!
            setCurrentImage(rhs.image()->clone(/* exactCopy = */ true), /* forceInitialUpdate = */ false);
        }
    }

    if (policy == REPLACE) {
        d->syncDecorationsWrapperLayerState();
    }

    if (rhs.d->preActivatedNode) {
        QQueue<KisNodeSP> linearizedNodes;
        KisLayerUtils::recursiveApplyNodes(rhs.d->image->root(),
                                           [&linearizedNodes](KisNodeSP node) {
            linearizedNodes.enqueue(node);
        });
        KisLayerUtils::recursiveApplyNodes(d->image->root(),
                                           [&linearizedNodes, &rhs, this](KisNodeSP node) {
            KisNodeSP refNode = linearizedNodes.dequeue();
            if (rhs.d->preActivatedNode.data() == refNode.data()) {
                d->preActivatedNode = node;
            }
        });
    }

    // reinitialize references' signal connection
    KisReferenceImagesLayerSP referencesLayer = this->referenceImagesLayer();
    if (referencesLayer) {
        d->referenceLayerConnections.clear();
        d->referenceLayerConnections.addConnection(
                    referencesLayer, SIGNAL(sigUpdateCanvas(QRectF)),
                    this, SIGNAL(sigReferenceImagesChanged()));

        emit sigReferenceImagesLayerChanged(referencesLayer);
    }

    KisDecorationsWrapperLayerSP decorationsLayer =
            KisLayerUtils::findNodeByType<KisDecorationsWrapperLayer>(d->image->root());
    if (decorationsLayer) {
        decorationsLayer->setDocument(this);
    }


    if (policy == REPLACE) {
        setModified(true);
    }
}

bool KisDocument::exportDocumentSync(const QString &path, const QByteArray &mimeType, KisPropertiesConfigurationSP exportConfiguration)
{
    {
        /**
         * The caller guarantees that no one else uses the document (usually,
         * it is a temporary document created specifically for exporting), so
         * we don't need to copy or lock the document. Instead we should just
         * ensure the barrier lock is synced and then released.
         */
        Private::StrippedSafeSavingLocker locker(&d->savingMutex, d->image);
        if (!locker.successfullyLocked()) {
            return false;
        }
    }

    d->savingImage = d->image;

    KisImportExportErrorCode status =
            d->importExportManager->
            exportDocument(path, path, mimeType, false, exportConfiguration);

    d->savingImage = 0;

    return status.isOk();
}


KritaUtils::JobResult KisDocument::initiateSavingInBackground(const QString actionName,
                                             const QObject *receiverObject, const char *receiverMethod,
                                             const KritaUtils::ExportFileJob &job,
                                             KisPropertiesConfigurationSP exportConfiguration,bool isAdvancedExporting)
{
    return initiateSavingInBackground(actionName, receiverObject, receiverMethod,
                                      job, exportConfiguration, std::unique_ptr<KisDocument>(), isAdvancedExporting);
}

KritaUtils::JobResult KisDocument::initiateSavingInBackground(const QString actionName,
                                             const QObject *receiverObject, const char *receiverMethod,
                                             const KritaUtils::ExportFileJob &job,
                                             KisPropertiesConfigurationSP exportConfiguration,
                                             std::unique_ptr<KisDocument> &&optionalClonedDocument,bool isAdvancedExporting)
{
    KIS_ASSERT_RECOVER_RETURN_VALUE(job.isValid(), KritaUtils::JobResult::Failure);

    QScopedPointer<KisDocument> clonedDocument;

    if (!optionalClonedDocument) {
        clonedDocument.reset(lockAndCloneForSaving());
    } else {
        clonedDocument.reset(optionalClonedDocument.release());
    }

    if (!d->savingMutex.tryLock()){
        return KritaUtils::JobResult::Busy;
    }

    if (!clonedDocument) {
        return KritaUtils::JobResult::Failure;
    }

    auto waitForImage = [] (KisImageSP image) {
        KisMainWindow *window = KisPart::instance()->currentMainwindow();
        if (window) {
            if (window->viewManager()) {
                window->viewManager()->blockUntilOperationsFinishedForced(image);
            }
        }
    };

    {
        KisNodeSP newRoot = clonedDocument->image()->root();
        KIS_SAFE_ASSERT_RECOVER(!KisLayerUtils::hasDelayedNodeWithUpdates(newRoot)) {
            KisLayerUtils::forceAllDelayedNodesUpdate(newRoot);
            waitForImage(clonedDocument->image());
        }
    }

    if (clonedDocument->image()->hasOverlaySelectionMask()) {
        clonedDocument->image()->setOverlaySelectionMask(0);
        waitForImage(clonedDocument->image());
    }

    KisConfig cfg(true);
    if (cfg.trimKra()) {
        clonedDocument->image()->cropImage(clonedDocument->image()->bounds());
        clonedDocument->image()->purgeUnusedData(false);
        waitForImage(clonedDocument->image());
    }

    KIS_SAFE_ASSERT_RECOVER(clonedDocument->image()->isIdle()) {
        waitForImage(clonedDocument->image());
    }

    KIS_ASSERT_RECOVER_RETURN_VALUE(!d->backgroundSaveDocument, KritaUtils::JobResult::Failure);
    KIS_ASSERT_RECOVER_RETURN_VALUE(!d->backgroundSaveJob.isValid(), KritaUtils::JobResult::Failure);
    d->backgroundSaveDocument.reset(clonedDocument.take());
    d->backgroundSaveJob = job;
    d->modifiedWhileSaving = false;

    if (d->backgroundSaveJob.flags & KritaUtils::SaveInAutosaveMode) {
        d->backgroundSaveDocument->d->isAutosaving = true;
    }

    connect(d->backgroundSaveDocument.data(),
            SIGNAL(sigBackgroundSavingFinished(KisImportExportErrorCode, QString, QString)),
            this,
            SLOT(slotChildCompletedSavingInBackground(KisImportExportErrorCode, QString, QString)));


    connect(this, SIGNAL(sigCompleteBackgroundSaving(KritaUtils::ExportFileJob, KisImportExportErrorCode, QString, QString)),
            receiverObject, receiverMethod, Qt::UniqueConnection);

    bool started =
            d->backgroundSaveDocument->startExportInBackground(actionName,
                                                               job.filePath,
                                                               job.filePath,
                                                               job.mimeType,
                                                               job.flags & KritaUtils::SaveShowWarnings,
                                                               exportConfiguration, isAdvancedExporting);

    if (!started) {
        // the state should have been deinitialized in slotChildCompletedSavingInBackground()
        KIS_SAFE_ASSERT_RECOVER (!d->backgroundSaveDocument && !d->backgroundSaveJob.isValid()) {
            d->backgroundSaveDocument.take()->deleteLater();
            d->savingMutex.unlock();
            d->backgroundSaveJob = KritaUtils::ExportFileJob();
        }
        return KritaUtils::JobResult::Failure;
    }

    return KritaUtils::JobResult::Success;
}


void KisDocument::slotChildCompletedSavingInBackground(KisImportExportErrorCode status, const QString &errorMessage, const QString &warningMessage)
{
    KIS_ASSERT_RECOVER_RETURN(isSaving());

    KIS_ASSERT_RECOVER(d->backgroundSaveDocument) {
        d->savingMutex.unlock();
        return;
    }

    if (d->backgroundSaveJob.flags & KritaUtils::SaveInAutosaveMode) {
        d->backgroundSaveDocument->d->isAutosaving = false;
    }

    d->backgroundSaveDocument.take()->deleteLater();

    KIS_ASSERT_RECOVER(d->backgroundSaveJob.isValid()) {
        d->savingMutex.unlock();
        return;
    }

    const KritaUtils::ExportFileJob job = d->backgroundSaveJob;
    d->backgroundSaveJob = KritaUtils::ExportFileJob();

    // unlock at the very end
    d->savingMutex.unlock();

    QFileInfo fi(job.filePath);
    KisUsageLogger::log(QString("Completed saving %1 (mime: %2). Result: %3. Warning: %4. Size: %5")
                        .arg(job.filePath)
                        .arg(QString::fromLatin1(job.mimeType))
                        .arg(!status.isOk() ? errorMessage : "OK")
                        .arg(warningMessage)
                        .arg(fi.size()));

    emit sigCompleteBackgroundSaving(job, status, errorMessage, warningMessage);
}

void KisDocument::slotAutoSaveImpl(std::unique_ptr<KisDocument> &&optionalClonedDocument)
{
    if (!d->modified || !d->modifiedAfterAutosave) return;
    const QString autoSaveFileName = generateAutoSaveFileName(localFilePath());

    emit statusBarMessage(i18n("Autosaving... %1", autoSaveFileName), successMessageTimeout);

    KisUsageLogger::log(QString("Autosaving: %1").arg(autoSaveFileName));

    const bool hadClonedDocument = bool(optionalClonedDocument);
    KritaUtils::JobResult result = KritaUtils::JobResult::Failure;

    if (d->image->isIdle() || hadClonedDocument) {
        result = initiateSavingInBackground(i18n("Autosaving..."),
                                             this, SLOT(slotCompleteAutoSaving(KritaUtils::ExportFileJob, KisImportExportErrorCode, QString, QString)),
                                             KritaUtils::ExportFileJob(autoSaveFileName, nativeFormatMimeType(), KritaUtils::SaveIsExporting | KritaUtils::SaveInAutosaveMode),
                                             0,
                                             std::move(optionalClonedDocument));
    } else {
        emit statusBarMessage(i18n("Autosaving postponed: document is busy..."), errorMessageTimeout);
    }

    if (result != KritaUtils::JobResult::Success && !hadClonedDocument && d->autoSaveFailureCount >= 3) {
        KisCloneDocumentStroke *stroke = new KisCloneDocumentStroke(this);
        connect(stroke, SIGNAL(sigDocumentCloned(KisDocument*)),
                this, SLOT(slotInitiateAsyncAutosaving(KisDocument*)),
                Qt::BlockingQueuedConnection);
        connect(stroke, SIGNAL(sigCloningCancelled()),
                this, SLOT(slotDocumentCloningCancelled()),
                Qt::BlockingQueuedConnection);

        KisStrokeId strokeId = d->image->startStroke(stroke);
        d->image->endStroke(strokeId);

        setInfiniteAutoSaveInterval();

    } else if (result != KritaUtils::JobResult::Success) {
        setEmergencyAutoSaveInterval();
    } else {
        d->modifiedAfterAutosave = false;
    }
}

bool KisDocument::resourceSavingFilter(const QString &path, const QByteArray &mimeType, KisPropertiesConfigurationSP exportConfiguration)
{
    if (QFileInfo(path).absolutePath().startsWith(KisResourceLocator::instance()->resourceLocationBase())) {

        QStringList pathParts = QFileInfo(path).absolutePath().split('/');
        if (pathParts.size() > 0) {
            QString resourceType = pathParts.last();
            if (KisResourceLoaderRegistry::instance()->resourceTypes().contains(resourceType)) {

                KisResourceModel model(resourceType);
                model.setResourceFilter(KisResourceModel::ShowAllResources);

                QString tempFileName = QDir::tempPath() + "/" + QFileInfo(path).fileName();

                if (QFileInfo(path).exists()) {

                    int outResourceId;
                    KoResourceSP res;
                    if (KisResourceCacheDb::getResourceIdFromVersionedFilename(QFileInfo(path).fileName(), resourceType, "", outResourceId)) {
                        res = model.resourceForId(outResourceId);
                    }

                    if (res) {
                        d->modifiedWhileSaving = false;

                        if (!exportConfiguration) {
                            QScopedPointer<KisImportExportFilter> filter(
                                KisImportExportManager::filterForMimeType(mimeType, KisImportExportManager::Export));
                            if (filter) {
                                exportConfiguration = filter->defaultConfiguration(nativeFormatMimeType(), mimeType);
                            }
                        }

                        if (exportConfiguration) {
                            // make sure the the name of the resource doesn't change
                            exportConfiguration->setProperty("name", res->name());
                        }

                        if (exportDocumentSync(tempFileName, mimeType, exportConfiguration)) {
                            QFile f2(tempFileName);
                            f2.open(QFile::ReadOnly);

                            QByteArray ba = f2.readAll();

                            QBuffer buf(&ba);
                            buf.open(QBuffer::ReadOnly);



                            if (res->loadFromDevice(&buf, KisGlobalResourcesInterface::instance())) {
                                if (model.updateResource(res)) {
                                    const QString filePath =
                                        KisResourceLocator::instance()->filePathForResource(res);

                                    d->updateDocumentMetadataOnSaving(filePath, mimeType);

                                    return true;
                                }
                            }
                        }
                    }
                }
                else {
                    d->modifiedWhileSaving = false;
                    if (exportDocumentSync(tempFileName, mimeType, exportConfiguration)) {
                        KoResourceSP res = model.importResourceFile(tempFileName, false);
                        if (res) {
                            const QString filePath =
                                KisResourceLocator::instance()->filePathForResource(res);

                            d->updateDocumentMetadataOnSaving(filePath, mimeType);

                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void KisDocument::slotAutoSave()
{
    slotAutoSaveImpl(std::unique_ptr<KisDocument>());
}

void KisDocument::slotInitiateAsyncAutosaving(KisDocument *clonedDocument)
{
    slotAutoSaveImpl(std::unique_ptr<KisDocument>(clonedDocument));
}

void KisDocument::slotDocumentCloningCancelled()
{
    setEmergencyAutoSaveInterval();
}

void KisDocument::slotPerformIdleRoutines()
{
    d->image->explicitRegenerateLevelOfDetail();


    /// TODO: automatic purging is disabled for now: it modifies
    ///       data managers without creating a transaction, which breaks
    ///       undo.

    // d->image->purgeUnusedData(true);
}

void KisDocument::slotCompleteAutoSaving(const KritaUtils::ExportFileJob &job, KisImportExportErrorCode status, const QString &errorMessage, const QString &warningMessage)
{
    Q_UNUSED(job);
    Q_UNUSED(warningMessage);

    const QString fileName = QFileInfo(job.filePath).fileName();

    if (!status.isOk()) {
        setEmergencyAutoSaveInterval();
        emit statusBarMessage(i18nc("%1 --- failing file name, %2 --- error message",
                                    "Error during autosaving %1: %2",
                                    fileName,
                                    exportErrorToUserMessage(status, errorMessage)), errorMessageTimeout);
    } else {
        KisConfig cfg(true);
        d->autoSaveDelay = cfg.autoSaveInterval();

        if (!d->modifiedWhileSaving) {
            d->autoSaveTimer->stop(); // until the next change
            d->autoSaveFailureCount = 0;
        } else {
            setNormalAutoSaveInterval();
        }

        emit statusBarMessage(i18n("Finished autosaving %1", fileName), successMessageTimeout);
    }
}

bool KisDocument::startExportInBackground(const QString &actionName,
                                          const QString &location,
                                          const QString &realLocation,
                                          const QByteArray &mimeType,
                                          bool showWarnings,
                                          KisPropertiesConfigurationSP exportConfiguration, bool isAdvancedExporting)
{
    d->savingImage = d->image;

    KisMainWindow *window = KisPart::instance()->currentMainwindow();
    if (window) {
        if (window->viewManager()) {
            d->savingUpdater = window->viewManager()->createThreadedUpdater(actionName);
            d->importExportManager->setUpdater(d->savingUpdater);
        }
    }

    KisImportExportErrorCode initializationStatus(ImportExportCodes::OK);
    d->childSavingFuture =
            d->importExportManager->exportDocumentAsyc(location,
                                                       realLocation,
                                                       mimeType,
                                                       initializationStatus,
                                                       showWarnings,
                                                       exportConfiguration,
                                                       isAdvancedExporting);

    if (!initializationStatus.isOk()) {
        if (d->savingUpdater) {
            d->savingUpdater->cancel();
        }
        d->savingImage.clear();
        emit sigBackgroundSavingFinished(initializationStatus, initializationStatus.errorMessage(), "");
        return false;
    }

    typedef QFutureWatcher<KisImportExportErrorCode> StatusWatcher;
    StatusWatcher *watcher = new StatusWatcher();
    watcher->setFuture(d->childSavingFuture);

    connect(watcher, SIGNAL(finished()), SLOT(finishExportInBackground()));
    connect(watcher, SIGNAL(finished()), watcher, SLOT(deleteLater()));

    return true;
}

void KisDocument::finishExportInBackground()
{
    KIS_SAFE_ASSERT_RECOVER(d->childSavingFuture.isFinished()) {
        emit sigBackgroundSavingFinished(ImportExportCodes::InternalError, "", "");
        return;
    }

    KisImportExportErrorCode status = d->childSavingFuture.result();
    QString errorMessage = status.errorMessage();
    QString warningMessage = d->lastWarningMessage;

    if (!d->lastErrorMessage.isEmpty()) {
        if (status == ImportExportCodes::InternalError || status == ImportExportCodes::Failure) {
            errorMessage = d->lastErrorMessage;
        } else {
            errorMessage += "\n" + d->lastErrorMessage;
        }
    }

    d->savingImage.clear();
    d->childSavingFuture = QFuture<KisImportExportErrorCode>();
    d->lastErrorMessage.clear();
    d->lastWarningMessage.clear();

    if (d->savingUpdater) {
        d->savingUpdater->setProgress(100);
    }

    emit sigBackgroundSavingFinished(status, errorMessage, warningMessage);
}

void KisDocument::setReadWrite(bool readwrite)
{
    d->readwrite = readwrite;
    setNormalAutoSaveInterval();

    Q_FOREACH (KisMainWindow *mainWindow, KisPart::instance()->mainWindows()) {
        mainWindow->setReadWrite(readwrite);
    }
}

void KisDocument::setAutoSaveDelay(int delay)
{
    if (isReadWrite() && delay > 0) {
        d->autoSaveTimer->start(delay * 1000);
    } else {
        d->autoSaveTimer->stop();
    }
}

void KisDocument::setNormalAutoSaveInterval()
{
    setAutoSaveDelay(d->autoSaveDelay);
    d->autoSaveFailureCount = 0;
}

void KisDocument::setEmergencyAutoSaveInterval()
{
    const int emergencyAutoSaveInterval = 10; /* sec */
    setAutoSaveDelay(emergencyAutoSaveInterval);
    d->autoSaveFailureCount++;
}

void KisDocument::setInfiniteAutoSaveInterval()
{
    setAutoSaveDelay(-1);
}

KoDocumentInfo *KisDocument::documentInfo() const
{
    return d->docInfo;
}

bool KisDocument::isModified() const
{
    return d->modified;
}

QPixmap KisDocument::generatePreview(const QSize& size)
{
    KisImageSP image = d->image;
    if (d->savingImage) image = d->savingImage;

    if (image) {
        QRect bounds = image->bounds();
        QSize originalSize = bounds.size();
        QSize newSize = bounds.size();
        newSize.scale(size, Qt::KeepAspectRatio);

        bool pixelArt = false;
        // determine if the image is pixel art or not
        if (originalSize.width() < size.width() && originalSize.height() < size.height()) {
            // the image must be smaller than the requested preview
            // the scale must be integer
            if (newSize.height()%originalSize.height() == 0 && newSize.width()%originalSize.width() == 0) {
                pixelArt = true;
            }
        }

        QPixmap px;
        if (pixelArt) {
            // do not scale while converting (because it uses Bicubic)
            QImage original = image->convertToQImage(originalSize, 0);
            // scale using FastTransformation, which is probably Nearest neighbour, suitable for pixel art
            QImage scaled = original.scaled(newSize, Qt::KeepAspectRatio, Qt::FastTransformation);
            px = QPixmap::fromImage(scaled);
        } else {
            px = QPixmap::fromImage(image->convertToQImage(newSize, 0));
        }
        if (px.size() == QSize(0,0)) {
            px = QPixmap(newSize);
            QPainter gc(&px);
            QBrush checkBrush = QBrush(KisCanvasWidgetBase::createCheckersImage(newSize.width() / 5));
            gc.fillRect(px.rect(), checkBrush);
            gc.end();
        }
        return px;
    }
    return QPixmap(size);
}

QString KisDocument::generateAutoSaveFileName(const QString & path) const
{
    QString retval;

    // Using the extension allows to avoid relying on the mime magic when opening
    const QString extension (".kra");
    QString prefix = KisConfig(true).readEntry<bool>("autosavefileshidden") ? QString(".") : QString();
    QRegularExpression autosavePattern1("^\\..+-autosave.kra$");
    QRegularExpression autosavePattern2("^.+-autosave.kra$");

    QFileInfo fi(path);
    QString dir = fi.absolutePath();

#ifdef Q_OS_ANDROID
    // URIs may or may not have a directory backing them, so we save to our default autosave location
    if (path.startsWith("content://")) {
        dir = KisAutoSaveRecoveryDialog::autoSaveLocation();
        QDir().mkpath(dir);
    }
#endif

    QString filename = fi.fileName();

    if (path.isEmpty() || autosavePattern1.match(filename).hasMatch() || autosavePattern2.match(filename).hasMatch() || !fi.isWritable()) {
        // Never saved?
        retval = QString("%1%2%3%4-%5-%6-autosave%7")
                     .arg(KisAutoSaveRecoveryDialog::autoSaveLocation())
                     .arg('/')
                     .arg(prefix)
                     .arg("krita")
                     .arg(qApp->applicationPid())
                     .arg(objectName())
                     .arg(extension);
    } else {
        // Beware: don't reorder arguments
        //   otherwise in case of filename = '1-file.kra' it will become '.-file.kra-autosave.kra' instead of '.1-file.kra-autosave.kra'
        retval = QString("%1%2%3%4-autosave%5").arg(dir).arg('/').arg(prefix).arg(filename).arg(extension);
    }

    //qDebug() << "generateAutoSaveFileName() for path" << path << ":" << retval;
    return retval;
}

bool KisDocument::importDocument(const QString &_path)
{
    bool ret;

    dbgUI << "path=" << _path;

    // open...
    ret = openPath(_path);

    // reset url & m_file (kindly? set by KisParts::openUrl()) to simulate a
    // File --> Import
    if (ret) {
        dbgUI << "success, resetting url";
        resetPath();
        setTitleModified();
    }

    return ret;
}


bool KisDocument::openPath(const QString &_path, OpenFlags flags)
{
    dbgUI << "path=" << _path;
    d->lastErrorMessage.clear();

    // Reimplemented, to add a check for autosave files and to improve error reporting
    if (_path.isEmpty()) {
        d->lastErrorMessage = i18n("Malformed Path\n%1", _path);  // ## used anywhere ?
        return false;
    }

    QString path = _path;
    QString original  = "";
    bool autosaveOpened = false;
    if (!fileBatchMode()) {
        QString file = path;
        QString asf = generateAutoSaveFileName(file);
        if (QFile::exists(asf)) {
            KisApplication *kisApp = static_cast<KisApplication*>(qApp);
            kisApp->hideSplashScreen();
            //qDebug() <<"asf=" << asf;
            // ## TODO compare timestamps ?
            KisRecoverNamedAutosaveDialog dlg(0, file, asf);
            dlg.exec();
            int res = dlg.result();

            switch (res) {
            case KisRecoverNamedAutosaveDialog::OpenAutosave :
                original = file;
                path = asf;
                autosaveOpened = true;
                break;
            case KisRecoverNamedAutosaveDialog::OpenMainFile :
                KisUsageLogger::log(QString("Removing autosave file: %1").arg(asf));
                QFile::remove(asf);
                break;
            default: // Cancel
                return false;
            }
        }
    }

    bool ret = openPathInternal(path);

    if (autosaveOpened || flags & RecoveryFile) {
        setReadWrite(true); // enable save button
        setModified(true);
        setRecovered(true);

        setPath(original); // since it was an autosave, it will be a local file
        setLocalFilePath(original);
    }
    else {
        if (ret) {

            if (!(flags & DontAddToRecent)) {
                KisPart::instance()->addRecentURLToAllMainWindows(QUrl::fromLocalFile(_path));
            }

            QFileInfo fi(_path);
            setReadWrite(fi.isWritable());
        }

        setRecovered(false);
    }

    return ret;
}

bool KisDocument::openFile()
{
    //dbgUI <<"for" << localFilePath();
    if (!QFile::exists(localFilePath()) && !fileBatchMode()) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("File %1 does not exist.", localFilePath()));
        return false;
    }

    QString filename = localFilePath();
    QString typeName = mimeType();

    if (typeName.isEmpty()) {
        typeName = KisMimeDatabase::mimeTypeForFile(filename);
    }

    // Allow to open backup files, don't keep the mimeType application/x-trash.
    if (typeName == "application/x-trash") {
        QString path = filename;
        while (path.length() > 0) {
            path.chop(1);
            typeName = KisMimeDatabase::mimeTypeForFile(path);
            //qDebug() << "\t" << path << typeName;
            if (!typeName.isEmpty()) {
                break;
            }
        }
        //qDebug() << "chopped" << filename  << "to" << path << "Was trash, is" << typeName;
    }
    dbgUI << localFilePath() << "type:" << typeName;

    KisMainWindow *window = KisPart::instance()->currentMainwindow();
    KoUpdaterPtr updater;
    if (window && window->viewManager()) {
        updater = window->viewManager()->createUnthreadedUpdater(i18n("Opening document"));
        d->importExportManager->setUpdater(updater);
    }

    KisImportExportErrorCode status = d->importExportManager->importDocument(localFilePath(), typeName);

    if (!status.isOk()) {
        if (window && window->viewManager()) {
            updater->cancel();
        }
        QString msg = status.errorMessage();
        KisUsageLogger::log(QString("Loading %1 failed: %2").arg(prettyPath()).arg(msg));

        if (!msg.isEmpty() && !fileBatchMode()) {
            DlgLoadMessages dlg(i18nc("@title:window", "Krita"),
                                i18n("Could not open %2.\nReason: %1", msg, prettyPath()),
                                errorMessage().split("\n") + warningMessage().split("\n"));
            dlg.exec();
        }
        return false;
    }
    else if (!warningMessage().isEmpty() && !fileBatchMode()) {
        DlgLoadMessages dlg(i18nc("@title:window", "Krita"),
                            i18n("There were problems opening %1", prettyPath()),
                            warningMessage().split("\n"));
        dlg.exec();
        setPath(QString());
    }

    setMimeTypeAfterLoading(typeName);
    d->syncDecorationsWrapperLayerState();
    emit sigLoadingFinished();

    undoStack()->clear();

    return true;
}

void KisDocument::autoSaveOnPause()
{
    if (!d->modified || !d->modifiedAfterAutosave)
        return;

    const QString autoSaveFileName = generateAutoSaveFileName(localFilePath());

    bool started = exportDocumentSync(autoSaveFileName, nativeFormatMimeType());

    if (started)
    {
        d->modifiedAfterAutosave = false;
        dbgAndroid << "autoSaveOnPause successful";
    }
    else
    {
        qWarning() << "Could not auto-save when paused";
    }
}

// shared between openFile and koMainWindow's "create new empty document" code
void KisDocument::setMimeTypeAfterLoading(const QString& mimeType)
{
    d->mimeType = mimeType.toLatin1();
    d->outputMimeType = d->mimeType;
}


bool KisDocument::loadNativeFormat(const QString & file_)
{
    return openPath(file_);
}

void KisDocument::setModified(bool mod)
{
    if (mod) {
        updateEditingTime(false);
    }

    if (d->isAutosaving)   // ignore setModified calls due to autosaving
        return;

    //dbgUI<<" url:" << url.path();
    //dbgUI<<" mod="<<mod<<" MParts mod="<<KisParts::ReadWritePart::isModified()<<" isModified="<<isModified();

    if (mod && !d->autoSaveTimer->isActive()) {
        // First change since last autosave -> start the autosave timer
        setNormalAutoSaveInterval();
    }
    d->modifiedAfterAutosave = mod;
    d->modifiedWhileSaving = mod;

    if (!mod) {
        d->imageModifiedWithoutUndo = mod;
    }

    if (mod == isModified())
        return;

    d->modified = mod;

    if (mod) {
        documentInfo()->updateParameters();
    }

    // This influences the title
    setTitleModified();
    emit modified(mod);
}

void KisDocument::setRecovered(bool value)
{
    d->isRecovered = value;
}

bool KisDocument::isRecovered() const
{
    return d->isRecovered;
}

void KisDocument::updateEditingTime(bool forceStoreElapsed)
{
    QDateTime now = QDateTime::currentDateTime();
    int firstModDelta = d->firstMod.secsTo(now);
    int lastModDelta = d->lastMod.secsTo(now);

    if (lastModDelta > 30) {
        d->docInfo->setAboutInfo("editing-time", QString::number(d->docInfo->aboutInfo("editing-time").toInt() + d->firstMod.secsTo(d->lastMod)));
        d->firstMod = now;
    } else if (firstModDelta > 60 || forceStoreElapsed) {
        d->docInfo->setAboutInfo("editing-time", QString::number(d->docInfo->aboutInfo("editing-time").toInt() + firstModDelta));
        d->firstMod = now;
    }

    d->lastMod = now;
}

QString KisDocument::prettyPath() const
{
    QString _url(path());
#ifdef Q_OS_WIN
    _url = QDir::toNativeSeparators(_url);
#endif
    return _url;
}

// Get caption from document info (title(), in about page)
QString KisDocument::caption() const
{
    QString c;
    const QString _url(QFileInfo(path()).fileName());

    // if URL is empty...it is probably an unsaved file
    if (_url.isEmpty()) {
        c = " [" + i18n("Not Saved") + "] ";
    } else {
        c = _url; // Fall back to document URL
    }

    return c;
}

void KisDocument::setTitleModified()
{
    emit titleModified(caption(), isModified());
}

QDomDocument KisDocument::createDomDocument(const QString& tagName, const QString& version) const
{
    return createDomDocument("krita", tagName, version);
}

//static
QDomDocument KisDocument::createDomDocument(const QString& appName, const QString& tagName, const QString& version)
{
    QDomImplementation impl;
    QString url = QString("http://www.calligra.org/DTD/%1-%2.dtd").arg(appName).arg(version);
    QDomDocumentType dtype = impl.createDocumentType(tagName,
                                                     QString("-//KDE//DTD %1 %2//EN").arg(appName).arg(version),
                                                     url);
    // The namespace URN doesn't need to include the version number.
    QString namespaceURN = QString("http://www.calligra.org/DTD/%1").arg(appName);
    QDomDocument doc = impl.createDocument(namespaceURN, tagName, dtype);
    doc.insertBefore(doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\""), doc.documentElement());
    return doc;
}

bool KisDocument::isNativeFormat(const QByteArray& mimeType) const
{
    if (mimeType == nativeFormatMimeType())
        return true;
    return extraNativeMimeTypes().contains(mimeType);
}

void KisDocument::setErrorMessage(const QString& errMsg)
{
    d->lastErrorMessage = errMsg;
}

QString KisDocument::errorMessage() const
{
    return d->lastErrorMessage;
}

void KisDocument::setWarningMessage(const QString& warningMsg)
{
    d->lastWarningMessage = warningMsg;
}

QString KisDocument::warningMessage() const
{
    return d->lastWarningMessage;
}


void KisDocument::removeAutoSaveFiles(const QString &autosaveBaseName, bool wasRecovered)
{
    // Eliminate any auto-save file
    QString asf = generateAutoSaveFileName(autosaveBaseName);   // the one in the current dir
    if (QFile::exists(asf)) {
        KisUsageLogger::log(QString("Removing autosave file: %1").arg(asf));
        QFile::remove(asf);
    }
    asf = generateAutoSaveFileName(QString());   // and the one in $HOME

    if (QFile::exists(asf)) {
        KisUsageLogger::log(QString("Removing autosave file: %1").arg(asf));
        QFile::remove(asf);
    }

    QList<QRegularExpression> expressions;

    expressions << QRegularExpression("^\\..+-autosave.kra$")
                << QRegularExpression("^.+-autosave.kra$");

    Q_FOREACH(const QRegularExpression &rex, expressions) {
        if (wasRecovered &&
                !autosaveBaseName.isEmpty() &&
                rex.match(QFileInfo(autosaveBaseName).fileName()).hasMatch() &&
                QFile::exists(autosaveBaseName)) {

            KisUsageLogger::log(QString("Removing autosave file: %1").arg(autosaveBaseName));
            QFile::remove(autosaveBaseName);
        }
    }
}

KoUnit KisDocument::unit() const
{
    return d->unit;
}

void KisDocument::setUnit(const KoUnit &unit)
{
    if (d->unit != unit) {
        d->unit = unit;
        emit unitChanged(unit);
    }
}

KUndo2Stack *KisDocument::undoStack()
{
    return d->undoStack;
}

KisImportExportManager *KisDocument::importExportManager() const
{
    return d->importExportManager;
}

void KisDocument::slotUndoStackCleanChanged(bool value)
{
    setModified(!value || d->imageModifiedWithoutUndo);
}

void KisDocument::slotConfigChanged()
{
    KisConfig cfg(true);

    if (d->undoStack->undoLimit() != cfg.undoStackLimit()) {
        if (!d->undoStack->isClean()) {
            d->undoStack->clear();
            // we set this because the document *has* changed, even though the
            // undo history was purged.
            setImageModifiedWithoutUndo();
        }
        d->undoStack->setUndoLimit(cfg.undoStackLimit());
    }

    d->autoSaveDelay = cfg.autoSaveInterval();
    setNormalAutoSaveInterval();
}

void KisDocument::slotImageRootChanged()
{
    d->syncDecorationsWrapperLayerState();
}

void KisDocument::clearUndoHistory()
{
    d->undoStack->clear();
}

KisGridConfig KisDocument::gridConfig() const
{
    return d->gridConfig;
}

void KisDocument::setGridConfig(const KisGridConfig &config)
{
    if (d->gridConfig != config) {
        d->gridConfig = config;
        d->syncDecorationsWrapperLayerState();
        emit sigGridConfigChanged(config);
    }
}

QList<KoResourceLoadResult> KisDocument::linkedDocumentResources()
{
    QList<KoResourceLoadResult> result;
    if (!d->linkedResourceStorage) {
        return result;
    }

    Q_FOREACH(const QString &resourceType, KisResourceLoaderRegistry::instance()->resourceTypes()) {
        QSharedPointer<KisResourceStorage::ResourceIterator> iter = d->linkedResourceStorage->resources(resourceType);
        while (iter->hasNext()) {
            iter->next();

            QBuffer buf;
            buf.open(QBuffer::WriteOnly);
            bool exportSuccessfull =
                d->linkedResourceStorage->exportResource(iter->url(), &buf);

            KoResourceSP resource = d->linkedResourceStorage->resource(iter->url());
            exportSuccessfull &= bool(resource);

            const QString name = resource ? resource->name() : QString();
            const QString fileName = QFileInfo(iter->url()).fileName();
            const KoResourceSignature signature(resourceType,
                                                KoMD5Generator::generateHash(buf.data()),
                                                fileName, name);

            if (exportSuccessfull) {
                result << KoEmbeddedResource(signature, buf.data());
            } else {
                result << signature;
            }
        }
    }

    return result;
}

void KisDocument::setPaletteList(const QList<KoColorSetSP > &paletteList, bool emitSignal)
{
    QList<KoColorSetSP> oldPaletteList;
    if (d->linkedResourceStorage) {
        QSharedPointer<KisResourceStorage::ResourceIterator> iter = d->linkedResourceStorage->resources(ResourceType::Palettes);
        while (iter->hasNext()) {
            iter->next();
            KoResourceSP resource = iter->resource();
            if (resource && resource->valid()) {
                oldPaletteList << resource.dynamicCast<KoColorSet>();
            }
        }
        if (oldPaletteList != paletteList) {
            KisResourceModel resourceModel(ResourceType::Palettes);
            Q_FOREACH(KoColorSetSP palette, oldPaletteList) {
                if (!paletteList.contains(palette)) {
                    resourceModel.setResourceInactive(resourceModel.indexForResource(palette));
                }
            }
            Q_FOREACH(KoColorSetSP palette, paletteList) {
                if (!oldPaletteList.contains(palette)) {
                    resourceModel.addResource(palette, d->linkedResourcesStorageID);
                }
                else {
                    palette->setStorageLocation(d->linkedResourcesStorageID);
                    resourceModel.updateResource(palette);
                }
            }
            if (emitSignal) {
                emit sigPaletteListChanged(oldPaletteList, paletteList);
            }
        }
    }
}

StoryboardItemList KisDocument::getStoryboardItemList()
{
    return d->m_storyboardItemList;
}

void KisDocument::setStoryboardItemList(const StoryboardItemList &storyboardItemList, bool emitSignal)
{
    d->m_storyboardItemList = storyboardItemList;
    if (emitSignal) {
        emit sigStoryboardItemListChanged();
    }
}

QVector<StoryboardComment> KisDocument::getStoryboardCommentsList()
{
    return d->m_storyboardCommentList;
}

void KisDocument::setStoryboardCommentList(const QVector<StoryboardComment> &storyboardCommentList, bool emitSignal)
{
    d->m_storyboardCommentList = storyboardCommentList;
    if (emitSignal) {
        emit sigStoryboardCommentListChanged();
    }
}

const KisGuidesConfig& KisDocument::guidesConfig() const
{
    return d->guidesConfig;
}

void KisDocument::setGuidesConfig(const KisGuidesConfig &data)
{
    if (d->guidesConfig == data) return;

    d->guidesConfig = data;
    d->syncDecorationsWrapperLayerState();
    emit sigGuidesConfigChanged(d->guidesConfig);
}


const KisMirrorAxisConfig& KisDocument::mirrorAxisConfig() const
{
    return d->mirrorAxisConfig;
}

void KisDocument::setMirrorAxisConfig(const KisMirrorAxisConfig &config)
{
    if (d->mirrorAxisConfig == config) {
        return;
    }

    d->mirrorAxisConfig = config;
    if (d->image) {
        d->image->setMirrorAxesCenter(KisAlgebra2D::absoluteToRelative(d->mirrorAxisConfig.axisPosition(),
                                                                       d->image->bounds()));
    }
    setModified(true);

    emit sigMirrorAxisConfigChanged();
}

void KisDocument::resetPath() {
    setPath(QString());
    setLocalFilePath(QString());
}

KoDocumentInfoDlg *KisDocument::createDocumentInfoDialog(QWidget *parent, KoDocumentInfo *docInfo) const
{
    return new KoDocumentInfoDlg(parent, docInfo);
}

bool KisDocument::isReadWrite() const
{
    return d->readwrite;
}

QString KisDocument::path() const
{
    return d->m_path;
}

bool KisDocument::closePath(bool promptToSave)
{
    if (promptToSave) {
        if ( isReadWrite() && isModified()) {
            Q_FOREACH (KisView *view, KisPart::instance()->views()) {
                if (view && view->document() == this) {
                    if (!view->queryClose()) {
                        return false;
                    }
                }
            }
        }
    }
    // Not modified => ok and delete temp file.
    d->mimeType = QByteArray();

    // It always succeeds for a read-only part,
    // but the return value exists for reimplementations
    // (e.g. pressing cancel for a modified read-write part)
    return true;
}



void KisDocument::setPath(const QString &path)
{
    d->m_path = path;
}

QString KisDocument::localFilePath() const
{
    return d->m_file;
}


void KisDocument::setLocalFilePath( const QString &localFilePath )
{
    d->m_file = localFilePath;
}

bool KisDocument::openPathInternal(const QString &path)
{
    if ( path.isEmpty() ) {
        return false;
    }

    if (d->m_bAutoDetectedMime) {
        d->mimeType = QByteArray();
        d->m_bAutoDetectedMime = false;
    }

    QByteArray mimeType = d->mimeType;

    if ( !closePath() ) {
        return false;
    }

    d->mimeType = mimeType;
    setPath(path);

    d->m_file.clear();

    d->m_file = d->m_path;

    bool ret = false;
    // set the mimeType only if it was not already set (for example, by the host application)
    if (d->mimeType.isEmpty()) {
        // get the mimeType of the file
        // using findByUrl() to avoid another string -> url conversion
        QString mime = KisMimeDatabase::mimeTypeForFile(d->m_path);
        d->mimeType = mime.toLocal8Bit();
        d->m_bAutoDetectedMime = true;
    }

    setPath(d->m_path);
    ret = openFile();

    if (ret) {
        emit completed();
    }
    else {
        emit canceled(QString());
    }
    return ret;
}

bool KisDocument::newImage(const QString& name,
                           qint32 width, qint32 height,
                           const KoColorSpace* cs,
                           const KoColor &bgColor, KisConfig::BackgroundStyle bgStyle,
                           int numberOfLayers,
                           const QString &description, const double imageResolution)
{
    Q_ASSERT(cs);

    KisImageSP image;

    if (!cs) return false;

    QApplication::setOverrideCursor(Qt::BusyCursor);

    image = new KisImage(createUndoStore(), width, height, cs, name);

    Q_CHECK_PTR(image);

    connect(image, SIGNAL(sigImageModified()), this, SLOT(setImageModified()), Qt::UniqueConnection);
    connect(image, SIGNAL(sigImageModifiedWithoutUndo()), this, SLOT(setImageModifiedWithoutUndo()), Qt::UniqueConnection);
    image->setResolution(imageResolution, imageResolution);

    image->assignImageProfile(cs->profile());
    image->waitForDone();

    documentInfo()->setAboutInfo("title", name);
    documentInfo()->setAboutInfo("abstract", description);

    KisConfig cfg(false);
    cfg.defImageWidth(width);
    cfg.defImageHeight(height);
    cfg.defImageResolution(imageResolution);
    if (!cfg.useDefaultColorSpace())
    {
        cfg.defColorModel(image->colorSpace()->colorModelId().id());
        cfg.setDefaultColorDepth(image->colorSpace()->colorDepthId().id());
        cfg.defColorProfile(image->colorSpace()->profile()->name());
    }

    bool autopin = cfg.autoPinLayersToTimeline();

    KisLayerSP bgLayer;
    if (bgStyle == KisConfig::RASTER_LAYER || bgStyle == KisConfig::FILL_LAYER) {
        KoColor strippedAlpha = bgColor;
        strippedAlpha.setOpacity(OPACITY_OPAQUE_U8);

        if (bgStyle == KisConfig::RASTER_LAYER) {
            bgLayer = new KisPaintLayer(image.data(), i18n("Background"), OPACITY_OPAQUE_U8, cs);
            bgLayer->paintDevice()->setDefaultPixel(strippedAlpha);
            bgLayer->setPinnedToTimeline(autopin);
        } else if (bgStyle == KisConfig::FILL_LAYER) {
            KisFilterConfigurationSP filter_config = KisGeneratorRegistry::instance()->get("color")->defaultConfiguration(KisGlobalResourcesInterface::instance());
            filter_config->setProperty("color", strippedAlpha.toQColor());
            filter_config->createLocalResourcesSnapshot();
            bgLayer = new KisGeneratorLayer(image.data(), i18nc("Name of automatically created background color fill layer", "Background Fill"), filter_config, image->globalSelection());
        }

        bgLayer->setOpacity(bgColor.opacityU8());

        if (numberOfLayers > 1) {
            //Lock bg layer if others are present.
            bgLayer->setUserLocked(true);
        }
    }
    else { // KisConfig::CANVAS_COLOR (needs an unlocked starting layer).
        image->setDefaultProjectionColor(bgColor);
        bgLayer = new KisPaintLayer(image.data(), image->nextLayerName(), OPACITY_OPAQUE_U8, cs);
    }

    Q_CHECK_PTR(bgLayer);
    image->addNode(bgLayer.data(), image->rootLayer().data());
    bgLayer->setDirty(QRect(0, 0, width, height));

    // reset mirror axis to default:
    d->mirrorAxisConfig.setAxisPosition(QRectF(image->bounds()).center());
    setCurrentImage(image);

    for(int i = 1; i < numberOfLayers; ++i) {
        KisPaintLayerSP layer = new KisPaintLayer(image, image->nextLayerName(), OPACITY_OPAQUE_U8, cs);
        layer->setPinnedToTimeline(autopin);
        image->addNode(layer, image->root(), i);
        layer->setDirty(QRect(0, 0, width, height));
    }

    KisUsageLogger::log(QString("Created image \"%1\", %2 * %3 pixels, %4 dpi. Color model: %6 %5 (%7). Layers: %8")
                        .arg(name)
                        .arg(width).arg(height)
                        .arg(imageResolution * 72.0)
                        .arg(image->colorSpace()->colorModelId().name())
                        .arg(image->colorSpace()->colorDepthId().name())
                        .arg(image->colorSpace()->profile()->name())
                        .arg(numberOfLayers));

    QApplication::restoreOverrideCursor();

    return true;
}

bool KisDocument::isSaving() const
{
    const bool result = d->savingMutex.tryLock();
    if (result) {
        d->savingMutex.unlock();
    }
    return !result;
}

void KisDocument::waitForSavingToComplete()
{
    if (isSaving()) {
        KisAsyncActionFeedback f(i18nc("progress dialog message when the user closes the document that is being saved", "Waiting for saving to complete..."), 0);
        f.waitForMutex(&d->savingMutex);
    }
}

KoShapeControllerBase *KisDocument::shapeController() const
{
    return d->shapeController;
}

KoShapeLayer* KisDocument::shapeForNode(KisNodeSP layer) const
{
    return d->shapeController->shapeForNode(layer);
}

QList<KisPaintingAssistantSP> KisDocument::assistants() const
{
    return d->assistants;
}

void KisDocument::setAssistants(const QList<KisPaintingAssistantSP> &value)
{
    if (d->assistants != value) {
        d->assistants = value;
        d->syncDecorationsWrapperLayerState();
        emit sigAssistantsChanged();
    }
}

KisReferenceImagesLayerSP KisDocument::referenceImagesLayer() const
{
    if (!d->image) return KisReferenceImagesLayerSP();

    KisReferenceImagesLayerSP referencesLayer =
            KisLayerUtils::findNodeByType<KisReferenceImagesLayer>(d->image->root());

    return referencesLayer;
}

void KisDocument::setReferenceImagesLayer(KisSharedPtr<KisReferenceImagesLayer> layer, bool updateImage)
{
    KisReferenceImagesLayerSP currentReferenceLayer = referenceImagesLayer();

    // updateImage=false inherently means we are not changing the
    // reference images layer, but just would like to update its signals.
    if (currentReferenceLayer == layer && updateImage) {
        return;
    }

    d->referenceLayerConnections.clear();

    if (updateImage) {
        if (currentReferenceLayer) {
            d->image->removeNode(currentReferenceLayer);
        }

        if (layer) {
            d->image->addNode(layer);
        }
    }

    currentReferenceLayer = layer;

    if (currentReferenceLayer) {
        d->referenceLayerConnections.addConnection(
                    currentReferenceLayer, SIGNAL(sigUpdateCanvas(QRectF)),
                    this, SIGNAL(sigReferenceImagesChanged()));
    }

    emit sigReferenceImagesLayerChanged(layer);
}

void KisDocument::setPreActivatedNode(KisNodeSP activatedNode)
{
    d->preActivatedNode = activatedNode;
}

KisNodeSP KisDocument::preActivatedNode() const
{
    return d->preActivatedNode;
}

KisImageWSP KisDocument::image() const
{
    return d->image;
}

KisImageSP KisDocument::savingImage() const
{
    return d->savingImage;
}


void KisDocument::setCurrentImage(KisImageSP image, bool forceInitialUpdate)
{
    if (d->image) {
        // Disconnect existing sig/slot connections
        d->image->setUndoStore(new KisDumbUndoStore());
        d->image->disconnect(this);
        d->shapeController->setImage(0);
        d->image = 0;
    }

    if (!image) return;

    if (d->linkedResourceStorage){
        d->linkedResourceStorage->setMetaData(KisResourceStorage::s_meta_name, image->objectName());
    }

    d->setImageAndInitIdleWatcher(image);
    d->image->setUndoStore(new KisDocumentUndoStore(this));
    d->shapeController->setImage(image);
    d->image->setMirrorAxesCenter(KisAlgebra2D::absoluteToRelative(d->mirrorAxisConfig.axisPosition(), image->bounds()));
    setModified(false);
    connect(d->image, SIGNAL(sigImageModified()), this, SLOT(setImageModified()), Qt::UniqueConnection);
    connect(d->image, SIGNAL(sigImageModifiedWithoutUndo()), this, SLOT(setImageModifiedWithoutUndo()), Qt::UniqueConnection);
    connect(d->image, SIGNAL(sigLayersChangedAsync()), this, SLOT(slotImageRootChanged()));

    if (forceInitialUpdate) {
        d->image->initialRefreshGraph();
    }
}

void KisDocument::hackPreliminarySetImage(KisImageSP image)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!d->image);

    // we set image without connecting idle-watcher, because loading
    // hasn't been finished yet
    d->image = image;
    d->shapeController->setImage(image);
}

void KisDocument::setImageModified()
{
    // we only set as modified if undo stack is not at clean state
    setModified(d->imageModifiedWithoutUndo || !d->undoStack->isClean());
}

void KisDocument::setImageModifiedWithoutUndo()
{
    d->imageModifiedWithoutUndo = true;
    setImageModified();
}


KisUndoStore* KisDocument::createUndoStore()
{
    return new KisDocumentUndoStore(this);
}

bool KisDocument::isAutosaving() const
{
    return d->isAutosaving;
}

QString KisDocument::exportErrorToUserMessage(KisImportExportErrorCode status, const QString &errorMessage)
{
    return errorMessage.isEmpty() ? status.errorMessage() : errorMessage;
}

void KisDocument::setAssistantsGlobalColor(QColor color)
{
    d->globalAssistantsColor = color;
}

QColor KisDocument::assistantsGlobalColor()
{
    return d->globalAssistantsColor;
}

QRectF KisDocument::documentBounds() const
{
    QRectF bounds = d->image->bounds();

    KisReferenceImagesLayerSP referenceImagesLayer = this->referenceImagesLayer();

    if (referenceImagesLayer) {
        bounds |= referenceImagesLayer->boundingImageRect();
    }

    return bounds;
}
