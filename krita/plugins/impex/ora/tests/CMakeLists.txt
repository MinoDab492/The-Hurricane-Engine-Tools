include_directories(     ${CMAKE_SOURCE_DIR}/sdk/tests )

include(KritaAddBrokenUnitTest)
macro_add_unittest_definitions()

if (APPLE)

    krita_add_broken_unit_tests(
        KisOraTest.cpp
        LINK_LIBRARIES kritaui Qt5::Test
        NAME_PREFIX "plugins-impex-"
        TARGET_NAMES_VAR BROKEN_TESTS
        ${MACOS_GUI_TEST}
    )

    macos_test_fixrpath(${BROKEN_TESTS})

else (APPLE)
    kis_add_test(
        KisOraTest.cpp
        TEST_NAME KisOraTest
        LINK_LIBRARIES kritaui Qt5::Test
        NAME_PREFIX "plugins-impex-"
        )

endif()
