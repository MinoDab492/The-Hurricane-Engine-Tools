/* This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2008 Alexander Dymo <adymo@kdevelop.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "kshortcutschemeseditor.h"
#include "KisShortcutsDialog_p.h"

#include <QLabel>
#include <QMenu>
#include <QFile>
#include <QPushButton>
#include <QDomDocument>
#include <QStandardPaths>
#include <QInputDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QDebug>

#include <kconfiggroup.h>
#include <kmessagebox.h>
#include <ksharedconfig.h>
#include <KoFileDialog.h>

#include "KisShortcutsDialog.h"
#include "kshortcutschemeshelper_p.h"
#include "kactioncollection.h"
#include "kxmlguiclient.h"

#include "KoResourcePaths.h"


KisKShortcutSchemesEditor::KisKShortcutSchemesEditor(KisShortcutsDialog *parent)
    : m_dialog(parent)
{
    KConfigGroup group(KSharedConfig::openConfig(), "Shortcut Schemes");

    QStringList schemes;
    schemes << QStringLiteral("Default");

    auto schemeFileLocations = KisKShortcutSchemesHelper::schemeFileLocations();
    schemes << schemeFileLocations.keys();

    QString currentScheme = group.readEntry("Current Scheme", "Default");
    QString schemeFileName = KisKShortcutSchemesHelper::schemeFileLocations().value(currentScheme);
    if (!QFileInfo(schemeFileName).exists()) {
        currentScheme = "Default";
    }
    setMargin(0);

    QLabel *schemesLabel = new QLabel(i18n("Shortcut Schemes:"), m_dialog);
    addWidget(schemesLabel);

    m_schemesList = new QComboBox(m_dialog);
    m_schemesList->setEditable(false);
    m_schemesList->addItems(schemes);
    m_schemesList->setCurrentIndex(m_schemesList->findText(currentScheme));
    schemesLabel->setBuddy(m_schemesList);
    addWidget(m_schemesList);

    m_newScheme = new QPushButton(i18nc("New shortcut scheme", "New..."));
    addWidget(m_newScheme);

    m_deleteScheme = new QPushButton(i18n("Delete"));
    addWidget(m_deleteScheme);

    QPushButton *moreActions = new QPushButton(i18n("Save/Load"));
    addWidget(moreActions);

    QMenu *moreActionsMenu = new QMenu(m_dialog);
    // moreActionsMenu->addAction(i18n("Save as Scheme Defaults"),
                               // this, SLOT(saveAsDefaultsForScheme()));

    moreActionsMenu->addAction(i18n("Save Custom Shortcuts"),
                               this, SLOT(saveCustomShortcuts()));
    moreActionsMenu->addAction(i18n("Load Custom Shortcuts"),
                               this, SLOT(loadCustomShortcuts()));
    moreActionsMenu->addAction(i18n("Export Scheme..."),
                               this, SLOT(exportShortcutsScheme()));
    moreActionsMenu->addAction(i18n("Import Scheme..."),
                               this, SLOT(importShortcutsScheme()));
    moreActions->setMenu(moreActionsMenu);

    addStretch(1);

    connect(m_schemesList, SIGNAL(activated(QString)),
            this, SIGNAL(shortcutsSchemeChanged(QString)));
    connect(m_newScheme, SIGNAL(clicked()), this, SLOT(newScheme()));
    connect(m_deleteScheme, SIGNAL(clicked()), this, SLOT(deleteScheme()));
    updateDeleteButton();
}

void KisKShortcutSchemesEditor::newScheme()
{
    bool ok;
    const QString newName = QInputDialog::getText(m_dialog, i18n("Name for New Scheme"),
                            i18n("Name for new scheme:"), QLineEdit::Normal, i18n("New Scheme"), &ok);
    if (!ok) {
        return;
    }

    if (m_schemesList->findText(newName) != -1) {
        KMessageBox::sorry(m_dialog, i18n("A scheme with this name already exists."));
        return;
    }

    const QString newSchemeFileName = KisKShortcutSchemesHelper::shortcutSchemeFileName(newName) + ".shortcuts";

    QFile schemeFile(newSchemeFileName);
    if (!schemeFile.open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << "Could not open scheme file.";
        return;
    }
    schemeFile.close();

    m_dialog->exportConfiguration(newSchemeFileName);
    m_schemesList->addItem(newName);
    m_schemesList->setCurrentIndex(m_schemesList->findText(newName));
    m_schemeFileLocations.insert(newName, newSchemeFileName);
    updateDeleteButton();
    emit shortcutsSchemeChanged(newName);
}

void KisKShortcutSchemesEditor::deleteScheme()
{
    if (KMessageBox::questionYesNo(m_dialog,
                                   i18n("Do you really want to delete the scheme %1?\n\
Note that this will not remove any system wide shortcut schemes.", currentScheme())) == KMessageBox::No) {
        return;
    }

    //delete the scheme for the app itself
    QFile::remove(KisKShortcutSchemesHelper::shortcutSchemeFileName(currentScheme()));

    m_schemesList->removeItem(m_schemesList->findText(currentScheme()));
    updateDeleteButton();
    emit shortcutsSchemeChanged(currentScheme());
}

QString KisKShortcutSchemesEditor::currentScheme()
{
    return m_schemesList->currentText();
}

void KisKShortcutSchemesEditor::exportShortcutsScheme()
{
    KConfigGroup group =  KSharedConfig::openConfig()->group("File Dialogs");
    QString proposedPath = group.readEntry("ExportShortcuts", KoResourcePaths::saveLocation("kis_shortcuts"));

    KoFileDialog dialog(m_dialog, KoFileDialog::SaveFile, "ExportShortcuts");
    dialog.setCaption(i18n("Export Shortcuts"));
    dialog.setDefaultDir(proposedPath);
    dialog.setMimeTypeFilters(QStringList() << "application/x-krita-shortcuts", "application/x-krita-shortcuts");
    QString path = dialog.filename();

    if (!path.isEmpty()) {
        m_dialog->exportConfiguration(path);
    }
}

void KisKShortcutSchemesEditor::saveCustomShortcuts()
{
    KConfigGroup group =  KSharedConfig::openConfig()->group("File Dialogs");
    QString proposedPath = group.readEntry("SaveCustomShortcuts", QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    KoFileDialog dialog(m_dialog, KoFileDialog::SaveFile, "SaveCustomShortcuts");
    dialog.setCaption(i18n("Save Shortcuts"));
    dialog.setDefaultDir(proposedPath);
    dialog.setMimeTypeFilters(QStringList() << "application/x-krita-shortcuts", "application/x-krita-shortcuts");
    QString path = dialog.filename();

    if (!path.isEmpty()) {
        m_dialog->saveCustomShortcuts(path);
    }
}



void KisKShortcutSchemesEditor::loadCustomShortcuts()
{
    KConfigGroup group =  KSharedConfig::openConfig()->group("File Dialogs");
    QString proposedPath = group.readEntry("ImportShortcuts", QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    KoFileDialog dialog(m_dialog, KoFileDialog::ImportFile, "ImportShortcuts");
    dialog.setCaption(i18n("Import Shortcuts"));
    dialog.setDefaultDir(proposedPath);
    dialog.setMimeTypeFilters(QStringList() << "application/x-krita-shortcuts", "application/x-krita-shortcuts");
    QString path = dialog.filename();

    if (path.isEmpty()) {
        return;
    }

    // auto ar = KisActionRegistry::instance();
    // ar->loadCustomShortcuts(path);
    m_dialog->loadCustomShortcuts(path);

}

void KisKShortcutSchemesEditor::importShortcutsScheme()
{
    KConfigGroup group =  KSharedConfig::openConfig()->group("File Dialogs");
    QString proposedPath = group.readEntry("ImportShortcuts", QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    KoFileDialog dialog(m_dialog, KoFileDialog::ImportFile, "ImportShortcuts");
    dialog.setCaption(i18n("Import Shortcuts"));
    dialog.setDefaultDir(proposedPath);
    dialog.setMimeTypeFilters(QStringList() << "application/x-krita-shortcuts", "application/x-krita-shortcuts");
    QString path = dialog.filename();

    if (path.isEmpty()) {
        return;
    }

    m_dialog->importConfiguration(path);
}

#if 0
// XXX: Not implemented
void KisKShortcutSchemesEditor::saveAsDefaultsForScheme()
{
    foreach (KisKActionCollection *collection, m_dialog->actionCollections()) {
        KisKShortcutSchemesHelper::exportActionCollection(collection, currentScheme());
    }
}
#endif

void KisKShortcutSchemesEditor::updateDeleteButton()
{
    m_deleteScheme->setEnabled(m_schemesList->count() >= 1);
}
