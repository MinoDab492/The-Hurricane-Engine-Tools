/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisAsyncColorSamplerHelper.h"

#include <QPainter>

#include "KoCanvasResourcesIds.h"
#include "KoCanvasResourceProvider.h"
#include "KoViewConverter.h"
#include "KoIcon.h"
#include "kis_cursor.h"
#include "kis_signal_compressor_with_param.h"
#include "kis_image_interfaces.h"
#include "kis_config.h"
#include "kis_canvas2.h"
#include "KisViewManager.h"
#include "KisDocument.h"
#include "KisReferenceImagesLayer.h"
#include "KisReferenceImagesDecoration.h"
#include "kis_display_color_converter.h"
#include "strokes/kis_color_sampler_stroke_strategy.h"


namespace {
std::pair<QRectF,QRectF> colorPreviewDocRectImpl(const QPointF &outlineDocPoint, bool colorPreviewShowComparePlate, const KoViewConverter *converter)
{
    KisConfig cfg(true);
    const QRectF colorPreviewViewRect = cfg.colorPreviewRect();

    const QRectF colorPreviewBaseColorViewRect =
        colorPreviewShowComparePlate ?
            colorPreviewViewRect.translated(colorPreviewViewRect.width(), 0) :
            QRectF();

    const QRectF colorPreviewDocumentRect = converter->viewToDocument(colorPreviewViewRect);
    const QRectF colorPreviewBaseColorDocumentRect =
        converter->viewToDocument(colorPreviewBaseColorViewRect);

    return std::make_pair(colorPreviewDocumentRect.translated(outlineDocPoint),
                          colorPreviewBaseColorDocumentRect.translated(outlineDocPoint));
}
}

struct KisAsyncColorSamplerHelper::Private
{
    Private(KisCanvas2 *_canvas)
        : canvas(_canvas)
    {}

    KisCanvas2 *canvas;

    int sampleResourceId {0};
    bool sampleCurrentLayer {true};
    bool updateGlobalColor {true};

    bool isActive {false};
    bool showPreview {false};
    bool showComparePlate {false};

    KisStrokeId strokeId;
    typedef KisSignalCompressorWithParam<QPointF> SamplingCompressor;
    QScopedPointer<SamplingCompressor> samplingCompressor;

    QTimer activationDelayTimer;

    QRectF currentColorDocRect;
    QRectF baseColorDocRect;

    QColor currentColor;
    QColor baseColor;

    KisStrokesFacade *strokesFacade() const {
        return canvas->image().data();
    }

    const KoViewConverter &converter() const {
        return *canvas->imageView()->viewConverter();
    }

};

KisAsyncColorSamplerHelper::KisAsyncColorSamplerHelper(KisCanvas2 *canvas)
    : m_d(new Private(canvas))
{
    using namespace std::placeholders; // For _1 placeholder
    std::function<void(QPointF)> callback =
        std::bind(&KisAsyncColorSamplerHelper::slotAddSamplingJob, this, _1);
    m_d->samplingCompressor.reset(
        new Private::SamplingCompressor(100, callback, KisSignalCompressor::FIRST_ACTIVE));

    m_d->activationDelayTimer.setInterval(100);
    m_d->activationDelayTimer.setSingleShot(true);
    connect(&m_d->activationDelayTimer, SIGNAL(timeout()), this, SLOT(activateDelayedPreview()));
}

KisAsyncColorSamplerHelper::~KisAsyncColorSamplerHelper()
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(!m_d->strokeId);
}

bool KisAsyncColorSamplerHelper::isActive() const
{
    return m_d->isActive;
}

void KisAsyncColorSamplerHelper::activate(bool sampleCurrentLayer, bool pickFgColor)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_d->isActive);
    m_d->isActive = true;

    m_d->sampleResourceId =
        pickFgColor ?
            KoCanvasResource::ForegroundColor :
            KoCanvasResource::BackgroundColor;

    m_d->sampleCurrentLayer = sampleCurrentLayer;
    m_d->showComparePlate = false;

    m_d->activationDelayTimer.start();
}

void KisAsyncColorSamplerHelper::activateDelayedPreview()
{
    // the event may come after we have finished color
    // picking if the user is quick
    if (!m_d->isActive) return;

    m_d->showPreview = true;

    const KoColor currentColor =
        m_d->canvas->resourceManager()->koColorResource(m_d->sampleResourceId);
    const QColor previewColor = m_d->canvas->displayColorConverter()->toQColor(currentColor);

    m_d->currentColor = previewColor;
    m_d->baseColor = previewColor;

    updateCursor(m_d->sampleCurrentLayer, m_d->sampleResourceId == KoCanvasResource::ForegroundColor);

    Q_EMIT sigRequestUpdateOutline();
}

void KisAsyncColorSamplerHelper::updateCursor(bool sampleCurrentLayer, bool pickFgColor)
{
    const int sampleResourceId =
            pickFgColor ?
                KoCanvasResource::ForegroundColor :
                KoCanvasResource::BackgroundColor;

    QCursor cursor;

    if (sampleCurrentLayer) {
        if (sampleResourceId == KoCanvasResource::ForegroundColor) {
            cursor = KisCursor::samplerLayerForegroundCursor();
        } else {
            cursor = KisCursor::samplerLayerBackgroundCursor();
        }
    } else {
        if (sampleResourceId == KoCanvasResource::ForegroundColor) {
            cursor = KisCursor::samplerImageForegroundCursor();
        } else {
            cursor = KisCursor::samplerImageBackgroundCursor();
        }
    }

    Q_EMIT sigRequestCursor(cursor);
}

