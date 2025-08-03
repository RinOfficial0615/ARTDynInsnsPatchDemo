cmake_minimum_required(VERSION 3.10)
project(dyn_patcher)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

IF (CMAKE_BUILD_TYPE STREQUAL Debug)
    ADD_DEFINITIONS(-DDEBUG)
ENDIF()

find_package(xdl REQUIRED CONFIG)

add_subdirectory(dobby)

set(DYN_PATCHER_SOURCES
        dyn_patcher.cc
)

add_library(dyn_patcher SHARED ${DYN_PATCHER_SOURCES})
target_link_libraries(dyn_patcher dobby xdl::xdl android log)