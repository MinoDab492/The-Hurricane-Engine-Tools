/*
 *  LayerBox.cc - part of Krita aka Krayon aka KimageShop
 *
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2006 Gábor Lehel <illissius@gmail.com>
 *  SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2011 José Luis Vergara <pentalis@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "LayerBox.h"

#include <QApplication>
#include <QToolButton>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QToolTip>
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QPixmap>
#include <QBitmap>
#include <QList>
#include <QVector>
#include <QLabel>
#include <QMenu>
#include <QWidgetAction>
#include <QProxyStyle>

#include <kis_debug.h>
#include <klocalizedstring.h>

#include <kis_icon.h>
#include <KoColorSpace.h>
#include <KoCompositeOpRegistry.h>
#include <KisDocument.h>
#include <kis_time_span.h>

#include <kis_types.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_layer.h>
#include <kis_group_layer.h>
#include <kis_mask.h>
#include <kis_node.h>
#include <kis_base_node.h>
#include <kis_composite_ops_model.h>
#include <kis_keyframe_channel.h>
#include <kis_image_animation_interface.h>
#include <KoProperties.h>

#include <kis_action.h>
#include "kis_action_manager.h"
#include "widgets/kis_cmb_composite.h"
#include "kis_slider_spin_box.h"
#include "KisViewManager.h"
#include "kis_node_manager.h"
#include "kis_node_model.h"
#include <kis_clipboard.h>

#include "canvas/kis_canvas2.h"
#include "kis_dummies_facade_base.h"
#include "kis_shape_controller.h"
#include "kis_selection_mask.h"
#include "kis_config.h"
#include "KisView.h"
#include "krita_utils.h"
#include "kis_color_label_selector_widget.h"
#include "kis_signals_blocker.h"
#include "kis_color_filter_combo.h"
#include "kis_node_filter_proxy_model.h"

#include "kis_selection.h"
#include "kis_processing_applicator.h"
#include "commands/kis_set_global_selection_command.h"
#include "KisSelectionActionsAdapter.h"

#include "kis_layer_utils.h"

#include "ui_WdgLayerBox.h"
#include "NodeView.h"
#include "SyncButtonAndAction.h"


class LayerBoxStyle : public QProxyStyle
{
public:
    LayerBoxStyle(QStyle *baseStyle = 0) : QProxyStyle(baseStyle) {}

    void drawPrimitive(PrimitiveElement element,
                       const QStyleOption *option,
                       QPainter *painter,
                       const QWidget *widget) const override
    {
        if (element == QStyle::PE_IndicatorItemViewItemDrop)
        {
            QColor color(widget->palette().color(QPalette::Highlight).lighter());

            if (option->rect.height() == 0) {
                QBrush brush(color);

                QRect r(option->rect);
                r.setTop(r.top() - 2);
                r.setBottom(r.bottom() + 2);

                painter->fillRect(r, brush);
            } else {
                color.setAlpha(200);
                QBrush brush(color);
                painter->fillRect(option->rect, brush);
            }
        }
        else
        {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
        }
    }
};

inline void LayerBox::connectActionToButton(KisViewManager* viewManager, QAbstractButton *button, const QString &id)
{
    if (!viewManager || !button) return;

    KisAction *action = viewManager->actionManager()->actionByName(id);

    if (!action) return;

    connect(button, SIGNAL(clicked()), action, SLOT(trigger()));
    connect(action, SIGNAL(sigEnableSlaves(bool)), button, SLOT(setEnabled(bool)));
    connect(viewManager->mainWindowAsQWidget(), SIGNAL(themeChanged()), this, SLOT(slotUpdateIcons()));
}

inline void LayerBox::addActionToMenu(QMenu *menu, const QString &id)
{
    if (m_canvas) {
        menu->addAction(m_canvas->viewManager()->actionManager()->actionByName(id));
    }
}

qint32 LayerBox::convertOpacityToInt(qreal opacity)
{
    /**
     * Scales opacity from the range 0...100
     * to the integer range 0...255
     */

    return qMin(255, int(opacity * 2.55 + 0.5));
}

