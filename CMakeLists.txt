cmake_minimum_required(VERSION 3.9)
project(IRKit VERSION 0.1 LANGUAGES CXX C)

option(IRKit_BUILD_TEST "whether or not to build the test" ON)
option(IRKit_INSTALL_SUBMODULES "whether or not to install submodules" OFF)

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
    COMPONENTS iostreams filesystem system
    REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(gumbo REQUIRED gumbo)

include(submodules/submodules.cmake)

add_library(irkit INTERFACE)
target_include_directories(irkit INTERFACE
    $<BUILD_INTERFACE:${MY_LIBRARY_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(irkit INTERFACE
    Boost::filesystem Boost::iostreams Boost::system)
install(DIRECTORY include/irkit DESTINATION include)
install(FILES irkit-config.cmake DESTINATION lib/irkit)
install(TARGETS irkit EXPORT irkit DESTINATION lib)
install(EXPORT irkit DESTINATION lib/irkit)

add_subdirectory(src)

if (IRKit_BUILD_TEST)
    enable_testing()
    add_subdirectory(tests)
endif()
