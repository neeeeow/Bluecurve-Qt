cmake_minimum_required(VERSION 3.16)

project(bluecurve-style)

set(KF5_MIN_VERSION "5.102.0")
set(KF6_MIN_VERSION "6.18.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")
set(QT5_MIN_VERSION "5.15.2")
set(QT_MIN_VERSION "6.9.0")

include(FeatureSummary)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH}  ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include(KDECompilerSettings NO_POLICY_SCOPE)

option(BUILD_QT5 "Build Qt5 style" ON)
option(BUILD_QT6 "Build with Qt6" ON)

function(build_Qt5)
  set(QT_MAJOR_VERSION 5)
  include(KDEInstallDirs5)
  include(KDECMakeSettings)

  find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

  # Set a unique folder for the qt5 build
  add_subdirectory(src build-qt5)

  unset(QUERY_EXECUTABLE CACHE)
endfunction()

function(build_Qt6)
  set(QT_MAJOR_VERSION 6)
  include(KDEInstallDirs6)
  include(KDECMakeSettings)
  
  find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
  
  # Different binary folder name
  add_subdirectory(src build-qt6)
  
  unset(QUERY_EXECUTABLE CACHE)
endfunction()

# Build the style
if(BUILD_QT5)
  build_Qt5()
endif()

if(BUILD_QT6)
  build_Qt6()
endif()

# Install the color theme
include(KDEInstallDirs)
install(FILES Bluecurve.colors DESTINATION ${KDE_INSTALL_DATADIR}/color-schemes/)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
