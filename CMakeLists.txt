cmake_minimum_required(VERSION 2.6)
project(IRKit CXX C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

# Set variables
set(CMAKE_CXX_FLAGS "-Wall -std=c++17")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
set(Boost_USE_STATIC_LIBS ON)
add_definitions(-DGSL_THROW_ON_CONTRACT_VIOLATION)

# Find dependencies
find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost 1.40
    COMPONENTS iostreams program_options serialization filesystem
    REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(gumbo REQUIRED gumbo)

# For mongoose
# TODO: deal with this dependency
include_directories(include lib/include)
add_subdirectory(lib)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
