cmake_minimum_required(VERSION 2.9)
project(PackageTest CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(test test.cpp)
target_link_libraries(test CONAN_PKG::irkit)
target_compile_features(test PRIVATE cxx_std_17)
