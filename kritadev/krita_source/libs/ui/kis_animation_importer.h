/*
 *  SPDX-FileCopyrightText: 2015 Jouni Pentikäinen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_ANIMATION_IMPORTER_H
#define KIS_ANIMATION_IMPORTER_H

#include "kis_types.h"
#include "kritaui_export.h"
#include <KisImportExportFilter.h>
#include <KisImportExportErrorCode.h>
#include <QPair>

class KisDocument;
class KisMainWindow;

class KRITAUI_EXPORT KisAnimationImporter : public QObject
{
    Q_OBJECT    

public:
    KisAnimationImporter(KisImageSP image, KoUpdaterPtr updater = 0);
    KisAnimationImporter(KisDocument* document);
    ~KisAnimationImporter() override;

    KisImportExportErrorCode import(QStringList files
                                    , int firstFrame
                                    , int step
                                    , bool autoAddHoldframes = false
                                    , bool startfrom0 = false
                                    , int isAscending = 0
                                    , bool assignDocumentProfile = false
                                    , QList<int> optionalKeyframeTimeList = {});

private:
    QPair<KisPaintLayerSP, class KisRasterKeyframeChannel*> initializePaintLayer(QScopedPointer<KisDocument>& doc, class KisUndoAdapter* undoAdapter);

private Q_SLOTS:
    void cancel();

private:
    struct Private;
    QScopedPointer<Private> m_d;
};

#endif
