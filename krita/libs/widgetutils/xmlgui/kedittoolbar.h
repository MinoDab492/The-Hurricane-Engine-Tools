/* This file is part of the KDE libraries
   SPDX-FileCopyrightText: 2000 Kurt Granroth <granroth@kde.org>
   SPDX-FileCopyrightText: 2006 Hamish Rodda <rodda@kde.org>

   SPDX-License-Identifier: LGPL-2.0-only
*/
#ifndef KEDITTOOLBAR_H
#define KEDITTOOLBAR_H

#include <QDialog>

#include <kritawidgetutils_export.h>

class KisKActionCollection;

class KisKEditToolBarPrivate;
class KisKXMLGUIFactory;
/**
 * @short A dialog used to customize or configure toolbars.
 *
 * This dialog only works if your application uses the XML UI
 * framework for creating menus and toolbars.  It depends on the XML
 * files to describe the toolbar layouts and it requires the actions
 * to determine which buttons are active.
 *
 * Typically you do not need to use it directly as KXmlGuiWindow::setupGUI
 * takes care of it.
 *
 * If you use plugListAction you need to overload saveNewToolbarConfig()
 * to plug actions again:
 *
 * \code
 * void MyClass::saveNewToolbarConfig()
 * {
 *   KXmlGuiWindow::saveNewToolbarConfig();
 *   plugActionList( "list1", list1Actions );
 *   plugActionList( "list2", list2Actions );
 * }
 * \endcode
 *
 * When created, KisKEditToolBar takes a KisKXMLGUIFactory object, and uses it to
 * find all of the action collections and XML files (there is one of each for the
 * mainwindow, but there could be more, when adding other XMLGUI clients like
 * KParts or plugins). The editor aims to be semi-intelligent about where it
 * assigns any modifications. In other words, it will not write out part specific
 * changes to your application's main XML file.
 *
 * KXmlGuiWindow and KParts::MainWindow take care of creating KisKEditToolBar correctly
 * and connecting to its newToolBarConfig slot, but if you really really want to do it
 * yourself, see the KXmlGuiWindow::configureToolbars() and KXmlGuiWindow::saveNewToolbarConfig() code.
 *
 * \image html kedittoolbar.png "KDE Toolbar Editor (KWrite)"
 *
 * @author Kurt Granroth <granroth@kde.org>
 * @maintainer David Faure <faure@kde.org>
 */
class KRITAWIDGETUTILS_EXPORT KisKEditToolBar : public QDialog
{
    Q_OBJECT
public:
    /**
     * Main constructor.
     *
     * The main parameter, @p factory, is a pointer to the
     * XML GUI factory object for your application.  It contains a list
     * of all of the GUI clients (along with the action collections and
     * xml files) and the toolbar editor uses that.
     *
     * Use this like so:
     * \code
     * KisKEditToolBar edit(factory());
     * if (edit.exec())
     * ...
     * \endcode
     *
     * @param factory Your application's factory object
     * @param parent The usual parent for the dialog.
     */
    explicit KisKEditToolBar(KisKXMLGUIFactory *factory,
                          QWidget *parent = 0);

    /// destructor
    ~KisKEditToolBar() override;

    /**
     * Sets the default toolbar that will be selected when the dialog is shown.
     * If not set, or QString() is passed in, the global default tool bar name
     * will be used.
     * @param toolBarName the name of the tool bar
     * @see setGlobalDefaultToolBar
     */
    void setDefaultToolBar(const QString &toolBarName);

    /**
     * The name (absolute or relative) of your application's UI resource file
     * is assumed to be share/apps/appname/appnameui.xmlgui though this can be
     * overridden by calling this method.
     *
     * The global parameter controls whether or not the
     * global resource file is used.  If this is @p true, then you may
     * edit all of the actions in your toolbars -- global ones and
     * local one.  If it is @p false, then you may edit only your
     * application's entries.  The only time you should set this to
     * false is if your application does not use the global resource
     * file at all (very rare).
     *
     * @param file The application's local resource file.
     * @param global If @p true, then the global resource file will also
     *               be parsed.
     */
    void setResourceFile(const QString &file, bool global = true);

    /**
     * Sets the default toolbar which will be auto-selected for all
     * KisKEditToolBar instances. Can be overridden on a per-dialog basis
     * by calling setDefaultToolBar( const QString& ) on the dialog.
     *   @param  toolBarName  the name of the tool bar
     */
    static void setGlobalDefaultToolBar(const char *toolBarName); // TODO should be const QString&

Q_SIGNALS:
    /**
     * Signal emitted when 'apply' or 'ok' is clicked or toolbars were reset.
     * Connect to it, to plug action lists and to call applyMainWindowSettings
     * (see sample code in this class's documentation)
     */
    void newToolBarConfig();

    QT_MOC_COMPAT void newToolbarConfig();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    friend class KisKEditToolBarPrivate;
    KisKEditToolBarPrivate *const d;

    Q_PRIVATE_SLOT(d, void _k_slotButtonClicked(QAbstractButton *))
    Q_PRIVATE_SLOT(d, void _k_acceptOK(bool))
    Q_PRIVATE_SLOT(d, void _k_enableApply(bool))

    Q_DISABLE_COPY(KisKEditToolBar)
};

#endif // _KEDITTOOLBAR_H
