﻿/*
 *  Copyright (c) 2020 Agata Cacko cacko.azh@gmail.com
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "wdgtagselection.h"

#include <QProcessEnvironment>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QGridLayout>
#include <QTableWidget>
#include <QPainter>
#include <QListWidget>
#include <QAction>
#include <QMouseEvent>
#include <QMenu>
#include <QPair>
#include <QTimer>

#include <KisImportExportManager.h>
#include <KoDocumentInfo.h>
#include <KoFileDialog.h>
#include <kis_icon.h>
#include <KoResource.h>
#include <KoResourceServer.h>
#include <KoResourceServerProvider.h>
#include <KisTagModel.h>

#include<KisWrappableHBoxLayout.h>


#include "kis_icon.h"

KisWdgTagSelectionControllerOneResource::KisWdgTagSelectionControllerOneResource(KisTagSelectionWidget *widget, bool editable)
    : QObject(widget)
    , m_tagSelectionWidget(widget)
    , m_editable(editable)
{
    connect(widget, SIGNAL(sigAddTagToSelection(KoID)), this, SLOT(slotAddTag(KoID)));
    connect(widget, SIGNAL(sigRemoveTagFromSelection(KoID)), this, SLOT(slotRemoveTag(KoID)));
    connect(widget, SIGNAL(sigCreateNewTag(QString)), this, SLOT(slotCreateNewTag(QString)));
    m_tagSelectionWidget->setEnabled(false); // because there is no resource selected yet

}

KisWdgTagSelectionControllerOneResource::~KisWdgTagSelectionControllerOneResource()
{

}

void KisWdgTagSelectionControllerOneResource::setResourceIds(QString resourceType, QList<int> resourceIds)
{
    QString oldResourceType = m_resourceType;
    m_resourceIds = resourceIds;
    m_resourceType = resourceType;

    if (resourceType != "" && (oldResourceType != resourceType || !m_tagResourceModel || !m_tagModel)) {
        m_tagResourceModel.reset(new KisTagResourceModel(resourceType));
        m_tagResourceModel->setResourceFilter(KisTagResourceModel::ShowAllResources);
        m_tagModel.reset(new KisTagModel(resourceType));
        m_tagModel->sort(KisAllTagsModel::Name);
    }

    if (resourceIds.count() == 0) {
        QList<KoID> emptyList;
        m_tagSelectionWidget->setTagList(m_editable, emptyList, emptyList);
        m_tagSelectionWidget->setEnabled(false);
    } else {
        m_tagResourceModel->setResourcesFilter(m_resourceIds.toVector());
        m_tagSelectionWidget->setEnabled(true);
        updateView();
    }
}

void KisWdgTagSelectionControllerOneResource::slotRemoveTag(KoID tag)
{
    if (m_resourceIds.count() == 0) return;

    KisTagSP tagsp = m_tagModel->tagForUrl(tag.id());

    m_tagResourceModel->untagResources(tagsp, m_resourceIds.toVector());
    updateView();
}

void KisWdgTagSelectionControllerOneResource::slotAddTag(KoID tag)
{
    if (m_resourceIds.count() == 0) return;

    KisTagSP tagsp = m_tagModel->tagForUrl(tag.id());

    m_tagResourceModel->tagResources(tagsp, m_resourceIds.toVector());
    updateView();
}

void KisWdgTagSelectionControllerOneResource::slotCreateNewTag(QString tag)
{
    if (m_resourceIds.count() == 0 || m_resourceType == "" || tag == "") return;

    KisTagSP tagsp = m_tagModel->tagForUrl(tag);
    if (tagsp.isNull()) {
        QVector<KoResourceSP> vec;
        m_tagModel->addTag(tag, false, vec);
        tagsp = m_tagModel->tagForUrl(tag);
    } else if (!tagsp->active()) { // if tag is active, simply use that tag
        // if you use this simple cast, the order of buttons must match order of options in the enum
        int response = QMessageBox::question(0, i18nc("Dialog title", "Overwrite tag?"), i18nc("Question to the user in a dialog about creating a tag",
                                                                                          "A tag with this unique name already exists. Do you want to replace it?"),
                                           i18nc("Option in a dialog to discard the previously existing tag and creating a new one in its place", "Replace (overwrite) tag"),
                                           i18nc("Option in a dialog to undelete (reactivate) existing tag with its old assigned resources", "Restore previous tag"), i18n("Cancel"));
        if (response == 0) { // Overwrite
            m_tagModel->addTag(tag, true, QVector<KoResourceSP>()); // will overwrite the tag
            tagsp = m_tagModel->tagForUrl(tag);
        } else if (response == 1) { // Restore/use previously existing one
            m_tagModel->setTagActive(tagsp);
        } else {
            updateView();
            return;
        }
    }

    KIS_ASSERT_RECOVER_RETURN(tagsp);
    m_tagResourceModel->tagResources(tagsp, m_resourceIds.toVector());
    updateView();
}

void KisWdgTagSelectionControllerOneResource::updateView()
{
    if (m_resourceIds.count() == 0) {
        QList<KoID> emptyList;
        m_tagSelectionWidget->setTagList(m_editable, emptyList, emptyList);
        return;
    }

    QMap<QString, int> tagsCounts;
    for (int i = 0; i < m_tagModel->rowCount(); i++) {
        QModelIndex idx = m_tagModel->index(i, 0);
        int id = m_tagModel->data(idx, Qt::UserRole + KisAllTagsModel::Id).toInt();
        if (id < 0) {
            continue;
        }
        QString tagUrl = m_tagModel->data(idx, Qt::UserRole + KisAllTagsModel::Url).toString();
        if (!tagsCounts.contains(tagUrl)) {
            tagsCounts.insert(tagUrl, 0);
        }
    }

    // IMPORTANT: this only works correctly because there was setResourcesFilter() called in setResourceIds() function
    // if at any moment there is situation this needs to work without setResourceIds(),
    // call m_tagResourceModel->setResourcesFilter(m_resourceIds.toVector()); before this loop
    // (it will make it slightly slower since it invalides filter in the proxy model)
    for (int i = 0; i < m_tagResourceModel->rowCount(); i++) {
        QModelIndex idx = m_tagResourceModel->index(i, 0);
        KisTagSP tag = m_tagResourceModel->data(idx, Qt::UserRole + KisAllTagResourceModel::Tag).value<KisTagSP>();
        tagsCounts[tag->url()] += 1;
    }
    QList<KoID> semiSelected;
    QList<KoID> selected;
    QList<KoID> toSelect;

    for (int i = 0; i < m_tagModel->rowCount(); i++) {
        QModelIndex idx = m_tagModel->index(i, 0);
        int id = m_tagModel->data(idx, Qt::UserRole + KisAllTagsModel::Id).toInt();
        if (id < 0) {
            continue;
        }
        QString tagUrl = m_tagModel->data(idx, Qt::UserRole + KisAllTagsModel::Url).toString();
        QString tagName = m_tagModel->data(idx, Qt::UserRole + KisAllTagsModel::Name).toString();
        KoID tag(tagUrl, tagName);
        if (tagsCounts[tagUrl] == m_resourceIds.count()) {
            selected << tag;
        } else if (tagsCounts[tagUrl] > 0) {
            semiSelected << tag;
            toSelect << tag; // we want to be able to add a tag to every resource even though some are already tagged
        } else {
            toSelect << tag;
        }
    }

    m_tagSelectionWidget->setTagList(m_editable, selected, toSelect, semiSelected);
}

KisWdgTagSelectionControllerBundleTags::KisWdgTagSelectionControllerBundleTags(KisTagSelectionWidget *widget, bool editable)
    : QObject(widget)
    , m_tagSelectionWidget(widget)
    , m_editable(editable)
{
    connect(widget, SIGNAL(sigAddTagToSelection(KoID)), this, SLOT(slotAddTag(KoID)));
    connect(widget, SIGNAL(sigRemoveTagFromSelection(KoID)), this, SLOT(slotRemoveTag(KoID)));
    updateView();
}

KisWdgTagSelectionControllerBundleTags::~KisWdgTagSelectionControllerBundleTags()
{

}

QList<int> KisWdgTagSelectionControllerBundleTags::getSelectedTagIds() const
{
    QList<int> selectedTags;
    Q_FOREACH(QString resourceType, m_selectedTagsByResourceType.keys()) {
        KisTagModel* model = new KisTagModel(m_resourceType);
        QList<KoID> tagList = m_selectedTagsByResourceType[resourceType];
        Q_FOREACH(KoID tag, tagList) {
            KisTagSP tagSP = model->tagForUrl(tag.id());
            selectedTags << tagSP->id();
        }
    }
    return selectedTags;
}

void KisWdgTagSelectionControllerBundleTags::slotRemoveTag(KoID custom)
{
    if (m_selectedTagsByResourceType.contains(m_resourceType)) {
        if (m_selectedTagsByResourceType[m_resourceType].contains(custom)) {
            m_selectedTagsByResourceType[m_resourceType].removeAll(custom);
            updateView();
        }
    }
}

void KisWdgTagSelectionControllerBundleTags::slotAddTag(KoID custom)
{
    if (!m_selectedTagsByResourceType.contains(m_resourceType)) {
        m_selectedTagsByResourceType.insert(m_resourceType, QList<KoID>());
    }
    if (!m_selectedTagsByResourceType[m_resourceType].contains(custom)) {
        m_selectedTagsByResourceType[m_resourceType].append(custom);
        updateView();
    }
}

void KisWdgTagSelectionControllerBundleTags::updateView()
{
    typedef QPair<QString, QString> resourceTypePair;
    QList<QPair<QString, QString>> resourceTypes = {
        resourceTypePair(i18n("Brush presets"), ResourceType::PaintOpPresets),
        resourceTypePair(i18n("Brush tips"), ResourceType::Brushes),
        resourceTypePair(i18n("Workspaces"), ResourceType::Workspaces),
        resourceTypePair(i18n("Patterns"), ResourceType::Patterns),
        resourceTypePair(i18n("Palettes"), ResourceType::Palettes),
        resourceTypePair(i18n("Layer styles"), ResourceType::LayerStyles),
        resourceTypePair(i18n("Gradients"), ResourceType::Gradients),
        resourceTypePair(i18n("Gamut masks"), ResourceType::GamutMasks),
        resourceTypePair(i18n("SeExpr scripts"), ResourceType::SeExprScripts),
    };

    KisTagModel* model = new KisTagModel(m_resourceType);

    QList<KoID> selected = m_selectedTagsByResourceType.contains(m_resourceType) ? m_selectedTagsByResourceType[m_resourceType] : QList<KoID>();
    QList<KoID> notSelected;


    for (int i = 0; i < model->rowCount(); i++) {
        QModelIndex idx = model->index(i, 0);
        KisTagSP tag = model->data(idx, Qt::UserRole + KisAllTagsModel::KisTagRole).value<KisTagSP>();

        if (tag.isNull() || tag->id() < 0) {
            continue;
        }

        KoID custom = KoID(tag->url(), tag->name());

        if (m_selectedTagsByResourceType.contains(m_resourceType)) {
            if (!m_selectedTagsByResourceType[m_resourceType].contains(custom)) {
                notSelected << custom;
            }
        } else { // no tags from this resource type are selected
            notSelected << custom;
        }
    }

    // m_selectedTags is already categorized correctly and is in KoID form

    m_tagSelectionWidget->setTagList(m_editable, selected, notSelected);

}

void KisWdgTagSelectionControllerBundleTags::setResourceType(const QString &resourceType)
{
    m_resourceType = resourceType;
    updateView();
}













