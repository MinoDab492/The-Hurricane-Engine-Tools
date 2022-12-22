/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISTOOLENCLOSEANDFILL_H
#define KISTOOLENCLOSEANDFILL_H

#include <QPoint>
#include <QList>

#include <kis_icon.h>
#include <kis_tool_shape.h>
#include <flake/kis_node_shape.h>
#include <KoIcon.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <KisEncloseAndFillPainter.h>
#include <commands_new/KisMergeLabeledLayersCommand.h>

#include "subtools/KisDynamicDelegatedTool.h"

class KisOptionCollectionWidget;
class KisColorButton;
class KoGroupButton;
class KisDoubleSliderSpinBox;
class KisAngleSelector;
class KisSliderSpinBox;
class QCheckBox;
class KisColorLabelSelectorWidget;
class QPushButton;
class QComboBox;

class KisToolEncloseAndFill : public KisDynamicDelegatedTool<KisToolShape>
{
    Q_OBJECT

public:
    enum EnclosingMethod
    {
        Rectangle,
        Ellipse,
        Path,
        Lasso,
        Brush
    };

    enum FillType
    {
        FillWithForegroundColor,
        FillWithBackgroundColor,
        FillWithPattern
    };

    enum Reference
    {
        CurrentLayer,
        AllLayers,
        ColorLabeledLayers
    };

    using RegionSelectionMethod = KisEncloseAndFillPainter::RegionSelectionMethod;

    KisToolEncloseAndFill(KoCanvasBase * canvas);
    ~KisToolEncloseAndFill() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void activateAlternateAction(AlternateAction action) override;
    void deactivateAlternateAction(AlternateAction action) override;
    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void continueAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void endAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void beginAlternateDoubleClickAction(KoPointerEvent *event, AlternateAction action) override;

    int flags() const override;
    QWidget* createOptionWidget() override;

public Q_SLOTS:
    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;

private:
    EnclosingMethod m_enclosingMethod {Lasso};

    RegionSelectionMethod m_regionSelectionMethod {defaultRegionSelectionMethod()};
    KoColor m_regionSelectionColor;
    bool m_regionSelectionInvert {false};
    bool m_regionSelectionIncludeContourRegions {false};

    FillType m_fillType {FillWithForegroundColor};
    qreal m_patternScale {100.0};
    qreal m_patternRotation {0.0};

    int m_fillThreshold {8};
    int m_fillOpacitySpread {100};
    bool m_useSelectionAsBoundary {true};

    bool m_antiAlias {false};
    int m_expand {0};
    int m_feather {0};

    Reference m_reference {CurrentLayer};
    QList<int> m_selectedColorLabels;

    KisPaintDeviceSP m_referencePaintDevice;
    KisMergeLabeledLayersCommand::ReferenceNodeInfoListSP m_referenceNodeList;

    KisOptionCollectionWidget *m_optionWidget {nullptr};

    KoGroupButton *m_buttonEnclosingMethodRectangle{nullptr};
    KoGroupButton *m_buttonEnclosingMethodEllipse{nullptr};
    KoGroupButton *m_buttonEnclosingMethodPath{nullptr};
    KoGroupButton *m_buttonEnclosingMethodLasso{nullptr};
    KoGroupButton *m_buttonEnclosingMethodBrush{nullptr};

    QComboBox *m_comboBoxRegionSelectionMethod {nullptr};
    KisColorButton *m_buttonRegionSelectionColor {nullptr};
    QCheckBox *m_checkBoxRegionSelectionInvert {nullptr};
    QCheckBox *m_checkBoxRegionSelectionIncludeContourRegions {nullptr};

    KoGroupButton *m_buttonFillWithFG{nullptr};
    KoGroupButton *m_buttonFillWithBG{nullptr};
    KoGroupButton *m_buttonFillWithPattern{nullptr};
    KisDoubleSliderSpinBox *m_sliderPatternScale {nullptr};
    KisAngleSelector *m_angleSelectorPatternRotation {nullptr};

