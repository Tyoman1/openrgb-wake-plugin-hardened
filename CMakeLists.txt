cmake_minimum_required(VERSION 3.21)

project(OpenRGBWakePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# The OpenRGB plugin SDK headers include nlohmann/json
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(OpenRGBWakePlugin SHARED
    src/WakePlugin.cpp
    src/WakePlugin.h
    src/SettingsWidget.cpp
    src/SettingsWidget.h
    src/PowerWatcher.cpp
    src/PowerWatcher.h
)

target_include_directories(OpenRGBWakePlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/OpenRGBPluginSDK
)

target_link_libraries(OpenRGBWakePlugin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    nlohmann_json::nlohmann_json
)

# Produce "OpenRGBWakePlugin.dll" (no lib/ prefix)
set_target_properties(OpenRGBWakePlugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "OpenRGBWakePlugin"
)

# Russian strings in the settings widget need explicit UTF-8 on MSVC
if(MSVC)
    target_compile_options(OpenRGBWakePlugin PRIVATE /utf-8)
endif()