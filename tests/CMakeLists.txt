cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0057 NEW)

find_package(Catch2 REQUIRED)
find_package(GTest REQUIRED)
include(GoogleTest)
include_directories(${GTEST_INCLUDE_DIRS})
set(CMAKE_EXE_LINKER_FLAGS "-pthread")

include(CTest)
include(Catch)

# TODO: Move to `unit` folder.
configure_file(randstr.txt randstr.txt COPYONLY)
configure_file(ukb_queries ukb_queries COPYONLY)
configure_file(doclist.txt doclist.txt COPYONLY)
set(TEST_SOURCES
    test_bitptr.cpp
    test_builder.cpp
    test_coding.cpp
    test_daat.cpp
    test_io.cpp
    test_prefixmap.cpp
    test_radix_tree.cpp
    test_taat.cpp
    test_warc.cpp
    test_block.cpp
    test_memoryview.cpp)
foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component (TEST_SRC_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_SRC_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_SRC_NAME} irkit GTest::GTest)
    target_compile_features(${TEST_SRC_NAME} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
    gtest_add_tests(TARGET ${TEST_SRC_NAME} TEST_PREFIX "${TEST_SRC_NAME}.")
endforeach(TEST_SRC)
target_link_libraries(test_prefixmap GTest::GTest)

add_subdirectory(unit)

if (IRKit_BUILD_INTEGRATION_TEST)
    add_subdirectory(integration)
endif()
