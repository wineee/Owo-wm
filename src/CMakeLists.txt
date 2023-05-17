set(target qwlbox)
set(QT_VERSION_MAJOR 6)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui)

find_package(QWlroots REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots=0.16.2)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_search_module(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)

include(WaylandScannerHelpers)
ws_generate(server stable/xdg-shell/xdg-shell.xml xdg-shell-protocol)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS *.cpp *.h)

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

include(PackageVersionHelper)
setup_package_version_variables(WLROOTS)

target_compile_definitions(${target}
    PRIVATE
    WLR_USE_UNSTABLE
)

set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE TRUE)

install(TARGETS ${target} DESTINATION ${CMAKE_INSTALL_BINDIR})

qt_finalize_executable(${target})

