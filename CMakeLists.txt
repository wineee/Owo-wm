cmake_minimum_required(VERSION 3.25)

project(qwlbox
    VERSION 0.0.1
    DESCRIPTION "a better tinywm"
    HOMEPAGE_URL "https://github.com/wineee/qwlbox"
    LANGUAGES CXX C
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

set(target qwlbox)
set(QT_VERSION_MAJOR 6)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui)

find_package(QWlroots REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots>=0.16.2)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_search_module(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)

include(WaylandScannerHelpers)
ws_generate(server stable/xdg-shell/xdg-shell.xml xdg-shell-protocol)

set(PROJECT_SOURCES src/main.cpp)
qt_add_executable(${target}
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(${target}
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Gui
    QWlroots::QWlroots
    PkgConfig::WAYLAND_SERVER
    PkgConfig::WLROOTS
    PkgConfig::PIXMAN
    PkgConfig::XKBCOMMON
)

install(TARGETS ${target} DESTINATION ${CMAKE_INSTALL_BINDIR})

qt_finalize_executable(${target})