LayerBox::LayerBox()
    : QDockWidget(i18n("Layers"))
    , m_canvas(0)
    , m_wdgLayerBox(new Ui_WdgLayerBox)
    , m_thumbnailCompressor(500, KisSignalCompressor::FIRST_INACTIVE)
    , m_colorLabelCompressor(500, KisSignalCompressor::FIRST_INACTIVE)
    , m_thumbnailSizeCompressor(100, KisSignalCompressor::FIRST_INACTIVE)
    , m_treeIndentationCompressor(100, KisSignalCompressor::FIRST_INACTIVE)
    , m_subtitleOpacityCompressor(100, KisSignalCompressor::FIRST_INACTIVE)
{
    KisConfig cfg(false);

    QWidget* mainWidget = new QWidget(this);
    setWidget(mainWidget);
    m_opacityDelayTimer.setSingleShot(true);

    m_wdgLayerBox->setupUi(mainWidget);

    m_wdgLayerBox->listLayers->setStyle(new LayerBoxStyle(m_wdgLayerBox->listLayers->style()));

    connect(m_wdgLayerBox->listLayers,
            SIGNAL(contextMenuRequested(QPoint,QModelIndex)),
            this, SLOT(slotContextMenuRequested(QPoint,QModelIndex)));
    connect(m_wdgLayerBox->listLayers,
            SIGNAL(collapsed(QModelIndex)), SLOT(slotCollapsed(QModelIndex)));
    connect(m_wdgLayerBox->listLayers,
            SIGNAL(expanded(QModelIndex)), SLOT(slotExpanded(QModelIndex)));
    connect(m_wdgLayerBox->listLayers,
            SIGNAL(selectionChanged(QModelIndexList)), SLOT(selectionChanged(QModelIndexList)));

    slotUpdateIcons();

    m_wdgLayerBox->bnAdd->setIconSize(QSize(22, 22));
    m_wdgLayerBox->bnDelete->setIconSize(QSize(22, 22));
    m_wdgLayerBox->bnRaise->setIconSize(QSize(22, 22));
    m_wdgLayerBox->bnLower->setIconSize(QSize(22, 22));
    m_wdgLayerBox->bnProperties->setIconSize(QSize(22, 22));
    m_wdgLayerBox->bnDuplicate->setIconSize(QSize(22, 22));

    m_wdgLayerBox->bnLower->setEnabled(false);
    m_wdgLayerBox->bnRaise->setEnabled(false);

    if (cfg.sliderLabels()) {
        m_wdgLayerBox->opacityLabel->hide();
        m_wdgLayerBox->doubleOpacity->setPrefix(QString("%1:  ").arg(i18n("Opacity")));
    }
    m_wdgLayerBox->doubleOpacity->setRange(0, 100, 0);
    m_wdgLayerBox->doubleOpacity->setSuffix(i18n("%"));

    connect(m_wdgLayerBox->doubleOpacity, SIGNAL(valueChanged(qreal)), SLOT(slotOpacitySliderMoved(qreal)));
    connect(&m_opacityDelayTimer, SIGNAL(timeout()), SLOT(slotOpacityChanged()));

    connect(m_wdgLayerBox->cmbComposite, SIGNAL(activated(int)), SLOT(slotCompositeOpChanged(int)));

    m_newLayerMenu = new QMenu(this);
    m_wdgLayerBox->bnAdd->setMenu(m_newLayerMenu);
    m_wdgLayerBox->bnAdd->setPopupMode(QToolButton::MenuButtonPopup);

    m_opLayerMenu = new QMenu(this);
    m_wdgLayerBox->bnProperties->setMenu(m_opLayerMenu);
    m_wdgLayerBox->bnProperties->setPopupMode(QToolButton::MenuButtonPopup);

    m_nodeModel = new KisNodeModel(this, 1);
    m_filteringModel = new KisNodeFilterProxyModel(this);
    m_filteringModel->setNodeModel(m_nodeModel);

    /**
     * Connect model updateUI() to enable/disable controls.
     * Note: nodeActivated() is connected separately in setImage(), because
     *       it needs particular order of calls: first the connection to the
     *       node manager should be called, then updateUI()
     */
    connect(m_nodeModel, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(updateUI()));
    connect(m_nodeModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(updateUI()));
    connect(m_nodeModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), SLOT(updateUI()));
    connect(m_nodeModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(updateUI()));
    connect(m_nodeModel, SIGNAL(modelReset()), SLOT(slotModelReset()));

    connect(m_nodeModel, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(slotForgetAboutSavedNodeBeforeEditSelectionMode()));
    connect(m_nodeModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(slotForgetAboutSavedNodeBeforeEditSelectionMode()));
    connect(m_nodeModel, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), SLOT(slotForgetAboutSavedNodeBeforeEditSelectionMode()));
    connect(m_nodeModel, SIGNAL(modelReset()), SLOT(slotForgetAboutSavedNodeBeforeEditSelectionMode()));

    // we should update expanded state of the nodes on adding the nodes
    connect(m_nodeModel, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(slotNodeCollapsedChanged()));
    connect(m_nodeModel, SIGNAL(modelReset()), SLOT(slotNodeCollapsedChanged()));

    m_showGlobalSelectionMask = new KisAction(i18n("&Show Global Selection Mask"), this);
    m_showGlobalSelectionMask->setObjectName("show-global-selection-mask");
    m_showGlobalSelectionMask->setActivationFlags(KisAction::ACTIVE_IMAGE);
    m_showGlobalSelectionMask->setToolTip(i18nc("@info:tooltip", "Shows global selection as a usual selection mask in <b>Layers</b> docker"));
    m_showGlobalSelectionMask->setCheckable(true);
    connect(m_showGlobalSelectionMask, SIGNAL(triggered(bool)), SLOT(slotEditGlobalSelection(bool)));

    m_showGlobalSelectionMask->setChecked(cfg.showGlobalSelection());

    m_colorSelector = new KisColorLabelSelectorWidgetMenuWrapper(this);
    MouseClickIgnore* mouseEater = new MouseClickIgnore(this);
    m_colorSelector->installEventFilter(mouseEater);
    connect(m_colorSelector->colorLabelSelector(), SIGNAL(currentIndexChanged(int)), SLOT(slotColorLabelChanged(int)));
    m_colorSelectorAction = new QWidgetAction(this);
    m_colorSelectorAction->setDefaultWidget(m_colorSelector);

    connect(m_nodeModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            &m_colorLabelCompressor, SLOT(start()));

    m_wdgLayerBox->listLayers->setModel(m_filteringModel);
    // this connection should be done *after* the setModel() call to
    // happen later than the internal selection model
    connect(m_filteringModel.data(), &KisNodeFilterProxyModel::sigBeforeBeginRemoveRows,
            this, &LayerBox::slotAdjustCurrentBeforeRemoveRows);


    //LayerFilter Menu
    QMenu *layerFilterMenu = new QMenu(this);
    m_wdgLayerBox->bnLayerFilters->setMenu(layerFilterMenu);
    m_wdgLayerBox->bnLayerFilters->setPopupMode(QToolButton::InstantPopup);

    const QIcon filterIcon = KisIconUtils::loadIcon("view-filter");
    m_wdgLayerBox->bnLayerFilters->setIcon(filterIcon);
    m_wdgLayerBox->bnLayerFilters->setAutoRaise(true);
    QPixmap filterEnabledPixmap = filterIcon.pixmap(64,64);
    const QBitmap filterEnabledBitmask = filterEnabledPixmap.mask();
    filterEnabledPixmap.fill(palette().color(QPalette::Highlight));
    filterEnabledPixmap.setMask(filterEnabledBitmask);
    const QIcon filterEnabledIcon = QIcon(filterEnabledPixmap);

    layerFilterWidget = new KisLayerFilterWidget(this);
    connect(layerFilterWidget, SIGNAL(filteringOptionsChanged()), this, SLOT(updateLayerFiltering()));
    connect(layerFilterWidget, &KisLayerFilterWidget::filteringOptionsChanged, [this, filterIcon, filterEnabledIcon](){
        if(layerFilterWidget->isCurrentlyFiltering()) {
            m_wdgLayerBox->bnLayerFilters->setIcon(filterEnabledIcon);
        } else {
            m_wdgLayerBox->bnLayerFilters->setIcon(filterIcon);
        }

        m_wdgLayerBox->bnLayerFilters->setSelectedColors(QList<int>::fromSet(layerFilterWidget->getActiveColors()));
        m_wdgLayerBox->bnLayerFilters->setTextFilter(layerFilterWidget->hasTextFilter());
    });

    QWidgetAction *layerFilterMenuAction = new QWidgetAction(this);
    layerFilterMenuAction->setDefaultWidget(layerFilterWidget);
    layerFilterMenu->addAction(layerFilterMenuAction);

    setEnabled(false);

    connect(&m_thumbnailCompressor, SIGNAL(timeout()), SLOT(updateThumbnail()));
    connect(&m_colorLabelCompressor, SIGNAL(timeout()), SLOT(updateAvailableLabels()));


    // set up the configure menu for changing thumbnail size
    QMenu* configureMenu = new QMenu(this);
    configureMenu->setContentsMargins(6, 6, 6, 6);
    configureMenu->addSection(i18n("Thumbnail Size"));

    m_wdgLayerBox->configureLayerDockerToolbar->setMenu(configureMenu);
    m_wdgLayerBox->configureLayerDockerToolbar->setIconSize(QSize(16, 16));
    m_wdgLayerBox->configureLayerDockerToolbar->setPopupMode(QToolButton::InstantPopup);
    m_wdgLayerBox->configureLayerDockerToolbar->setAutoRaise(true);


    // add horizontal slider
    thumbnailSizeSlider = new QSlider(this);
    thumbnailSizeSlider->setOrientation(Qt::Horizontal);
    thumbnailSizeSlider->setRange(20, 80);

    thumbnailSizeSlider->setValue(cfg.layerThumbnailSize(false)); // grab this from the kritarc

    thumbnailSizeSlider->setMinimumHeight(20);
    thumbnailSizeSlider->setMinimumWidth(40);
    thumbnailSizeSlider->setTickInterval(5);


    QWidgetAction *sliderAction= new QWidgetAction(this);
    sliderAction->setDefaultWidget(thumbnailSizeSlider);
    configureMenu->addAction(sliderAction);


    connect(thumbnailSizeSlider, SIGNAL(valueChanged(int)), &m_thumbnailSizeCompressor, SLOT(start()));
    connect(&m_thumbnailSizeCompressor, SIGNAL(timeout()), SLOT(slotUpdateThumbnailIconSize()));

    configureMenu->addSection(i18nc("@item:inmenu Layers Docker settings, slider", "Tree Indentation"));

    // add horizontal slider
    indentationSlider = new QSlider(Qt::Horizontal, this);
    indentationSlider->setRange(20, 100);
    indentationSlider->setMinimumSize(40, 20);
    indentationSlider->setSingleStep(5);
    indentationSlider->setPageStep(20);
    indentationSlider->setValue(cfg.layerTreeIndentation());


    sliderAction= new QWidgetAction(this);
    sliderAction->setDefaultWidget(indentationSlider);
    configureMenu->addAction(sliderAction);

    // NOTE: if KisConfig would just compress its file sync events, we wouldn't need
    // this extra compressor that juggles between slow UI and disk thrashing
    connect(indentationSlider, SIGNAL(valueChanged(int)), &m_treeIndentationCompressor, SLOT(start()));
    connect(&m_treeIndentationCompressor, SIGNAL(timeout()), SLOT(slotUpdateTreeIndentation()));


    // Layer subtitle settings:
    // subtitle style combobox
    configureMenu->addSection(i18nc("@item:inmenu Layers Docker settings, combobox", "Subtitle Style"));
    subtitleCombobox = new QComboBox(this);
    subtitleCombobox->setToolTip(i18nc("@item:tooltip", "None: Show nothing.\n"
                                                        "Simple: Show changed opacities or blending modes.\n"
                                                        "Balanced: Show both opacity and blending mode if either are changed.\n"
                                                        "Detailed: Show both opacity and blending mode even if unchanged."));
    subtitleCombobox->insertItems(0, QStringList ({
        i18nc("@item:inlistbox Layer Docker subtitle style", "None"),
        i18nc("@item:inlistbox Layer Docker subtitle style", "Simple"),
        i18nc("@item:inlistbox Layer Docker subtitle style", "Balanced"),
        i18nc("@item:inlistbox Layer Docker subtitle style", "Detailed"),
    }));
    subtitleCombobox->setCurrentIndex((int)cfg.layerSubtitleStyle());

    QWidgetAction *cmbboxAction = new QWidgetAction(this);
    cmbboxAction->setDefaultWidget(subtitleCombobox);
    configureMenu->addAction(cmbboxAction);
    connect(subtitleCombobox, SIGNAL(currentIndexChanged(int)), SLOT(slotUpdateLayerSubtitleStyle()));

    // subtitle opacity slider
    subtitleOpacitySlider = new KisSliderSpinBox(this);
    subtitleOpacitySlider->setPrefix(QString("%1:  ").arg(i18n("Opacity")));
    subtitleOpacitySlider->setSuffix(i18n("%"));
    subtitleOpacitySlider->setToolTip(i18nc("@item:tooltip", "Subtitle text opacity"));
    // 55% is the opacity of nonvisible layer text
    subtitleOpacitySlider->setRange(55, 100);
    subtitleOpacitySlider->setMinimumSize(40, 20);
    subtitleOpacitySlider->setSingleStep(5);
    subtitleOpacitySlider->setPageStep(15);
    subtitleOpacitySlider->setValue(cfg.layerSubtitleOpacity());
    if (subtitleCombobox->currentIndex() == 0) {
        subtitleOpacitySlider->setDisabled(true);
    }

    sliderAction= new QWidgetAction(this);
    sliderAction->setDefaultWidget(subtitleOpacitySlider);
    configureMenu->addAction(sliderAction);
    connect(subtitleOpacitySlider, SIGNAL(valueChanged(int)), &m_subtitleOpacityCompressor, SLOT(start()));
    connect(&m_subtitleOpacityCompressor, SIGNAL(timeout()), SLOT(slotUpdateLayerSubtitleOpacity()));

    // subtitle inline checkbox
    subtitleInlineChkbox = new QCheckBox(i18nc("@item:inmenu Layers Docker settings, checkbox", "Inline"), this);
    subtitleInlineChkbox->setChecked(cfg.useInlineLayerSubtitles());
    subtitleInlineChkbox->setToolTip(i18nc("@item:tooltip", "If enabled, show subtitles beside layer names.\n"
                                                            "If disabled, show below layer names (when enough space)."));
    if (subtitleCombobox->currentIndex() == 0) {
        subtitleInlineChkbox->setDisabled(true);
    }

    QWidgetAction *chkboxAction = new QWidgetAction(this);
    chkboxAction->setDefaultWidget(subtitleInlineChkbox);
    configureMenu->addAction(chkboxAction);
    connect(subtitleInlineChkbox, SIGNAL(stateChanged(int)), SLOT(slotUpdateUseInlineLayerSubtitles()));

}