    KisSliderSpinBox *m_sliderFillThreshold {nullptr};
    KisSliderSpinBox *m_sliderFillOpacitySpread {nullptr};
    QCheckBox *m_checkBoxSelectionAsBoundary {nullptr};

    QCheckBox *m_checkBoxAntiAlias {nullptr};
    KisSliderSpinBox *m_sliderExpand {nullptr};
    KisSliderSpinBox *m_sliderFeather {nullptr};

    KoGroupButton *m_buttonReferenceCurrent{nullptr};
    KoGroupButton *m_buttonReferenceAll{nullptr};
    KoGroupButton *m_buttonReferenceLabeled{nullptr};
    KisColorLabelSelectorWidget *m_widgetLabels {nullptr};

    KConfigGroup m_configGroup;
    
    bool m_alternateActionStarted {false};

    bool wantsAutoScroll() const override { return false; }

    EnclosingMethod loadEnclosingMethodFromConfig() const;
    void saveEnclosingMethodToConfig(EnclosingMethod enclosingMethod);
    QString enclosingMethodToConfigString(EnclosingMethod enclosingMethod) const;
    EnclosingMethod configStringToEnclosingMethod(const QString &configString) const;
    static constexpr EnclosingMethod defaultEnclosingMethod() { return Lasso; }

    QString regionSelectionMethodToUserString(RegionSelectionMethod regionSelectionMethod) const;
    RegionSelectionMethod loadRegionSelectionMethodFromConfig() const;
    void saveRegionSelectionMethodToConfig(RegionSelectionMethod regionSelectionMethod);
    QString regionSelectionMethodToConfigString(RegionSelectionMethod regionSelectionMethod) const;
    RegionSelectionMethod configStringToRegionSelectionMethod(const QString &configString) const;
    static constexpr RegionSelectionMethod defaultRegionSelectionMethod() { return RegionSelectionMethod::SelectAllRegions; }

    KoColor loadRegionSelectionColorFromConfig();

    QString referenceToUserString(Reference reference) const;
    Reference loadReferenceFromConfig() const;
    void saveReferenceToConfig(Reference reference);
    QString referenceToConfigString(Reference reference) const;
    Reference configStringToReference(const QString &configString) const;
    static constexpr Reference defaultReference() { return CurrentLayer; }

    void setupEnclosingSubtool();
    bool subtoolHasUserInteractionRunning() const;

    void loadConfiguration();

private Q_SLOTS:
    void
    slot_optionButtonStripEnclosingMethod_buttonToggled(KoGroupButton *button,
                                                        bool checked);
    void slot_comboBoxRegionSelectionMethod_currentIndexChanged(int);
    void slot_buttonRegionSelectionColor_changed(const KoColor &color);
    void slot_checkBoxRegionSelectionInvert_toggled(bool checked);
    void slot_checkBoxRegionSelectionIncludeContourRegions_toggled(bool checked);
    void slot_optionButtonStripFillWith_buttonToggled(KoGroupButton *button,
                                                      bool checked);
    void slot_sliderPatternScale_valueChanged(double value);
    void slot_angleSelectorPatternRotation_angleChanged(double value);
    void slot_sliderFillThreshold_valueChanged(int value);
    void slot_sliderFillOpacitySpread_valueChanged(int value);
    void slot_checkBoxSelectionAsBoundary_toggled(bool checked);
    void slot_checkBoxAntiAlias_toggled(bool checked);
    void slot_sliderExpand_valueChanged(int value);
    void slot_sliderFeather_valueChanged(int value);
    void slot_optionButtonStripReference_buttonToggled(KoGroupButton *button,
                                                       bool checked);
    void slot_widgetLabels_selectionChanged();
    void slot_buttonReset_clicked();

    void slot_delegateTool_enclosingMaskProduced(KisPixelSelectionSP enclosingMask);

    void resetCursorStyle() override;
};

#endif
