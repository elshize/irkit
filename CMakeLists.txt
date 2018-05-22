cmake_minimum_required(VERSION 3.9)
project(IRKit VERSION 0.1 LANGUAGES CXX C)

option(IRKit_BUILD_TEST "whether or not to build the test" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

set(IRKit_CXX_PRIVATE_FEATURES cxx_std_17)
set(CMAKE_C_FLAGS "-Wall -std=c99")
set(CMAKE_CXX_FLAGS "-fno-strict-aliasing")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
set(Boost_USE_STATIC_LIBS ON)
add_definitions(-DGSL_THROW_ON_CONTRACT_VIOLATION)

# Find dependencies
find_package(PkgConfig REQUIRED)
find_package(Boost 1.40
    COMPONENTS iostreams program_options serialization filesystem system
    REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(gumbo REQUIRED gumbo)

include(submodules/submodules.cmake)

add_library(irkit INTERFACE)
target_include_directories(irkit INTERFACE include/)

add_subdirectory(src)

if (IRKit_BUILD_TEST)
    enable_testing()
    add_subdirectory(tests)
endif()