LayerBox::~LayerBox()
{
    delete m_wdgLayerBox;
}


void expandNodesRecursively(KisNodeSP root, QPointer<KisNodeFilterProxyModel> filteringModel, NodeView *nodeView)
{
    if (!root) return;
    if (filteringModel.isNull()) return;
    if (!nodeView) return;

    nodeView->blockSignals(true);

    KisNodeSP node = root->firstChild();
    while (node) {
        QModelIndex idx = filteringModel->indexFromNode(node);
        if (idx.isValid()) {
            nodeView->setExpanded(idx, !node->collapsed());
        }
        if (!node->collapsed() && node->childCount() > 0) {
            expandNodesRecursively(node, filteringModel, nodeView);
        }
        node = node->nextSibling();
    }
    nodeView->blockSignals(false);
}

void LayerBox::slotAddLayerBnClicked()
{
    if (m_canvas) {
        KisNodeList nodes = m_nodeManager->selectedNodes();

        if (nodes.size() == 1) {
            KisAction *action = m_canvas->viewManager()->actionManager()->actionByName("add_new_paint_layer");
            action->trigger();
        } else {
            KisAction *action = m_canvas->viewManager()->actionManager()->actionByName("create_quick_group");
            action->trigger();
        }
    }
}

void LayerBox::setViewManager(KisViewManager* kisview)
{
    m_nodeManager = kisview->nodeManager();

    if (m_nodeManager) {
        connect(m_nodeManager, SIGNAL(sigNodeActivated(KisNodeSP)), SLOT(slotForgetAboutSavedNodeBeforeEditSelectionMode()));
    }

    kisview->actionManager()->
            addAction(m_showGlobalSelectionMask->objectName(),
                      m_showGlobalSelectionMask);

    connect(m_wdgLayerBox->bnAdd, SIGNAL(clicked()), this, SLOT(slotAddLayerBnClicked()));

    connectActionToButton(kisview, m_wdgLayerBox->bnDuplicate, "duplicatelayer");

    KisActionManager *actionManager = kisview->actionManager();

    KisAction *action = actionManager->createAction("RenameCurrentLayer");
    Q_ASSERT(action);
    connect(action, SIGNAL(triggered()), this, SLOT(slotRenameCurrentNode()));

    m_propertiesAction = actionManager->createAction("layer_properties");
    Q_ASSERT(m_propertiesAction);
    new SyncButtonAndAction(m_propertiesAction, m_wdgLayerBox->bnProperties, this);
    connect(m_propertiesAction, SIGNAL(triggered()), this, SLOT(slotPropertiesClicked()));

    connect(m_opLayerMenu, SIGNAL(aboutToShow()), this, SLOT(slotLayerOpMenuOpened()));

    // It's necessary to clear the layer operations menu when it closes, else
    // the color selector can't be shared with the right-click context menu
    connect(m_opLayerMenu, SIGNAL(aboutToHide()), this, SLOT(slotLayerOpMenuClosed()));

    m_removeAction = actionManager->createAction("remove_layer");
    Q_ASSERT(m_removeAction);
    new SyncButtonAndAction(m_removeAction, m_wdgLayerBox->bnDelete, this);
    connect(m_removeAction, SIGNAL(triggered()), this, SLOT(slotRmClicked()));

    action = actionManager->createAction("move_layer_up");
    Q_ASSERT(action);
    new SyncButtonAndAction(action, m_wdgLayerBox->bnRaise, this);
    connect(action, SIGNAL(triggered()), this, SLOT(slotRaiseClicked()));

    action = actionManager->createAction("move_layer_down");
    Q_ASSERT(action);
    new SyncButtonAndAction(action, m_wdgLayerBox->bnLower, this);
    connect(action, SIGNAL(triggered()), this, SLOT(slotLowerClicked()));

    m_changeCloneSourceAction = actionManager->createAction("set-copy-from");
    Q_ASSERT(m_changeCloneSourceAction);
    connect(m_changeCloneSourceAction, &KisAction::triggered,
            this, &LayerBox::slotChangeCloneSourceClicked);

    m_layerToggleSolo = actionManager->createAction("toggle_layer_soloing");
    connect(m_layerToggleSolo, SIGNAL(triggered(bool)), this, SLOT(toggleActiveLayerSolo()));
}

