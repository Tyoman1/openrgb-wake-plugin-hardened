cmake_minimum_required(VERSION 3.21)

project(OpenRGBWakePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(OpenRGBWakePlugin SHARED
    src/WakePlugin.cpp
    src/WakePlugin.h
    src/SettingsWidget.cpp
    src/SettingsWidget.h
    src/PowerWatcher.cpp
    src/PowerWatcher.h
    src/MouseActivityWatcher.cpp
    src/MouseActivityWatcher.h
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

set_target_properties(OpenRGBWakePlugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "OpenRGBWakePlugin"
)

if(MSVC)
    target_compile_options(OpenRGBWakePlugin PRIVATE
        /utf-8
        /guard:cf
        /sdl
    )
    target_link_options(OpenRGBWakePlugin PRIVATE
        /guard:cf
        /CETCOMPAT
        /DYNAMICBASE
        /NXCOMPAT
        /HIGHENTROPYVA
    )
endif()
