cmake_minimum_required(VERSION 2.6)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
project(irkit)
set(CMAKE_CXX_COMPILER, "g++")
set(CMAKE_CXX_FLAGS "-O3 -Wall -std=c++17 -fPIC -fconcepts")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

add_definitions(-DGSL_THROW_ON_CONTRACT_VIOLATION)

enable_testing()
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

add_executable(runtaat runtaat.cpp)
target_link_libraries(runtaat stdc++fs)

add_executable(rundaat rundaat.cpp)
target_link_libraries(rundaat stdc++fs)

add_subdirectory(tests)
