/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISTEST
#define KISTEST

#include <KoTestConfig.h>
#include <QApplication>
#include <simpletest.h>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QtTest/qtestsystem.h>
#include <set>
#include <QLocale>

/**
 * There is a hierarchy of libraries built on the kritaresources library
 * that provide resources:
 *
 * pigment: kocolorset, kosegmentgradient, kostopgradient, kopattern
 *   flake: koseexprscript, kogamutmask, kosvgsymbolcollection
 *     image: kispaintoppreset, kispsdlayerstyle
 *       brush: kisgbrbrush, kisimagepipebrush, kissvgbrush, kispngbrush
 *         ui: kiswindowloyout, kissession, kisworkspace
 *
 * Depending on which library the test links again, it should use
 *
 *  testresources.h
 *  testpigment.h
 *  testflake.h
 *  testimage.h
 *  testbrush.h
 *  testui.h
 *
 * To get the right KISTEST_MAIN for the resources it needs access to.
 *
 * This means that adding a new resource means not only adding it in
 * KisApplication, but also this file.
 */



#if defined(QT_NETWORK_LIB)
#  include <QtTest/qtest_network.h>
#endif
#include <QtTest/qtest_widgets.h>

#ifdef QT_KEYPAD_NAVIGATION
#  define QTEST_DISABLE_KEYPAD_NAVIGATION QApplication::setNavigationMode(Qt::NavigationModeNone);
#else
#  define QTEST_DISABLE_KEYPAD_NAVIGATION
#endif

#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTBRUSH) || defined(TESTIMAGE) || defined(TESTUI)
#include <QImageReader>
#include <QList>
#include <QByteArray>
#include <QStringList>
#include <QStandardPaths>
#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QImageWriter>

#include <KisResourceTypes.h>
#include <KisResourceLoaderRegistry.h>
#include <KisMimeDatabase.h>
#include <KisResourceLoader.h>
#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KoResourcePaths.h>



#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
#include <resources/KoSegmentGradient.h>
#include <resources/KoStopGradient.h>
#include <resources/KoColorSet.h>
#include <resources/KoPattern.h>
#endif

#if defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
#if defined HAVE_SEEXPR
#include <KisSeExprScript.h>
#endif
#include <resources/KoGamutMask.h>
#include <resources/KoSvgSymbolCollectionResource.h>
#endif

#if defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
#include <kis_paintop_preset.h>
#include <kis_psd_layer_style.h>
#endif

#if defined(TESTBRUSH) || defined(TESTUI)
#include <kis_gbr_brush.h>
#include <kis_imagepipe_brush.h>
#include <kis_svg_brush.h>
#include <kis_png_brush.h>
#endif

#if defined(TESTUI)
#include <KisWindowLayoutResource.h>
#include <kis_workspace_resource.h>
#include <KisSessionResource.h>
#endif

