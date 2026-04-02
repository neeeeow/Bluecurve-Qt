# Set the source files
add_library(bluecurve-style-qt${QT_MAJOR_VERSION} MODULE
  bluecurvestyle.cpp
  bluecurvestyle.h
  bluecurvestyleplugin.cpp
  bluecurvestyleplugin.h
)

set_target_properties(bluecurve-style-qt${QT_MAJOR_VERSION} PROPERTIES
  OUTPUT_NAME bluecurve
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt${QT_MAJOR_VERSION}"
)

target_link_libraries(bluecurve-style-qt${QT_MAJOR_VERSION}
  Qt${QT_MAJOR_VERSION}::Widgets
)

install(TARGETS bluecurve-style-qt${QT_MAJOR_VERSION} DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/styles/)
