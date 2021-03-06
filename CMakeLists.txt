cmake_minimum_required(VERSION 3.11)
project(IRKit VERSION 0.1 LANGUAGES CXX C)

##
## Options
##
option(IRKit_BUILD_TEST "whether or not to build the test" ON)
option(IRKit_BUILD_INTEGRATION_TEST "whether or not to build the integration test" OFF)
option(IRKit_BUILD_BENCHMARKS "whether or not to build the benchmarks" OFF)
option(IRKit_USE_SYSTEM_BOOST "use system boost instead of conan dependency" OFF)
option(IRKit_NO_SANITIZERS "do not use sanitizers for Debug" OFF)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG_ASSERT")
if(NOT IRKit_NO_SANITIZERS)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fno-sanitize=alignment")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined")
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()
MESSAGE(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
if(CMAKE_BUILD_TYPE EQUAL "Debug")
    add_compile_definitions("SPDLOG_DEBUG_ON")
endif()

set(IRKit_CXX_PRIVATE_FEATURES cxx_std_17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99 -fno-strict-aliasing -march=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -fno-strict-aliasing -march=native")
set(Boost_USE_STATIC_LIBS ON)
add_definitions(-DGSL_THROW_ON_CONTRACT_VIOLATION)
add_definitions(-DBOOST_NO_AUTO_PTR)
add_definitions(-Dnsel_CONFIG_CONFIRMS_COMPILATION_ERRORS=1)
add_definitions(-DBOOST_NO_CXX11_SCOPED_ENUMS)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_library(irkit INTERFACE)
add_library(irkit::irkit ALIAS irkit)

target_include_directories(irkit INTERFACE
    $<BUILD_INTERFACE:${IRKit_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

find_package(Boost COMPONENTS iostreams filesystem system REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)
find_package(range-v3 REQUIRED)
find_package(TBB REQUIRED)
find_package(ParallelSTL REQUIRED)
find_package(cppitertools REQUIRED)
find_package(rax REQUIRED)
find_package(taily REQUIRED)

target_link_libraries(irkit INTERFACE
    Threads::Threads
    Boost::boost
    Boost::iostreams
    Boost::system
    Boost::filesystem
    fmt::fmt
    spdlog::spdlog
    nlohmann_json
    range-v3
    TBB::tbb
    pstl::ParallelSTL
    cppitertools
    CONAN_PKG::rax
    taily::taily
    CONAN_PKG::type_safe
    CONAN_PKG::debug_assert
    CONAN_PKG::streamvbyte
    CONAN_PKG::gsl_microsoft
)

install(DIRECTORY include/irkit DESTINATION include)
install(DIRECTORY include/nonstd DESTINATION include)
#install(FILES irkit-config.cmake DESTINATION lib/irkit)
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
