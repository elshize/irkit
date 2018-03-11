cmake_minimum_required(VERSION 3.9)
project(IRKit CXX C)
set(IRKit_VERSION_MAJOR 0)
set(IRKit_VERSION_MINOR 1)

option(IRKit_BUILD_TEST "whether or not to build the test" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

# Set variables
set(CMAKE_CXX_FLAGS "-Wall -std=c++17")
set(CMAKE_C_FLAGS "-Wall -std=c99")
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

include_directories(include
                    ${rax_INCLUDE_DIR}
                    ${GSL_INCLUDE_DIR}
                    ${range_INCLUDE_DIR}
                    ${json_INCLUDE_DIR}
                    ${mongoose_INCLUDE_DIR}
                    ${debug_assert_INCLUDE_DIR}
                    ${type_safe_INCLUDE_DIR}
                    ${CLI11_INCLUDE_DIR})

add_subdirectory(src)

if (IRKit_BUILD_TEST)
    enable_testing()
    add_subdirectory(tests)
endif()