namespace {

void addResourceTypes()
{
#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
    // All Krita's resource types
    KoResourcePaths::addAssetType("markers", "data", "/styles/");
    KoResourcePaths::addAssetType("kis_pics", "data", "/pics/");
    KoResourcePaths::addAssetType("kis_images", "data", "/images/");
    KoResourcePaths::addAssetType("metadata_schema", "data", "/metadata/schemas/");
    KoResourcePaths::addAssetType("gmic_definitions", "data", "/gmic/");
    KoResourcePaths::addAssetType("kis_defaultpresets", "data", "/defaultpresets/");
    KoResourcePaths::addAssetType("psd_layer_style_collections", "data", "/asl");
    KoResourcePaths::addAssetType("kis_shortcuts", "data", "/shortcuts/");
    KoResourcePaths::addAssetType("kis_actions", "data", "/actions");
    KoResourcePaths::addAssetType("kis_actions", "data", "/pykrita");
    KoResourcePaths::addAssetType("icc_profiles", "data", "/color/icc");
    KoResourcePaths::addAssetType("icc_profiles", "data", "/profiles/");
    KoResourcePaths::addAssetType("tags", "data", "/tags/");
    KoResourcePaths::addAssetType("templates", "data", "/templates");
    KoResourcePaths::addAssetType("pythonscripts", "data", "/pykrita");
    KoResourcePaths::addAssetType("preset_icons", "data", "/preset_icons");

    // Make directories for all resources we can save, and tags
    QDir d;
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tags/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/asl/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/bundles/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/brushes/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/gradients/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/paintoppresets/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/palettes/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/patterns/");
    // between 4.2.x and 4.3.0 there was a change from 'taskset' to 'tasksets'
    // so to make older resource folders compatible with the new version, let's rename the folder
    // so no tasksets are lost.
    if (d.exists(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/taskset/")) {
        d.rename(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/taskset/",
                 QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tasksets/");
    }
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tasksets/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/workspaces/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/input/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/pykrita/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/symbols/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/color-schemes/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/preset_icons/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/preset_icons/tool_icons/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/preset_icons/emblem_icons/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/gamutmasks/");
#if defined HAVE_SEEXPR
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/seexpr_scripts/");
#endif
#endif

}

void registerResources()
{

#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)

    addResourceTypes();

    KisResourceLoaderRegistry *reg = KisResourceLoaderRegistry::instance();

    QList<QByteArray> src = QImageReader::supportedMimeTypes();
    QStringList allImageMimes;
    Q_FOREACH(const QByteArray ba, src) {
        if (QImageWriter::supportedMimeTypes().contains(ba)) {
            allImageMimes << QString::fromUtf8(ba);
        }
    }
    allImageMimes << KisMimeDatabase::mimeTypeForSuffix("pat");

    reg->add(new KisResourceLoader<KoPattern>(ResourceType::Patterns, ResourceType::Patterns, i18n("Patterns"), allImageMimes));
    reg->add(new KisResourceLoader<KoSegmentGradient>(ResourceSubType::SegmentedGradients, ResourceType::Gradients, i18n("Gradients"), QStringList() << "application/x-gimp-gradient"));
    reg->add(new KisResourceLoader<KoStopGradient>(ResourceSubType::StopGradients, ResourceType::Gradients, i18n("Gradients"), QStringList() << "image/svg+xml"));

    reg->add(new KisResourceLoader<KoColorSet>(ResourceType::Palettes, ResourceType::Palettes, i18n("Palettes"),
                                     QStringList() << KisMimeDatabase::mimeTypeForSuffix("kpl")
                                               << KisMimeDatabase::mimeTypeForSuffix("gpl")
                                               << KisMimeDatabase::mimeTypeForSuffix("pal")
                                               << KisMimeDatabase::mimeTypeForSuffix("act")
                                               << KisMimeDatabase::mimeTypeForSuffix("aco")
                                               << KisMimeDatabase::mimeTypeForSuffix("css")
                                               << KisMimeDatabase::mimeTypeForSuffix("colors")
                                               << KisMimeDatabase::mimeTypeForSuffix("xml")
                                               << KisMimeDatabase::mimeTypeForSuffix("sbz")));
#endif

#if defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
#if defined HAVE_SEEXPR
    reg->add(new KisResourceLoader<KisSeExprScript>(ResourceType::SeExprScripts, ResourceType::SeExprScripts, i18n("SeExpr Scripts"), QStringList() << "application/x-krita-seexpr-script"));
#endif
    reg->add(new KisResourceLoader<KoGamutMask>(ResourceType::GamutMasks, ResourceType::GamutMasks, i18n("Gamut masks"), QStringList() << "application/x-krita-gamutmasks"));
    reg->add(new KisResourceLoader<KoSvgSymbolCollectionResource>(ResourceType::Symbols, ResourceType::Symbols, i18n("SVG symbol libraries"), QStringList() << "image/svg+xml"));
#endif


#if defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
     reg->add(new KisResourceLoader<KisPaintOpPreset>(ResourceType::PaintOpPresets, ResourceType::PaintOpPresets, i18n("Brush presets"), QStringList() << "application/x-krita-paintoppreset"));
     reg->add(new KisResourceLoader<KisPSDLayerStyle>(ResourceType::LayerStyles,
                                                     ResourceType::LayerStyles,
                                                     ResourceType::LayerStyles,
                                                     QStringList() << "application/x-photoshop-style"));
#endif

#if defined(TESTBRUSH) || defined(TESTUI)

    reg->add(new KisResourceLoader<KisGbrBrush>(ResourceSubType::GbrBrushes, ResourceType::Brushes, i18n("Brush tips"), QStringList() << "image/x-gimp-brush"));
    reg->add(new KisResourceLoader<KisImagePipeBrush>(ResourceSubType::GihBrushes, ResourceType::Brushes, i18n("Brush tips"), QStringList() << "image/x-gimp-brush-animated"));
    reg->add(new KisResourceLoader<KisSvgBrush>(ResourceSubType::SvgBrushes, ResourceType::Brushes, i18n("Brush tips"), QStringList() << "image/svg+xml"));
    reg->add(new KisResourceLoader<KisPngBrush>(ResourceSubType::PngBrushes, ResourceType::Brushes, i18n("Brush tips"), QStringList() << "image/png"));

#endif

#if defined(TESTUI)
    reg->add(new KisResourceLoader<KisWindowLayoutResource>(ResourceType::WindowLayouts, ResourceType::WindowLayouts, i18n("Window layouts"), QStringList() << "application/x-krita-windowlayout"));
    reg->add(new KisResourceLoader<KisSessionResource>(ResourceType::Sessions, ResourceType::Sessions, i18n("Sessions"), QStringList() << "application/x-krita-session"));
    reg->add(new KisResourceLoader<KisWorkspaceResource>(ResourceType::Workspaces, ResourceType::Workspaces, i18n("Workspaces"), QStringList() << "application/x-krita-workspace"));
#endif

#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTBRUSH) || defined(TESTIMAGE) || defined(TESTUI)
    if (!KisResourceCacheDb::initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))) {
        qFatal("Could not initialize the resource cachedb");
    }

    KisResourceLocator::instance()->initialize(KoResourcePaths::getApplicationRoot() + "/share/krita");
#endif

}

#define KISTEST_MAIN(TestObject) \
int main(int argc, char *argv[]) \
{ \
    qputenv("LANGUAGE", "en"); \
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); \
    qputenv("QT_LOGGING_RULES", ""); \
    QStandardPaths::setTestModeEnabled(true); \
    qputenv("EXTRA_RESOURCE_DIRS", QByteArray(KRITA_RESOURCE_DIRS_FOR_TESTS)); \
    qputenv("KRITA_PLUGIN_PATH", QByteArray(KRITA_PLUGINS_DIR_FOR_TESTS)); \
    QApplication app(argc, argv); \
    app.setAttribute(Qt::AA_Use96Dpi, true); \
    QTEST_DISABLE_KEYPAD_NAVIGATION \
    registerResources(); \
    TestObject tc; \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}

}
#else
#define KISTEST_MAIN(TestObject) \
int main(int argc, char *argv[]) \
{ \
    qputenv("LANGUAGE", "en"); \
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); \
    qputenv("QT_LOGGING_RULES", ""); \
    qputenv("EXTRA_RESOURCE_DIRS", QByteArray(KRITA_RESOURCE_DIRS_FOR_TESTS)); \
    qputenv("KRITA_PLUGIN_PATH", QByteArray(KRITA_PLUGINS_DIR_FOR_TESTS)); \
    QStandardPaths::setTestModeEnabled(true); \
    QApplication app(argc, argv); \
    app.setAttribute(Qt::AA_Use96Dpi, true); \
    QTEST_DISABLE_KEYPAD_NAVIGATION \
    TestObject tc; \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}
#endif


#endif
