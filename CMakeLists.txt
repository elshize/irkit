cmake_minimum_required(VERSION 3.9)
project(IRKit VERSION 0.1 LANGUAGES CXX C)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

option(IRKit_BUILD_TEST "whether or not to build the test" ON)
option(IRKit_BUILD_INTEGRATION_TEST "whether or not to build the integration test" OFF)
option(IRKit_BUILD_BENCHMARKS "whether or not to build the benchmarks" OFF)
option(IRKit_USE_SYSTEM_BOOST "use system boost instead of conan dependency" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
MESSAGE(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

set(IRKit_CXX_PRIVATE_FEATURES cxx_std_17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99 -fno-strict-aliasing -march=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -march=native")
set(Boost_USE_STATIC_LIBS ON)
add_definitions(-DGSL_THROW_ON_CONTRACT_VIOLATION)
add_definitions(-DBOOST_NO_AUTO_PTR)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_library(irkit INTERFACE)
add_library(irkit::irkit ALIAS irkit)

target_include_directories(irkit INTERFACE
    $<BUILD_INTERFACE:${IRKit_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
if (IRKit_USE_SYSTEM_BOOST)
    find_package(Boost COMPONENTS
        iostreams filesystem system log thread REQUIRED)
    target_link_libraries(irkit INTERFACE
        Boost::boost
        Boost::iostreams
        Boost::filesystem
        Boost::system
        Boost::log
        Boost::thread
        CONAN_PKG::rangev3
        CONAN_PKG::jsonformoderncpp
        CONAN_PKG::type_safe
        CONAN_PKG::debug_assert
        CONAN_PKG::gsl_microsoft
        CONAN_PKG::streamvbyte
        CONAN_PKG::rax
        pthread)
else()
    #add_definitions(-DBOOST_LOG_DYN_LINK)
    target_link_libraries(irkit INTERFACE
        CONAN_PKG::boost
        CONAN_PKG::rangev3
        CONAN_PKG::jsonformoderncpp
        CONAN_PKG::type_safe
        CONAN_PKG::debug_assert
        CONAN_PKG::gsl_microsoft
        CONAN_PKG::streamvbyte
        CONAN_PKG::rax
        pthread)
endif()
install(DIRECTORY include/irkit DESTINATION include)
install(FILES irkit-config.cmake DESTINATION lib/irkit)
install(TARGETS irkit EXPORT irkit DESTINATION lib)
install(EXPORT irkit DESTINATION lib/irkit)
export(PACKAGE irkit)

add_subdirectory(src)
configure_file(scripts/run-tests.sh run-tests.sh COPYONLY)

if (IRKit_BUILD_TEST)
    enable_testing()
    add_subdirectory(tests)
endif()

if (IRKit_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
