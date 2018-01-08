cmake_minimum_required(VERSION 2.6)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
project(irkit)
set(CMAKE_CXX_COMPILER, "g++")
set(CMAKE_CXX_FLAGS "-g -Wall -std=c++17 -fPIC -fconcepts")
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

add_executable(server src/server.cpp)
target_link_libraries(server stdc++fs mongoose)

add_executable(runtaat src/runtaat.cpp)
target_link_libraries(runtaat stdc++fs)

add_executable(rundaat src/rundaat.cpp)
target_link_libraries(rundaat stdc++fs)

add_executable(build_index src/build_index.cpp)
target_link_libraries(build_index stdc++fs)

add_executable(run_queries src/run_queries.cpp)
target_link_libraries(run_queries stdc++fs)

add_subdirectory(tests)