void LayerBox::setCanvas(KoCanvasBase *canvas)
{
    if (m_canvas == canvas)
        return;

    setEnabled(canvas != 0);

    if (m_canvas) {
        m_canvas->disconnectCanvasObserver(this);
        m_nodeModel->setDummiesFacade(0, 0, 0, 0, 0);
        m_selectionActionsAdapter.reset();

        if (m_image) {
            KisImageAnimationInterface *animation = m_image->animationInterface();
            animation->disconnect(this);
        }

        disconnect(m_image, 0, this, 0);
        disconnect(m_nodeManager, 0, this, 0);
        disconnect(m_nodeModel, 0, m_nodeManager, 0);
        m_nodeManager->slotSetSelectedNodes(KisNodeList());
    }

    m_canvas = dynamic_cast<KisCanvas2*>(canvas);

    if (m_canvas) {
        m_image = m_canvas->image();
        emit imageChanged();
        connect(m_image, SIGNAL(sigImageUpdated(QRect)), &m_thumbnailCompressor, SLOT(start()));

        KisDocument* doc = static_cast<KisDocument*>(m_canvas->imageView()->document());
        KisShapeController *kritaShapeController =
                dynamic_cast<KisShapeController*>(doc->shapeController());
        KisDummiesFacadeBase *kritaDummiesFacade =
                static_cast<KisDummiesFacadeBase*>(kritaShapeController);


        m_selectionActionsAdapter.reset(new KisSelectionActionsAdapter(m_canvas->viewManager()->selectionManager()));
        m_nodeModel->setDummiesFacade(kritaDummiesFacade,
                                      m_image,
                                      kritaShapeController,
                                      m_selectionActionsAdapter.data(),
                                      m_nodeManager);

        connect(m_image, SIGNAL(sigAboutToBeDeleted()), SLOT(notifyImageDeleted()));
        connect(m_image, SIGNAL(sigNodeCollapsedChanged()), SLOT(slotNodeCollapsedChanged()));

        // cold start
        if (m_nodeManager) {
            setCurrentNode(m_nodeManager->activeNode());
            // Connection KisNodeManager -> LayerBox
            connect(m_nodeManager, SIGNAL(sigUiNeedChangeActiveNode(KisNodeSP)),
                    this, SLOT(setCurrentNode(KisNodeSP)));

            connect(m_nodeManager,
                    SIGNAL(sigUiNeedChangeSelectedNodes(QList<KisNodeSP>)),
                    SLOT(slotNodeManagerChangedSelection(QList<KisNodeSP>)));
        }
        else {
            setCurrentNode(m_canvas->imageView()->currentNode());
        }

        // Connection LayerBox -> KisNodeManager (isolate layer)
        connect(m_nodeModel, SIGNAL(toggleIsolateActiveNode()),
                m_nodeManager, SLOT(toggleIsolateActiveNode()));

        KisImageAnimationInterface *animation = m_image->animationInterface();
        connect(animation, &KisImageAnimationInterface::sigUiTimeChanged, this, &LayerBox::slotImageTimeChanged);

        expandNodesRecursively(m_image->rootLayer(), m_filteringModel, m_wdgLayerBox->listLayers);
        m_wdgLayerBox->listLayers->scrollTo(m_wdgLayerBox->listLayers->currentIndex());
        updateAvailableLabels();

        addActionToMenu(m_newLayerMenu, "add_new_paint_layer");
        addActionToMenu(m_newLayerMenu, "add_new_group_layer");
        addActionToMenu(m_newLayerMenu, "add_new_clone_layer");
        addActionToMenu(m_newLayerMenu, "add_new_shape_layer");
        addActionToMenu(m_newLayerMenu, "add_new_adjustment_layer");
        addActionToMenu(m_newLayerMenu, "add_new_fill_layer");
        addActionToMenu(m_newLayerMenu, "add_new_file_layer");
        m_newLayerMenu->addSeparator();
        addActionToMenu(m_newLayerMenu, "add_new_transparency_mask");
        addActionToMenu(m_newLayerMenu, "add_new_filter_mask");
        addActionToMenu(m_newLayerMenu, "add_new_colorize_mask");
        addActionToMenu(m_newLayerMenu, "add_new_transform_mask");
        addActionToMenu(m_newLayerMenu, "add_new_selection_mask");

    }

}


