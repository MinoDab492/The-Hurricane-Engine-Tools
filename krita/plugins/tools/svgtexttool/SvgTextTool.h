/* This file is part of the KDE project
 *
   SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SVG_TEXT_TOOL
#define SVG_TEXT_TOOL

#include <KConfigGroup>
#include <KoToolBase.h>
#include <QPushButton>
#include <QFontComboBox>
#include <QPointer>
#include <QDoubleSpinBox>

#include "SvgTextEditor.h"

class KoSelection;
class SvgTextEditor;
class KoSvgTextShape;

class SvgTextTool : public KoToolBase
{
    Q_OBJECT
public:
    explicit SvgTextTool(KoCanvasBase *canvas);
    ~SvgTextTool() override;
    /// reimplemented from KoToolBase
    void paint(QPainter &gc, const KoViewConverter &converter) override;
    /// reimplemented from KoToolBase
    void mousePressEvent(KoPointerEvent *event) override;
    /// reimplemented from superclass
    void mouseDoubleClickEvent(KoPointerEvent *event) override;
    /// reimplemented from KoToolBase
    void mouseMoveEvent(KoPointerEvent *event) override;
    /// reimplemented from KoToolBase
    void mouseReleaseEvent(KoPointerEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    /// reimplemented from KoToolBase
    void activate(const QSet<KoShape *> &shapes) override;
    /// reimplemented from KoToolBase
    void deactivate() override;

    KisPopupWidgetInterface* popupWidget() override;

protected:
    /// reimplemented from KoToolBase
    virtual QWidget *createOptionWidget() override;

    KoSelection *koSelection() const;
    KoSvgTextShape *selectedShape() const;

private Q_SLOTS:

    void showEditor();
    void slotTextEditorClosed();
    void textUpdated(KoSvgTextShape *shape, const QString &svg, const QString &defs, bool richTextUpdated);

    /**
     * @brief generateDefs
     * This generates a defs section with the appropriate
     * css and css strings assigned. This allows the artist
     * to select settings that new texts will be created with.
     * @return a string containing the defs.
     */
    QString generateDefs();

    /**
     * @brief storeDefaults
     * store default font and point size when they change.
     */
    void storeDefaults();

private:
    QPointer<SvgTextEditor> m_editor;
    QPushButton *m_edit {nullptr};
    QPointF m_dragStart {QPointF(0, 0)};
    QPointF m_dragEnd {QPointF(0, 0)};
    bool m_dragging {false};
    QFontComboBox *m_defFont {nullptr};
    QComboBox *m_defPointSize {nullptr};
    QButtonGroup *m_defAlignment {nullptr};
    QDoubleSpinBox *m_defLetterSpacing {nullptr};
    KConfigGroup m_configGroup;

    QRectF m_hoveredShapeHighlightRect;
    boost::optional<KoColor> m_originalColor { boost::none };
};

#endif
