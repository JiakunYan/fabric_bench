cmake_minimum_required(VERSION 3.13)
project(fabric_bench)

set(FB_SERVER ofi CACHE STRING "Server: ofi MPI ibv")

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

find_package(OpenMP REQUIRED)
if("ofi" IN_LIST FB_SERVER)
    find_package(Fabric REQUIRED)
    set(FB_USE_OFI ON)
endif()

configure_file(include/config.hpp.in include/config.hpp @ONLY)
add_subdirectory(modules)

add_library(fb_lib INTERFACE)
target_include_directories(fb_lib INTERFACE
        ${CMAKE_BINARY_DIR}/include
        include
)
target_link_libraries(fb_lib INTERFACE
        mlog-obj
        pmi-obj
        Fabric::OFI
        OpenMP::OpenMP_CXX
)

option(USE_GPROF "use gprof" OFF)
if (USE_GPROF)
    # export GMON_OUT_PREFIX="gmon.out"
    target_compile_options(fb_lib INTERFACE -pg -fno-inline)
    target_link_options(fb_lib INTERFACE -pg)
endif()

add_subdirectory(benchmark)