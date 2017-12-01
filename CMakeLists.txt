cmake_minimum_required(VERSION 2.6)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
project(bloodhound 0.1)
#set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_CXX_FLAGS "-g -Wall -std=c++17 -fPIC")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

enable_testing()
find_package(UnitTest++ REQUIRED)
find_package(GTest REQUIRED)

ExternalProject_Add(
    gsl
    GIT_REPOSITORY https://github.com/Microsoft/GSL.git
    CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND ""
)

include_directories(include lib/include)

add_subdirectory(lib)

add_executable(server server.cpp)
target_link_libraries(server stdc++fs mongoose)

add_subdirectory(tests)
