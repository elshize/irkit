cmake_minimum_required(VERSION 2.6)
project(IRKit)

# Set variables
set(CMAKE_CXX_COMPILER, "g++")
set(CMAKE_CXX_FLAGS "-g -Wall -std=c++17 -fPIC")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
set(Boost_USE_STATIC_LIBS ON)
add_definitions(-DGSL_THROW_ON_CONTRACT_VIOLATION)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Find dependencies
find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost 1.40
    COMPONENTS iostreams program_options serialization
    REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(gumbo REQUIRED gumbo)
find_library(DHT dht)

# Set include directories
include_directories(${Boost_INCLUDE_DIR})
include_directories(${gumbo_INCLUDE_DIRS})
include_directories(snowball)
include_directories(include lib/include)

set(diskhash libdht.so)

# For mongoose
# TODO: deal with this dependency
add_subdirectory(lib)

add_subdirectory(src/blip)

add_executable(runtaat src/runtaat.cpp)
target_link_libraries(runtaat stdc++fs)

add_executable(rundaat src/rundaat.cpp)
target_link_libraries(rundaat stdc++fs)

add_executable(build_index src/build_index.cpp)
target_link_libraries(build_index stdc++fs)

add_executable(irk-merge src/irk-merge.cpp)
target_link_libraries(irk-merge stdc++fs ${Boost_LIBRARIES} ${DHT})

add_executable(irk-postings src/irk-postings.cpp)
target_link_libraries(irk-postings stdc++fs ${Boost_LIBRARIES} ${DHT})

add_executable(irk-compress src/irk-compress.cpp)
target_link_libraries(irk-compress stdc++fs ${Boost_LIBRARIES} ${DHT})

add_executable(irk-uncompress src/irk-uncompress.cpp)
target_link_libraries(irk-uncompress stdc++fs ${Boost_LIBRARIES} ${DHT})

add_executable(irk-query src/irk-query.cpp)
target_link_libraries(irk-query stdc++fs ${Boost_LIBRARIES} ${DHT})

add_executable(irk-warc src/irk-warc.cpp)
target_link_libraries(irk-warc
    stdc++fs ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${gumbo_LIBRARIES} ${DHT})

add_executable(irk-part src/irk-part.cpp)
target_link_libraries(irk-part stdc++fs ${Boost_LIBRARIES} ${DHT})

add_executable(irk-termmap src/irk-termmap.cpp)
target_link_libraries(irk-termmap stdc++fs ${Boost_LIBRARIES} ${DHT})

enable_testing()
add_subdirectory(tests)
