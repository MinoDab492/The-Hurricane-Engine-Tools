include_directories(${CMAKE_SOURCE_DIR}/libs/image/metadata
                    ${CMAKE_SOURCE_DIR}/sdk/tests )
include(ECMAddTests)

macro_add_unittest_definitions()

kis_add_test(
    TestNodeView.cpp
    LINK_LIBRARIES kritaui Qt5::Test
    NAME_PREFIX "plugins-dockers-layerdocker-")

