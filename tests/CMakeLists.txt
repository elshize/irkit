cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0057 NEW)

include(GoogleTest)
include_directories(${GTEST_INCLUDE_DIRS})
set(CMAKE_EXE_LINKER_FLAGS "-pthread")

configure_file(randstr.txt randstr.txt COPYONLY)
set(TEST_SOURCES
    test_bitptr.cpp
    test_builder.cpp
    test_coding.cpp
    test_daat.cpp
    test_heap.cpp
    test_immutablebittrie.cpp
    test_index.cpp
    test_index_integration.cpp
    test_index_reading.cpp
    test_io.cpp
    test_merger.cpp
    test_mutablebittrie.cpp
    test_posting_ranges.cpp
    test_prefixmap.cpp
    test_radix_tree.cpp
    test_taat.cpp
    test_warc.cpp)
foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component (TEST_SRC_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_SRC_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_SRC_NAME}
        irkit debug_assert ranges rax gtest gmock nlohmann_json ${Boost_LIBRARIES})
    target_compile_features(${TEST_SRC_NAME} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
    gtest_add_tests(TARGET ${TEST_SRC_NAME} TEST_PREFIX "${TEST_SRC_NAME}.")
endforeach(TEST_SRC)
target_link_libraries(test_prefixmap ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES} rax)
