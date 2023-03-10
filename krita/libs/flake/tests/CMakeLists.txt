include(ECMAddTests)
include(KritaAddBrokenUnitTest)

include_directories(${CMAKE_SOURCE_DIR}/sdk/tests)

macro_add_unittest_definitions()

if (APPLE)
    kis_add_tests(
        TestPosition.cpp
        TestSelection.cpp
        TestKoShapeFactory.cpp
        TestShapeContainer.cpp
        TestShapeGroupCommand.cpp
        TestShapeBackgroundCommand.cpp
        TestShapeStrokeCommand.cpp
        TestShapeShadowCommand.cpp
        TestInputDevice.cpp
        TestPathShape.cpp
        TestControlPointMoveCommand.cpp
        TestPointTypeCommand.cpp
        TestRemoveSubpathCommand.cpp
        TestPathSegment.cpp
        TestSegmentTypeCommand.cpp

        LINK_LIBRARIES kritaflake Qt5::Test
        NAME_PREFIX "libs-flake-"
        TARGET_NAMES_VAR OK_TESTS
        GUI)

    krita_add_broken_unit_test(
        TestSvgParser.cpp
        TEST_NAME TestSvgParser
        LINK_LIBRARIES kritaflake Qt5::Test
        NAME_PREFIX "libs-flake-"
        GUI)

    krita_add_broken_unit_test(
        TestSvgParser.cpp
        TEST_NAME TestSvgParserCloned
        LINK_LIBRARIES kritaflake Qt5::Test
        NAME_PREFIX "libs-flake-"
        GUI)
    set_property(TARGET TestSvgParserCloned
                 PROPERTY COMPILE_DEFINITIONS USE_CLONED_SHAPES)

    krita_add_broken_unit_test(
        TestSvgParser.cpp
        TEST_NAME TestSvgParserRoundTrip
        LINK_LIBRARIES kritaflake Qt5::Test
        NAME_PREFIX "libs-flake-"
        GUI)
    set_property(TARGET TestSvgParserRoundTrip
                 PROPERTY COMPILE_DEFINITIONS USE_ROUND_TRIP)

    # Broken unit test.app can be run if installed
     ############## broken tests ###############
    krita_add_broken_unit_tests(
        TestPathTool.cpp
        TestShapeAt.cpp
        TestShapePainting.cpp
        TestShapeReorderCommand.cpp
        TestResourceManager.cpp
        TestSnapStrategy.cpp
        TestPointMergeCommand.cpp
        TestPointRemoveCommand.cpp
        TestKoDrag.cpp
        TestKoMarkerCollection.cpp

        LINK_LIBRARIES kritaflake Qt5::Test
        NAME_PREFIX "libs-flake-"
        TARGET_NAMES_VAR BROKEN_TESTS
        GUI)

    krita_add_broken_unit_test( TestSvgText.cpp
        TEST_NAME TestSvgText
        LINK_LIBRARIES kritaflake Qt5::Test
        NAME_PREFIX "libs-flake-"
        GUI)

    krita_add_broken_unit_test( TestSvgText.cpp
        TEST_NAME TestSvgTextCloned
        LINK_LIBRARIES kritaflake Qt5::Test
        NAME_PREFIX "libs-flake-"
        GUI)
    set_property(TARGET TestSvgTextCloned
                 PROPERTY COMPILE_DEFINITIONS USE_CLONED_SHAPES)

    krita_add_broken_unit_test( TestSvgText.cpp
        TEST_NAME TestSvgTextRoundTrip
        LINK_LIBRARIES kritaflake Qt5::Test
        NAME_PREFIX "libs-flake-")
    set_property(TARGET TestSvgTextRoundTrip
                 PROPERTY COMPILE_DEFINITIONS USE_ROUND_TRIP)


    ############## FIX RPATH ###############
    macos_test_fixrpath(
        ${OK_TESTS}
        ${BROKEN_TESTS}
        TestSvgParser
        TestSvgParserCloned
        TestSvgParserRoundTrip

        TestSvgText
        TestSvgTextCloned
        TestSvgTextRoundTrip
        )


else(APPLE)

kis_add_tests(
    TestPosition.cpp
    TestSelection.cpp
    TestPathTool.cpp
    TestShapeAt.cpp
    TestShapePainting.cpp
    TestKoShapeFactory.cpp
    TestShapeContainer.cpp
    TestShapeGroupCommand.cpp
    TestShapeReorderCommand.cpp
    TestResourceManager.cpp
    TestShapeBackgroundCommand.cpp
    TestShapeStrokeCommand.cpp
    TestShapeShadowCommand.cpp
    TestInputDevice.cpp
    TestSnapStrategy.cpp
    TestPathShape.cpp
    TestControlPointMoveCommand.cpp
    TestPointTypeCommand.cpp
    TestPointRemoveCommand.cpp
    TestRemoveSubpathCommand.cpp
    TestPathSegment.cpp
    TestSegmentTypeCommand.cpp
    TestKoDrag.cpp
    TestKoMarkerCollection.cpp

    LINK_LIBRARIES kritaflake Qt5::Test
    NAME_PREFIX "libs-flake-")

kis_add_test(
    TestSvgParser.cpp
    TEST_NAME TestSvgParser
    LINK_LIBRARIES kritaflake Qt5::Test
    NAME_PREFIX "libs-flake-")

kis_add_test(
    TestSvgParser.cpp
    TEST_NAME TestSvgParserCloned
    LINK_LIBRARIES kritaflake Qt5::Test
    NAME_PREFIX "libs-flake-")
set_property(TARGET TestSvgParserCloned
             PROPERTY COMPILE_DEFINITIONS USE_CLONED_SHAPES)

kis_add_test(
    TestSvgParser.cpp
    TEST_NAME TestSvgParserRoundTrip
    LINK_LIBRARIES kritaflake Qt5::Test
    NAME_PREFIX "libs-flake-")
set_property(TARGET TestSvgParserRoundTrip
             PROPERTY COMPILE_DEFINITIONS USE_ROUND_TRIP)

############## broken tests ###############

krita_add_broken_unit_test(TestPointMergeCommand.cpp
    TEST_NAME TestPointMergeCommand
    LINK_LIBRARIES kritaflake Qt5::Test
    NAME_PREFIX "libs-flake-")

krita_add_broken_unit_test( TestSvgText.cpp
    TEST_NAME TestSvgText
    LINK_LIBRARIES kritaflake Qt5::Test
    NAME_PREFIX "libs-flake-")

krita_add_broken_unit_test( TestSvgText.cpp
    TEST_NAME TestSvgTextCloned
    LINK_LIBRARIES kritaflake Qt5::Test
    NAME_PREFIX "libs-flake-")
set_property(TARGET TestSvgTextCloned
             PROPERTY COMPILE_DEFINITIONS USE_CLONED_SHAPES)

krita_add_broken_unit_test( TestSvgText.cpp
    TEST_NAME TestSvgTextRoundTrip
    LINK_LIBRARIES kritaflake Qt5::Test
    NAME_PREFIX "libs-flake-")
set_property(TARGET TestSvgTextRoundTrip
             PROPERTY COMPILE_DEFINITIONS USE_ROUND_TRIP)

endif()