void LayerBox::unsetCanvas()
{
    setEnabled(false);
    if (m_canvas) {
        m_newLayerMenu->clear();
    }

    m_filteringModel->unsetDummiesFacade();
    disconnect(m_image, 0, this, 0);
    disconnect(m_nodeManager, 0, this, 0);
    disconnect(m_nodeModel, 0, m_nodeManager, 0);
    m_nodeManager->slotSetSelectedNodes(KisNodeList());

    m_canvas = 0;
}

void LayerBox::notifyImageDeleted()
{
    setCanvas(0);
}

void LayerBox::updateUI()
{
    if (!m_canvas) return;
    if (!m_nodeManager) return;

    KisNodeSP activeNode = m_nodeManager->activeNode();

    if (activeNode != m_activeNode) {
        m_activeNodeConnections.clear();
        m_activeNode = activeNode;

        if (activeNode) {
            KisPaintDeviceSP parentLayerDevice = activeNode->parent() ? activeNode->parent()->original() : 0;
            if (parentLayerDevice) {
                // update blending modes availability
                m_activeNodeConnections.addConnection(
                     parentLayerDevice, SIGNAL(colorSpaceChanged(const KoColorSpace*)),
                     this, SLOT(updateUI()));
            }

            m_activeNodeConnections.addConnection(
                    activeNode, SIGNAL(opacityChanged(quint8)),
                    this, SLOT(slotUpdateOpacitySlider(quint8)));
        }
    }

    m_wdgLayerBox->bnRaise->setEnabled(activeNode && activeNode->isEditable(false) && (activeNode->nextSibling()
                                                                                       || (activeNode->parent() && activeNode->parent() != m_image->root())));
    m_wdgLayerBox->bnLower->setEnabled(activeNode && activeNode->isEditable(false) && (activeNode->prevSibling()
                                                                                       || (activeNode->parent() && activeNode->parent() != m_image->root())));

    m_wdgLayerBox->doubleOpacity->setEnabled(activeNode && activeNode->isEditable(false));

    m_wdgLayerBox->cmbComposite->setEnabled(activeNode && activeNode->isEditable(false));

    if (activeNode) {
        if (activeNode->inherits("KisColorizeMask") || activeNode->inherits("KisLayer")) {

            m_wdgLayerBox->doubleOpacity->setEnabled(true);

            if (!m_wdgLayerBox->doubleOpacity->isDragging()) {
                slotSetOpacity(activeNode->opacity() * 100.0 / 255);
            }

            const KoCompositeOp* compositeOp = activeNode->compositeOp();
            if (compositeOp) {
                /// the composite op works in the color space of the parent layer,
                /// not the active one.
                m_wdgLayerBox->cmbComposite->validate(compositeOp->colorSpace());
                slotSetCompositeOp(compositeOp);
            } else {
                m_wdgLayerBox->cmbComposite->setEnabled(false);
            }

            const KisGroupLayer *group = qobject_cast<const KisGroupLayer*>(activeNode.data());
            bool compositeSelectionActive = !(group && group->passThroughMode());

            m_wdgLayerBox->cmbComposite->setEnabled(compositeSelectionActive);
        } else if (activeNode->inherits("KisMask")) {
            m_wdgLayerBox->cmbComposite->setEnabled(false);
            m_wdgLayerBox->doubleOpacity->setEnabled(false);
        }
    }
}


/**
 * This method is called *only* when non-GUI code requested the
 * change of the current node
 */
void LayerBox::setCurrentNode(KisNodeSP node)
{
    m_filteringModel->setActiveNode(node);

    QModelIndex index = node ? m_filteringModel->indexFromNode(node) : QModelIndex();
    m_filteringModel->setData(index, true, KisNodeModel::ActiveRole);
    updateUI();
}

void LayerBox::slotModelReset()
{
    if(m_nodeModel->hasDummiesFacade()) {
        QItemSelection selection;
        Q_FOREACH (const KisNodeSP node, m_nodeManager->selectedNodes()) {
            const QModelIndex &idx = m_filteringModel->indexFromNode(node);
            if(idx.isValid()){
                QItemSelectionRange selectionRange(idx);
                selection << selectionRange;
            }
        }

        m_wdgLayerBox->listLayers->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    }

    updateUI();
}

void LayerBox::slotSetCompositeOp(const KoCompositeOp* compositeOp)
{
    KoID opId = KoCompositeOpRegistry::instance().getKoID(compositeOp->id());

    m_wdgLayerBox->cmbComposite->blockSignals(true);
    m_wdgLayerBox->cmbComposite->selectCompositeOp(opId);
    m_wdgLayerBox->cmbComposite->blockSignals(false);
}

// range: 0-100
void LayerBox::slotSetOpacity(double opacity)
{
    Q_ASSERT(opacity >= 0 && opacity <= 100);
    m_wdgLayerBox->doubleOpacity->blockSignals(true);
    m_wdgLayerBox->doubleOpacity->setValue(opacity);
    m_wdgLayerBox->doubleOpacity->blockSignals(false);
}

void LayerBox::slotUpdateOpacitySlider(quint8 value) {
    double percentage = value * 100.0 / 255.0;

    m_wdgLayerBox->doubleOpacity->blockSignals(true);
    m_wdgLayerBox->doubleOpacity->setValue(percentage);
    m_wdgLayerBox->doubleOpacity->blockSignals(false);
}

void LayerBox::slotContextMenuRequested(const QPoint &pos, const QModelIndex &index)
{
    KisNodeList nodes = m_nodeManager->selectedNodes();
    KisNodeSP activeNode = m_nodeManager->activeNode();
    if (nodes.isEmpty() || !activeNode) return;

    if (m_canvas) {
        QMenu menu;
        updateLayerOpMenu(index, menu);
        menu.exec(pos);
    }
}

void LayerBox::slotMinimalView()
{
    m_wdgLayerBox->listLayers->setDisplayMode(NodeView::MinimalMode);
}

void LayerBox::slotDetailedView()
{
    m_wdgLayerBox->listLayers->setDisplayMode(NodeView::DetailedMode);
}

void LayerBox::slotThumbnailView()
{
    m_wdgLayerBox->listLayers->setDisplayMode(NodeView::ThumbnailMode);
}

