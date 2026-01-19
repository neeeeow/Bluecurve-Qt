cmake_minimum_required(VERSION 3.16)

project(bluecurve-style LANGUAGES CXX)

set(KF5_MIN_VERSION "5.102.0")
set(KF6_MIN_VERSION "6.18.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")
set(QT5_MIN_VERSION "5.15.2")
set(QT_MIN_VERSION "6.9.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Needed for Qt installation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# ECM setup
find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

include(KDEInstallDirs6)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(bluecurve-style MODULE
  src/bits.cpp
  src/bluecurvestyle.cpp
  src/bluecurvestyle.h
  src/bluecurvestyleplugin.cpp
  src/bluecurvestyleplugin.h
)

set_target_properties(bluecurve-style PROPERTIES
  OUTPUT_NAME bluecurve
)

target_link_libraries(bluecurve-style
  Qt6::Widgets
)

install(TARGETS bluecurve-style DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/styles/)