void KisAsyncColorSamplerHelper::setUpdateGlobalColor(bool value)
{
    m_d->updateGlobalColor = value;
}

bool KisAsyncColorSamplerHelper::updateGlobalColor() const
{
    return m_d->updateGlobalColor;
}

void KisAsyncColorSamplerHelper::deactivate()
{
    KIS_SAFE_ASSERT_RECOVER(!m_d->strokeId) {
        endAction();
    }

    m_d->activationDelayTimer.stop();

    m_d->showPreview = false;
    m_d->showComparePlate = false;

    m_d->currentColorDocRect = QRectF();
    m_d->currentColor = QColor();
    m_d->baseColor = QColor();
    m_d->baseColorDocRect = QRectF();

    m_d->isActive = false;

    Q_EMIT sigRequestCursorReset();
    Q_EMIT sigRequestUpdateOutline();
}

void KisAsyncColorSamplerHelper::startAction(const QPointF &docPoint, int radius, int blend)
{
    KisColorSamplerStrokeStrategy *strategy = new KisColorSamplerStrokeStrategy(radius, blend);
    connect(strategy, &KisColorSamplerStrokeStrategy::sigColorUpdated,
            this, &KisAsyncColorSamplerHelper::slotColorSamplingFinished);
    connect(strategy, &KisColorSamplerStrokeStrategy::sigFinalColorSelected,
            this, &KisAsyncColorSamplerHelper::sigFinalColorSelected);

    m_d->strokeId = m_d->strokesFacade()->startStroke(strategy);
    m_d->samplingCompressor->start(docPoint);
}

void KisAsyncColorSamplerHelper::continueAction(const QPointF &docPoint)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->strokeId);
    m_d->samplingCompressor->start(docPoint);
}

void KisAsyncColorSamplerHelper::endAction()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->strokeId);

    m_d->strokesFacade()->addJob(m_d->strokeId,
        new KisColorSamplerStrokeStrategy::FinalizeData());

    m_d->strokesFacade()->endStroke(m_d->strokeId);
    m_d->strokeId.clear();
}

QRectF KisAsyncColorSamplerHelper::colorPreviewDocRect(const QPointF &docPoint)
{
    if (!m_d->showPreview) return QRectF();

    std::tie(m_d->currentColorDocRect, m_d->baseColorDocRect) =
            colorPreviewDocRectImpl(docPoint, m_d->showComparePlate, &m_d->converter());

    return m_d->currentColorDocRect | m_d->baseColorDocRect;
}

void KisAsyncColorSamplerHelper::paint(QPainter &gc, const KoViewConverter &converter)
{
    if (!m_d->showPreview) return;

    const QRectF viewRect = converter.documentToView(m_d->currentColorDocRect);
    gc.fillRect(viewRect, m_d->currentColor);

    if (m_d->showComparePlate) {
        const QRectF baseColorRect = converter.documentToView(m_d->baseColorDocRect);
        gc.fillRect(baseColorRect, m_d->baseColor);
    }
}

void KisAsyncColorSamplerHelper::slotAddSamplingJob(const QPointF &docPoint)
{
    /**
     * The actual sampling is delayed by a compressor, so we can get this
     * event when the stroke is already closed
     */
    if (!m_d->strokeId) return;

    KisImageSP image = m_d->canvas->image();

    const QPoint imagePoint = image->documentToImagePixelFloored(docPoint);

    if (!m_d->sampleCurrentLayer) {
        KisSharedPtr<KisReferenceImagesLayer> referencesLayer = m_d->canvas->imageView()->document()->referenceImagesLayer();
        if (referencesLayer && m_d->canvas->referenceImagesDecoration()->visible()) {
            QColor color = referencesLayer->getPixel(imagePoint);
            if (color.isValid() && color.alpha() != 0) {
                slotColorSamplingFinished(KoColor(color, image->colorSpace()));
                return;
            }
        }
    }

    KisPaintDeviceSP device = m_d->sampleCurrentLayer ?
        m_d->canvas->imageView()->currentNode()->colorSampleSourceDevice() :
        image->projection();

    if (device) {
        // Used for color sampler blending.
        const KoColor currentColor =
            m_d->canvas->resourceManager()->koColorResource(m_d->sampleResourceId);

        m_d->strokesFacade()->addJob(m_d->strokeId,
            new KisColorSamplerStrokeStrategy::Data(device, imagePoint, currentColor));
    } else {
        QString message = i18n("Color sampler does not work on this layer.");
        m_d->canvas->viewManager()->showFloatingMessage(message, koIcon("object-locked"));
    }
}

void KisAsyncColorSamplerHelper::slotColorSamplingFinished(const KoColor &rawColor)
{
    KoColor color(rawColor);

    color.setOpacity(OPACITY_OPAQUE_U8);

    if (m_d->updateGlobalColor) {
        m_d->canvas->resourceManager()->setResource(m_d->sampleResourceId, color);
    }

    Q_EMIT sigRawColorSelected(rawColor);
    Q_EMIT sigColorSelected(color);

    if (!m_d->showPreview) return;

    const QColor previewColor = m_d->canvas->displayColorConverter()->toQColor(color);

    m_d->showComparePlate = true;
    m_d->currentColor = previewColor;

    Q_EMIT sigRequestUpdateOutline();
}
