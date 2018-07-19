cmake_minimum_required(VERSION 3.9)
project(IRKit VERSION 0.1 LANGUAGES CXX C)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

option(IRKit_BUILD_TEST "whether or not to build the test" ON)
option(IRKit_BUILD_INTEGRATION_TEST "whether or not to build the integration test" OFF)
option(IRKit_BUILD_BENCHMARKS "whether or not to build the benchmarks" OFF)
option(IRKit_INSTALL_SUBMODULES "whether or not to install submodules" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
MESSAGE(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

set(IRKit_CXX_PRIVATE_FEATURES cxx_std_17)
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c99 -fno-strict-aliasing -march=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -march=native")
set(Boost_USE_STATIC_LIBS ON)
add_definitions(-DGSL_THROW_ON_CONTRACT_VIOLATION)

if (EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
    include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)
find_package(Boost 1.40
    COMPONENTS iostreams filesystem system log
    REQUIRED)
find_package(zlib REQUIRED)
find_package(Threads REQUIRED)
MESSAGE(STATUS ${ZLIB_LIBRARIES})
find_package(gumbo-parser REQUIRED)
MESSAGE(STATUS ${gumbo-parser_LIBRARIES})
#pkg_check_modules(gumbo REQUIRED gumbo)

include(submodules/submodules.cmake)

add_library(irkit INTERFACE)
add_library(irkit::irkit ALIAS irkit)

target_include_directories(irkit INTERFACE
    $<BUILD_INTERFACE:${IRKit_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(irkit INTERFACE
    Boost::filesystem Boost::iostreams Boost::system)
add_dependencies(irkit ranges rax nlohmann_json GSL type_safe)
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