void LayerBox::slotRmClicked()
{
    if (!m_canvas) return;
    m_nodeManager->removeNode();
}

void LayerBox::slotRaiseClicked()
{
    if (!m_canvas) return;
    m_nodeManager->raiseNode();
}

void LayerBox::slotLowerClicked()
{
    if (!m_canvas) return;
    m_nodeManager->lowerNode();
}

void LayerBox::slotPropertiesClicked()
{
    if (!m_canvas) return;
    if (KisNodeSP active = m_nodeManager->activeNode()) {
        m_nodeManager->nodeProperties(active);
    }
}

void LayerBox::slotLayerOpMenuOpened()
{
    if (!m_canvas) return;
    updateLayerOpMenu(m_wdgLayerBox->listLayers->currentIndex(), *m_opLayerMenu);
}

void LayerBox::slotLayerOpMenuClosed()
{
    if (!m_canvas) return;
    m_opLayerMenu->clear();
}

void LayerBox::slotChangeCloneSourceClicked()
{
    if (!m_canvas) return;
    m_nodeManager->changeCloneSource();
}

void LayerBox::slotCompositeOpChanged(int index)
{
    Q_UNUSED(index);
    if (!m_canvas) return;

    QString compositeOp = m_wdgLayerBox->cmbComposite->selectedCompositeOp().id();
    m_nodeManager->nodeCompositeOpChanged(m_nodeManager->activeColorSpace()->compositeOp(compositeOp));
}

void LayerBox::slotOpacityChanged()
{
    if (!m_canvas) return;
    m_blockOpacityUpdate = true;
    m_nodeManager->setNodeOpacity(m_changedOpacityNode, convertOpacityToInt(m_newOpacity));
    m_blockOpacityUpdate = false;
}

void LayerBox::slotOpacitySliderMoved(qreal opacity)
{
    m_newOpacity = opacity;
    m_changedOpacityNode = m_activeNode;
    m_opacityDelayTimer.start(200);
}

void LayerBox::slotCollapsed(const QModelIndex &index)
{
    KisNodeSP node = m_filteringModel->nodeFromIndex(index);
    if (node) {
        node->setCollapsed(true);
    }
}

void LayerBox::slotExpanded(const QModelIndex &index)
{
    KisNodeSP node = m_filteringModel->nodeFromIndex(index);
    if (node) {
        node->setCollapsed(false);
    }
}

void LayerBox::slotSelectOpaque()
{
    if (!m_canvas) return;
    QAction *action = m_canvas->viewManager()->actionManager()->actionByName("selectopaque");
    if (action) {
        action->trigger();
    }
}

void LayerBox::slotNodeCollapsedChanged()
{
    if (m_nodeModel->hasDummiesFacade()) {
        expandNodesRecursively(m_image->rootLayer(), m_filteringModel, m_wdgLayerBox->listLayers);
    }
}

inline bool isSelectionMask(KisNodeSP node)
{
    return dynamic_cast<KisSelectionMask*>(node.data());
}

KisNodeSP LayerBox::findNonHidableNode(KisNodeSP startNode)
{
    if (KisNodeManager::isNodeHidden(startNode, true) &&
            startNode->parent() &&
            !startNode->parent()->parent()) {


        KisNodeSP node = startNode->prevSibling();
        while (node && KisNodeManager::isNodeHidden(node, true)) {
            node = node->prevSibling();
        }

        if (!node) {
            node = startNode->nextSibling();
            while (node && KisNodeManager::isNodeHidden(node, true)) {
                node = node->nextSibling();
            }
        }

        if (!node) {
            node = m_image->root()->lastChild();
            while (node && KisNodeManager::isNodeHidden(node, true)) {
                node = node->prevSibling();
            }
        }

        KIS_ASSERT_RECOVER_NOOP(node && "cannot activate any node!");
        startNode = node;
    }

    return startNode;
}

