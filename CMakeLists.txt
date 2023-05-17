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
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

add_subdirectory(src)
