/*
 *  SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2004 Sven Langkamp <sven.langkamp@gmail.com>
 *  SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_SEGMENT_GRADIENT_SLIDER_H_
#define _KIS_SEGMENT_GRADIENT_SLIDER_H_

#include <QWidget>

#include <KoSegmentGradient.h>
#include <kis_signal_compressor.h>

class QAction;
class QMenu;

class KoGradientSegment;

#include "kritaui_export.h"

/**
 * @brief The KisSegmentGradientSlider class makes it possible to edit gradients.
 */
class KRITAUI_EXPORT KisSegmentGradientSlider : public QWidget
{
    Q_OBJECT

public:
    enum HandleType
    {
        HandleType_None,
        HandleType_Segment,
        HandleType_MidPoint,
        HandleType_Stop
    };

    struct Handle
    {
        HandleType type{HandleType_None};
        int index{0};
    };

    static constexpr qreal shrinkEpsilon = 0.00001;

    KisSegmentGradientSlider(QWidget *parent = nullptr, const char* name = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

    Handle selectedHandle() { return m_selectedHandle; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public Q_SLOTS:
    void setGradientResource(KoSegmentGradientSP agr);
    void chooseSelectedStopColor();
    void selectPreviousHandle();
    void selectNextHandle();
    void moveHandle(Handle handle, qreal distance, bool useShrinkEpsilon = true);
    void moveHandleLeft(Handle handle, qreal distance, bool useShrinkEpsilon = true);
    void moveHandleRight(Handle handle, qreal distance, bool useShrinkEpsilon = true);
    void moveSelectedHandle(qreal distance, bool useShrinkEpsilon = true);
    void moveSelectedHandleLeft(qreal distance, bool useShrinkEpsilon = true);
    void moveSelectedHandleRight(qreal distance, bool useShrinkEpsilon = true);
    void deleteHandle(Handle handle);
    void centerSelectedHandle();
    void deleteSelectedHandle();
    void collapseSelectedSegment();
    void splitSelectedSegment();
    void duplicateSelectedSegment();
    void mirrorSelectedSegment();
    void flipGradient();
    void distributeStopsEvenly();
    
Q_SIGNALS:
    void selectedHandleChanged();
    void updateRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent * e) override;
    void mouseReleaseEvent(QMouseEvent * e) override;
    void mouseMoveEvent(QMouseEvent * e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void leaveEvent(QEvent *e) override;

private Q_SLOTS:
    void updateHandleSize();

private:
    struct TemporallyDeletedHandleInfo
    {
        Handle handle {HandleType_None, 0};
        KoGradientSegmentEndpointType leftEndPointType {COLOR_ENDPOINT};
        KoGradientSegmentEndpointType rightEndPointType {COLOR_ENDPOINT};
        qreal leftEndPointOffset {0.0};
        qreal rightEndPointOffset {0.0};
        KoColor leftEndPointColor;
        KoColor rightEndPointColor;
        int leftInterpolationType {0};
        int rightInterpolationType {0};
        int leftColorInterpolationType {0};
        int rightColorInterpolationType {0};
        qreal leftMiddleOffset {0.0};
        qreal rightMiddleOffset {0.0};
    };

    // This epsilon controls how much can shrink a segment when dragging
    static constexpr int removeStopDistance{32};
    KoSegmentGradientSP m_gradient;
    Handle m_selectedHandle {HandleType_None, 0};
    Handle m_hoveredHandle {HandleType_None, 0};
    QMenu* m_segmentMenu {nullptr};
    bool m_drag {false};
    qreal m_dragT {0.0};
    qreal m_relativeDragOffset {0.0};
    QAction *m_removeSegmentAction {nullptr};
    QSize m_handleSize;
    TemporallyDeletedHandleInfo m_temporallyDeletedHandleInfo;
    KisSignalCompressor m_updateCompressor;

    QRect sliderRect() const;
    QRect gradientStripeRect() const;
    QRect handlesStripeRect() const;
    int minimalHeight() const;
    void handleIncrementInput(int direction, Qt::KeyboardModifiers modifiers);
    bool deleteHandleImpl(Handle handle);
};

#endif
