cmake_minimum_required(VERSION 3.12)

project(wiiurpxlib
    LANGUAGES CXX
    VERSION 1.0
)

add_library(wiiurpx
    ${PROJECT_SOURCE_DIR}/source/wiiurpxlib.cpp
)
add_library(wiiurpxlib::wiiurpxlib ALIAS wiiurpx)
set_property(TARGET wiiurpx PROPERTY CXX_STANDARD 20)

target_include_directories(wiiurpx
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/source
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(zlib REQUIRED IMPORTED_TARGET zlib)

target_link_libraries(wiiurpx PRIVATE
    PkgConfig::zlib
)
