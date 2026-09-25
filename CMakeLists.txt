cmake_minimum_required(VERSION 3.16)

project(weasel-fluent-cpp-qt VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.8.2/msvc2022_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)

# Disable gallery/tests in Fluent-Qt submodule
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_INSTALL OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_ENABLE_WARNINGS OFF CACHE BOOL "" FORCE)

add_subdirectory(3rdparty/Fluent-Qt)

set(SOURCES
    src/main.cpp
    src/config.h
    src/config.cpp
    src/rime_engine.h
    src/rime_engine.cpp
    src/candidate_window.h
    src/candidate_window.cpp
    src/settings_window.h
    src/settings_window.cpp
    src/emoji_picker_window.h
    src/emoji_picker_window.cpp
)

add_executable(weasel-fluent-cpp-qt ${SOURCES})

target_include_directories(weasel-fluent-cpp-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Fluent-Qt/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Fluent-Qt/src
)

target_link_libraries(weasel-fluent-cpp-qt PRIVATE
    FluentQt::FluentQt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
)

if(WIN32)
    target_link_libraries(weasel-fluent-cpp-qt PRIVATE
        User32
        Gdi32
        Dwmapi
    )
endif()