void LayerBox::slotEditGlobalSelection(bool showSelections)
{
    KisNodeSP lastActiveNode = m_nodeManager->activeNode();
    KisNodeSP activateNode = lastActiveNode;
    KisSelectionMaskSP globalSelectionMask;

    if (!showSelections) {
        activateNode =
            m_savedNodeBeforeEditSelectionMode ?
                KisNodeSP(m_savedNodeBeforeEditSelectionMode) :
                findNonHidableNode(activateNode);
    }

    m_nodeModel->setShowGlobalSelection(showSelections);

    globalSelectionMask = m_image->rootLayer()->selectionMask();

    // try to find deactivated, but visible masks
    if (!globalSelectionMask) {
        KoProperties properties;
        properties.setProperty("visible", true);
        QList<KisNodeSP> masks = m_image->rootLayer()->childNodes(QStringList("KisSelectionMask"), properties);
        if (!masks.isEmpty()) {
            globalSelectionMask = dynamic_cast<KisSelectionMask*>(masks.first().data());
        }
    }

    // try to find at least any selection mask
    if (!globalSelectionMask) {
        KoProperties properties;
        QList<KisNodeSP> masks = m_image->rootLayer()->childNodes(QStringList("KisSelectionMask"), properties);
        if (!masks.isEmpty()) {
            globalSelectionMask = dynamic_cast<KisSelectionMask*>(masks.first().data());
        }
    }

    if (globalSelectionMask) {
        if (showSelections) {
            activateNode = globalSelectionMask;
        }
    }

    if (activateNode != lastActiveNode) {
        m_nodeManager->slotNonUiActivatedNode(activateNode);
    } else if (lastActiveNode) {
        setCurrentNode(lastActiveNode);
    }

    if (showSelections && !globalSelectionMask) {
        KisProcessingApplicator applicator(m_image, 0,
                                           KisProcessingApplicator::NONE,
                                           KisImageSignalVector(),
                                           kundo2_i18n("Quick Selection Mask"));

        applicator.applyCommand(
            new KisLayerUtils::KeepNodesSelectedCommand(
                m_nodeManager->selectedNodes(), KisNodeList(),
                lastActiveNode, 0, m_image, false),
            KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
        applicator.applyCommand(new KisSetEmptyGlobalSelectionCommand(m_image),
                                KisStrokeJobData::SEQUENTIAL,
                                KisStrokeJobData::EXCLUSIVE);
        applicator.applyCommand(new KisLayerUtils::SelectGlobalSelectionMask(m_image),
                                KisStrokeJobData::SEQUENTIAL,
                                KisStrokeJobData::EXCLUSIVE);

        applicator.end();
    } else if (!showSelections &&
               globalSelectionMask &&
               globalSelectionMask->selection()->selectedRect().isEmpty()) {

        KisProcessingApplicator applicator(m_image, 0,
                                           KisProcessingApplicator::NONE,
                                           KisImageSignalVector(),
                                           kundo2_i18n("Cancel Quick Selection Mask"));
        applicator.applyCommand(new KisSetGlobalSelectionCommand(m_image, 0), KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
        applicator.end();
    }

    if (showSelections) {
        m_savedNodeBeforeEditSelectionMode = lastActiveNode;
    }
}

void LayerBox::selectionChanged(const QModelIndexList &selection)
{
    if (!m_nodeManager) return;

    /**
     * When the user clears the extended selection by clicking on the
     * empty area of the docker, the selection should be reset on to
     * the active layer, which might be even unselected(!).
     */
    if (selection.isEmpty() && m_nodeManager->activeNode()) {
        QModelIndex selectedIndex =
                m_filteringModel->indexFromNode(m_nodeManager->activeNode());

        m_wdgLayerBox->listLayers->selectionModel()->
                setCurrentIndex(selectedIndex, QItemSelectionModel::ClearAndSelect);
        return;
    }

    QList<KisNodeSP> selectedNodes;
    Q_FOREACH (const QModelIndex &idx, selection) {
        // Precaution because node manager doesn't like duplicates in that list.
        // NodeView Selection behavior is SelectRows, although currently only column 0 allows selections.
        if (idx.column() != 0) {
            continue;
        }
        selectedNodes << m_filteringModel->nodeFromIndex(idx);
    }

    m_nodeManager->slotSetSelectedNodes(selectedNodes);
    updateUI();
}

void LayerBox::slotAdjustCurrentBeforeRemoveRows(const QModelIndex &parent, int start, int end)
{
    /**
     * Qt has changed its behavior when deleting an item. Previously
     * the selection priority was on the next item in the list, and
     * now it has shanged to the previous item. Here we just adjust
     * the selected item after the node removal.
     *
     * This method is called right before the Qt's beginRemoveRows()
     * is called, that is we make sure that Qt will never have to
     * adjust the position of the removed cursor.
     *
     * See bug: https://bugs.kde.org/show_bug.cgi?id=345601
     */

    QModelIndex currentIndex = m_wdgLayerBox->listLayers->currentIndex();
    QAbstractItemModel *model = m_filteringModel;

    if (currentIndex.isValid() && parent == currentIndex.parent()
            && currentIndex.row() >= start && currentIndex.row() <= end) {
        QModelIndex old = currentIndex;

        if (model && end < model->rowCount(parent) - 1) // there are rows left below the change
            currentIndex = model->index(end + 1, old.column(), parent);
        else if (model && start > 0) // there are rows left above the change
            currentIndex = model->index(start - 1, old.column(), parent);
        else // there are no rows left in the table
            currentIndex = QModelIndex();

        if (currentIndex.isValid() && currentIndex != old) {
            m_wdgLayerBox->listLayers->setCurrentIndex(currentIndex);
        }
    }
}

void LayerBox::slotNodeManagerChangedSelection(const KisNodeList &nodes)
{
    if (!m_nodeManager) return;

    QModelIndexList newSelection;
    Q_FOREACH(KisNodeSP node, nodes) {
        newSelection << m_filteringModel->indexFromNode(node);
    }

    QItemSelectionModel *model = m_wdgLayerBox->listLayers->selectionModel();

    if (KritaUtils::compareListsUnordered(newSelection, model->selectedRows())) {
        return;
    }

    QItemSelection selection;
    Q_FOREACH(const QModelIndex &idx, newSelection) {
        selection.select(idx, idx);
    }

    model->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void LayerBox::updateThumbnail()
{
    m_wdgLayerBox->listLayers->updateNode(m_wdgLayerBox->listLayers->currentIndex());
}

void LayerBox::slotRenameCurrentNode()
{
    m_wdgLayerBox->listLayers->edit(m_wdgLayerBox->listLayers->currentIndex());
}

void LayerBox::slotColorLabelChanged(int label)
{
    KisNodeList selectedNodes = m_nodeManager->selectedNodes();

    Q_FOREACH(KisNodeSP selectedNode, selectedNodes) {
        //Always apply label to selected nodes..
        selectedNode->setColorLabelIndex(label);

        //Apply label only to unlabelled children..
        KisNodeList children = selectedNode->childNodes(QStringList(), KoProperties());

        auto applyLabelFunc =
                [label](KisNodeSP child) {
            if (child->colorLabelIndex() == 0) {
                child->setColorLabelIndex(label);
            }
        };

        Q_FOREACH(KisNodeSP child, children) {
            KisLayerUtils::recursiveApplyNodes(child, applyLabelFunc);
        }
    }
}

void LayerBox::updateAvailableLabels()
{
    if (!m_image) return;
    layerFilterWidget->updateColorLabels(m_image->root());
}

void LayerBox::updateLayerFiltering()
{
    m_filteringModel->setAcceptedLabels(layerFilterWidget->getActiveColors());
    m_filteringModel->setTextFilter(layerFilterWidget->getTextFilter());
}

void LayerBox::slotImageTimeChanged(int time)
{
    Q_UNUSED(time);
    updateUI();
}

void LayerBox::updateLayerOpMenu(const QModelIndex &index, QMenu &menu) {

    KisNodeList nodes = m_nodeManager->selectedNodes();
    KisNodeSP activeNode = m_nodeManager->activeNode();
    const bool singleNode = nodes.size() == 1;

    if (index.isValid()) {
        menu.addAction(m_propertiesAction);

        KisLayerSP singleLayer = dynamic_cast<KisLayer*>(activeNode.data());

        if (singleLayer) {
            addActionToMenu(&menu, "layer_style");

            if (singleLayer->layerStyle()) {
                addActionToMenu(&menu, "copy_layer_style");
            }

            if (KisClipboard::instance()->hasLayerStyles()) {
                addActionToMenu(&menu, "paste_layer_style");
            }
        }

        Q_FOREACH(KisNodeSP node, nodes) {
            if (node && node->inherits("KisCloneLayer")) {
                menu.addAction(m_changeCloneSourceAction);
                break;
            }
        }

        {
            KisSignalsBlocker b(m_colorSelector->colorLabelSelector());
            m_colorSelector->colorLabelSelector()->setCurrentIndex(singleNode ? activeNode->colorLabelIndex() : -1);
        }

        menu.addAction(m_colorSelectorAction);

        menu.addSeparator();

        addActionToMenu(&menu, "cut_layer_clipboard");
        addActionToMenu(&menu, "copy_layer_clipboard");
        addActionToMenu(&menu, "paste_layer_from_clipboard");
        menu.addAction(m_removeAction);
        addActionToMenu(&menu, "duplicatelayer");
        addActionToMenu(&menu, "merge_layer");
        addActionToMenu(&menu, "new_from_visible");

        if (singleNode) {
            addActionToMenu(&menu, "flatten_image");
            addActionToMenu(&menu, "flatten_layer");
        }

        menu.addSeparator();
        QMenu *selectMenu = menu.addMenu(i18n("&Select"));
        addActionToMenu(selectMenu, "select_all_layers");
        addActionToMenu(selectMenu, "select_visible_layers");
        addActionToMenu(selectMenu, "select_invisible_layers");
        addActionToMenu(selectMenu, "select_locked_layers");
        addActionToMenu(selectMenu, "select_unlocked_layers");
        QMenu *groupMenu = menu.addMenu(i18nc("A group of layers", "&Group"));
        addActionToMenu(groupMenu, "create_quick_group");
        addActionToMenu(groupMenu, "create_quick_clipping_group");
        addActionToMenu(groupMenu, "quick_ungroup");
        QMenu *locksMenu = menu.addMenu(i18n("&Toggle Locks && Visibility"));
        addActionToMenu(locksMenu, "toggle_layer_visibility");
        addActionToMenu(locksMenu, "toggle_layer_lock");
        addActionToMenu(locksMenu, "toggle_layer_inherit_alpha");
        addActionToMenu(locksMenu, "toggle_layer_alpha_lock");

        if (singleNode) {
            QMenu *addLayerMenu = menu.addMenu(i18n("&Add"));
            addActionToMenu(addLayerMenu, "add_new_transparency_mask");
            addActionToMenu(addLayerMenu, "add_new_filter_mask");
            addActionToMenu(addLayerMenu, "add_new_colorize_mask");
            addActionToMenu(addLayerMenu, "add_new_transform_mask");
            addActionToMenu(addLayerMenu, "add_new_selection_mask");
            addLayerMenu->addSeparator();
            addActionToMenu(addLayerMenu, "add_new_clone_layer");

            QMenu *convertToMenu = menu.addMenu(i18n("&Convert"));
            addActionToMenu(convertToMenu, "convert_to_paint_layer");
            addActionToMenu(convertToMenu, "convert_to_transparency_mask");
            addActionToMenu(convertToMenu, "convert_to_filter_mask");
            addActionToMenu(convertToMenu, "convert_to_selection_mask");
            addActionToMenu(convertToMenu, "convert_to_file_layer");

            QMenu *splitAlphaMenu = menu.addMenu(i18n("S&plit Alpha"));
            addActionToMenu(splitAlphaMenu, "split_alpha_into_mask");
            addActionToMenu(splitAlphaMenu, "split_alpha_write");
            addActionToMenu(splitAlphaMenu, "split_alpha_save_merged");
        } else {
            QMenu *addLayerMenu = menu.addMenu(i18n("&Add"));
            addActionToMenu(addLayerMenu, "add_new_clone_layer");
        }

        menu.addSeparator();

        addActionToMenu(&menu, "pin_to_timeline");

        if (singleNode) {
            KisNodeSP node = m_filteringModel->nodeFromIndex(index);
            if (node && !node->inherits("KisTransformMask")) {
                addActionToMenu(&menu, "isolate_active_layer");
                addActionToMenu(&menu, "isolate_active_group");
            }

            addActionToMenu(&menu, "selectopaque");

        }
    }
}

void LayerBox::slotForgetAboutSavedNodeBeforeEditSelectionMode()
{
    m_savedNodeBeforeEditSelectionMode = 0;
}

void LayerBox::slotUpdateIcons() {
    m_wdgLayerBox->bnAdd->setIcon(KisIconUtils::loadIcon("addlayer"));
    m_wdgLayerBox->bnRaise->setIcon(KisIconUtils::loadIcon("arrowup"));
    m_wdgLayerBox->bnDelete->setIcon(KisIconUtils::loadIcon("deletelayer"));
    m_wdgLayerBox->bnLower->setIcon(KisIconUtils::loadIcon("arrowdown"));
    m_wdgLayerBox->bnProperties->setIcon(KisIconUtils::loadIcon("properties"));
    m_wdgLayerBox->bnDuplicate->setIcon(KisIconUtils::loadIcon("duplicatelayer"));
    m_wdgLayerBox->configureLayerDockerToolbar->setIcon(KisIconUtils::loadIcon("view-choose"));

    // call child function about needing to update icons
    m_wdgLayerBox->listLayers->slotUpdateIcons();
}

void LayerBox::toggleActiveLayerSolo() {
    NodeView* view = m_wdgLayerBox->listLayers;
    if (!view)
        return;

    KisNodeSP node = m_nodeManager->activeNode();
    if (!node)
        return;

    QModelIndex index = m_filteringModel->indexFromNode(node);
    if (!index.isValid())
        return;

    view->toggleSolo(index);
}

void LayerBox::slotUpdateThumbnailIconSize()
{
    KisConfig cfg(false);
    cfg.setLayerThumbnailSize(thumbnailSizeSlider->value());

    m_wdgLayerBox->listLayers->slotConfigurationChanged();
}

void LayerBox::slotUpdateTreeIndentation()
{
    KisConfig cfg(false);
    if (indentationSlider->value() == cfg.layerTreeIndentation()) {
        return;
    }
    cfg.setLayerTreeIndentation(indentationSlider->value());
    m_wdgLayerBox->listLayers->slotConfigurationChanged();
}

void LayerBox::slotUpdateLayerSubtitleStyle()
{
    KisConfig cfg(false);
    if (subtitleCombobox->currentIndex() == cfg.layerSubtitleStyle()) {
        return;
    }
    cfg.setLayerSubtitleStyle((KisConfig::LayerSubtitleStyle)subtitleCombobox->currentIndex());
    m_wdgLayerBox->listLayers->slotConfigurationChanged();
    m_wdgLayerBox->listLayers->viewport()->update();
    if (subtitleCombobox->currentIndex() == 0) {
        subtitleOpacitySlider->setDisabled(true);
        subtitleInlineChkbox->setDisabled(true);
    }
    else {
        subtitleOpacitySlider->setDisabled(false);
        subtitleInlineChkbox->setDisabled(false);
    }
}

void LayerBox::slotUpdateLayerSubtitleOpacity()
{
    KisConfig cfg(false);
    if (subtitleOpacitySlider->value() == cfg.layerSubtitleOpacity()) {
        return;
    }
    cfg.setLayerSubtitleOpacity(subtitleOpacitySlider->value());
    m_wdgLayerBox->listLayers->slotConfigurationChanged();
    m_wdgLayerBox->listLayers->viewport()->update();
}

void LayerBox::slotUpdateUseInlineLayerSubtitles()
{
    KisConfig cfg(false);
    if (subtitleInlineChkbox->isChecked() == cfg.useInlineLayerSubtitles()) {
        return;
    }
    cfg.setUseInlineLayerSubtitles(subtitleInlineChkbox->isChecked());
    m_wdgLayerBox->listLayers->slotConfigurationChanged();
    m_wdgLayerBox->listLayers->viewport()->update();
}


#include "moc_LayerBox.cpp"
