set(KRITA_COMPOSITIONDOCKER_SOURCES compositionmodel.cpp compositiondocker.cpp compositiondocker_dock.cpp )

ki18n_wrap_ui(KRITA_COMPOSITIONDOCKER_SOURCES
    wdgcompositiondocker.ui
)

kis_add_library(kritacompositiondocker MODULE ${KRITA_COMPOSITIONDOCKER_SOURCES})
target_link_libraries(kritacompositiondocker kritaui)
install(TARGETS kritacompositiondocker DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
