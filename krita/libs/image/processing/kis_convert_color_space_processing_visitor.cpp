/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_convert_color_space_processing_visitor.h"

#include "kis_external_layer_iface.h"

#include "kis_do_something_command.h"
#include "kis_paint_device.h"
#include "kis_transaction.h"
#include "kis_undo_adapter.h"
#include "kis_transform_mask.h"
#include "lazybrush/kis_colorize_mask.h"

#include "kis_filter_mask.h"
#include "kis_adjustment_layer.h"
#include "kis_group_layer.h"
#include "kis_paint_layer.h"
#include "kis_projection_leaf.h"
#include "kis_time_span.h"
#include <KoColorConversionTransformation.h>
#include <KoUpdater.h>
#include <commands_new/KisChangeChannelFlagsCommand.h>
#include <commands_new/KisChangeChannelLockFlagsCommand.h>
#include <commands_new/KisResetGroupLayerCacheCommand.h>
#include <kis_do_something_command.h>

KisConvertColorSpaceProcessingVisitor::KisConvertColorSpaceProcessingVisitor(const KoColorSpace *srcColorSpace,
                                                                             const KoColorSpace *dstColorSpace,
                                                                             KoColorConversionTransformation::Intent renderingIntent,
                                                                             KoColorConversionTransformation::ConversionFlags conversionFlags)
    : m_srcColorSpace(srcColorSpace)
    , m_dstColorSpace(dstColorSpace)
    , m_renderingIntent(renderingIntent)
    , m_conversionFlags(conversionFlags)
{
}

void KisConvertColorSpaceProcessingVisitor::visitExternalLayer(KisExternalLayer *layer, KisUndoAdapter *undoAdapter)
{
    KisProcessingVisitor::ProgressHelper helper(layer);
    KoUpdater *updater = helper.updater();
    undoAdapter->addCommand(layer->convertTo(m_dstColorSpace, m_renderingIntent, m_conversionFlags));
    updater->setProgress(100);
}

void KisConvertColorSpaceProcessingVisitor::visit(KisAdjustmentLayer *layer, KisUndoAdapter *undoAdapter)
{
    using namespace KisDoSomethingCommandOps;
    undoAdapter->addCommand(new KisDoSomethingCommand<NotifyColorSpaceChangedOp, KisAdjustmentLayer*>(layer, false));
    KisSimpleProcessingVisitor::visit(layer, undoAdapter);
    undoAdapter->addCommand(new KisDoSomethingCommand<NotifyColorSpaceChangedOp, KisAdjustmentLayer*>(layer, true));
}

void KisConvertColorSpaceProcessingVisitor::visit(KisFilterMask *mask, KisUndoAdapter *undoAdapter)
{
    using namespace KisDoSomethingCommandOps;
    undoAdapter->addCommand(new KisDoSomethingCommand<NotifyColorSpaceChangedOp, KisFilterMask*>(mask, false));
    KisSimpleProcessingVisitor::visit(mask, undoAdapter);
    undoAdapter->addCommand(new KisDoSomethingCommand<NotifyColorSpaceChangedOp, KisFilterMask*>(mask, true));
}

void KisConvertColorSpaceProcessingVisitor::visitNodeWithPaintDevice(KisNode *node, KisUndoAdapter *undoAdapter)
{
    if (!node->projectionLeaf()->isLayer()) return;
    if (*m_dstColorSpace == *node->colorSpace()) return;

    bool alphaLock = false;
    bool alphaDisabled = false;

    KisLayer *layer = dynamic_cast<KisLayer*>(node);
    KIS_SAFE_ASSERT_RECOVER_RETURN(layer);

    KisProcessingVisitor::ProgressHelper helper(layer);

    KisPaintLayer *paintLayer = 0;

    KUndo2Command *parentConversionCommand = new KUndo2Command();

    if (m_srcColorSpace->colorModelId() != m_dstColorSpace->colorModelId()) {
        alphaDisabled = layer->alphaChannelDisabled();
        new KisChangeChannelFlagsCommand(QBitArray(), layer, parentConversionCommand);
        if ((paintLayer = dynamic_cast<KisPaintLayer*>(layer))) {
            alphaLock = paintLayer->alphaLocked();
            new KisChangeChannelLockFlagsCommand(QBitArray(), paintLayer, parentConversionCommand);
        }
    }

    if (layer->original()) {
        layer->original()->convertTo(m_dstColorSpace, m_renderingIntent, m_conversionFlags, parentConversionCommand, helper.updater());
    }

    if (layer->paintDevice()) {
        layer->paintDevice()->convertTo(m_dstColorSpace, m_renderingIntent, m_conversionFlags, parentConversionCommand, helper.updater());
    }

    if (layer->projection()) {
        layer->projection()->convertTo(m_dstColorSpace, m_renderingIntent, m_conversionFlags, parentConversionCommand, helper.updater());
    }

    if (layer && alphaDisabled) {
        new KisChangeChannelFlagsCommand(m_dstColorSpace->channelFlags(true, false),
                                         layer, parentConversionCommand);
    }

    if (paintLayer && alphaLock) {
        new KisChangeChannelLockFlagsCommand(m_dstColorSpace->channelFlags(true, false),
                                             paintLayer, parentConversionCommand);
    }

    undoAdapter->addCommand(parentConversionCommand);
    layer->invalidateFrames(KisTimeSpan::infinite(0), layer->extent());
}

void KisConvertColorSpaceProcessingVisitor::visit(KisGroupLayer *layer, KisUndoAdapter *undoAdapter)
{
    // Group Layers determine their color space from their paint device, thus before setting
    // channel flags, it must be reset to the new CS first.

    bool alphaDisabled = layer->alphaChannelDisabled();

    /// Group layers are supposed to always be in the image colorspace,
    /// having groups in any other color space is not yet supported by
    /// .kra file format
    const KoColorSpace *srcColorSpace = layer->colorSpace();
    const KoColorSpace *dstColorSpace = layer->image() ? layer->image()->colorSpace() : m_dstColorSpace;

    // the swap of FINALIZING/INITIALIZING is intentional here, see the comment above
    undoAdapter->addCommand(new KisResetGroupLayerCacheCommand(layer, dstColorSpace, KisResetGroupLayerCacheCommand::FINALIZING));

    if (srcColorSpace->colorModelId() != dstColorSpace->colorModelId()) {
        QBitArray channelFlags;

        if (alphaDisabled) {
            channelFlags = dstColorSpace->channelFlags(true, false);
        }

        undoAdapter->addCommand(new KisChangeChannelFlagsCommand(channelFlags, layer));
    }

    undoAdapter->addCommand(new KisResetGroupLayerCacheCommand(layer, srcColorSpace, KisResetGroupLayerCacheCommand::INITIALIZING));
}

void KisConvertColorSpaceProcessingVisitor::visit(KisTransformMask *node, KisUndoAdapter *undoAdapter)
{
    node->threadSafeForceStaticImageUpdate();
    KisSimpleProcessingVisitor::visit(node, undoAdapter);
}

void KisConvertColorSpaceProcessingVisitor::visitColorizeMask(KisColorizeMask *node, KisUndoAdapter *undoAdapter)
{
    KisProcessingVisitor::ProgressHelper helper(dynamic_cast<KisNode *>(node));
    KoUpdater *updater = helper.updater();
    undoAdapter->addCommand(node->setColorSpace(m_dstColorSpace, m_renderingIntent, m_conversionFlags, updater));
    node->invalidateFrames(KisTimeSpan::infinite(0), node->extent());
}
