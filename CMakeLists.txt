cmake_minimum_required(VERSION 3.16)
project(BluecurveStyle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Enable automoc, autouic, etc.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

add_library(bluecurve-style MODULE
	src/bits.cpp
    src/bluecurvestyle.cpp
    src/bluecurvestyle.h
    src/bluecurvestyleplugin.cpp
    src/bluecurvestyleplugin.h
    src/bluecurve.json   # Embed JSON metadata here
)

target_include_directories(bluecurve-style PRIVATE src)

target_link_libraries(bluecurve-style
    Qt6::Widgets
)

set_target_properties(bluecurve-style PROPERTIES
    OUTPUT_NAME bluecurve
)

# Install location (optional, you can skip if testing in build dir)
install(TARGETS bluecurve-style
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt6/plugins/styles
)
