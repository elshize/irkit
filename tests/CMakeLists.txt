cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0057 NEW)

include(GoogleTest)
include_directories(${GTEST_INCLUDE_DIRS})
set(CMAKE_EXE_LINKER_FLAGS "-pthread")

configure_file(randstr.txt randstr.txt COPYONLY)
#file(GLOB TEST_SOURCES test_*.cpp)
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
    target_link_libraries(${TEST_SRC_NAME} ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
    gtest_add_tests(TARGET ${TEST_SRC_NAME} TEST_PREFIX "${TEST_SRC_NAME}.")
endforeach(TEST_SRC)
target_link_libraries(test_prefixmap ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES} rax)

#add_executable(test_heap test_heap.cpp)
#target_link_libraries(test_heap ${GTEST_BOTH_LIBRARIES})
##add_test(HeapTests test_heap)
#gtest_add_tests(TARGET test_heap TEST_PREFIX test_heap.)
#
#add_executable(test_index test_index.cpp)
#target_link_libraries(test_index ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
##add_test(IndexTests test_index)
#gtest_add_tests(TARGET test_index TEST_PREFIX test_index.)
#
#add_executable(test_taat test_taat.cpp)
#target_link_libraries(test_taat ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
##add_test(TaatTests test_taat)
#gtest_add_tests(TARGET test_taat TEST_PREFIX test_taat.)
#
#add_executable(test_daat test_daat.cpp)
#target_link_libraries(test_daat ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
##add_test(DaatTests test_daat)
#gtest_add_tests(TARGET test_daat TEST_PREFIX test_daat.)
#
#add_executable(test_builder test_builder.cpp)
#target_link_libraries(test_builder ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
##add_test(IndexBuilderTests test_builder)
#gtest_add_tests(TARGET test_builder TEST_PREFIX test_builder.)
#
#add_executable(test_merger test_merger.cpp)
#target_link_libraries(test_merger ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
##add_test(IndexMergerTests test_merger)
#gtest_add_tests(TARGET test_merger TEST_PREFIX test_builder.)

#add_executable(test_index_reading test_index_reading.cpp)
#target_link_libraries(test_index_reading ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(IndexReadingTests test_index_reading)
#
#add_executable(test_posting_ranges test_posting_ranges.cpp)
#target_link_libraries(test_posting_ranges ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(PostingRangesTests test_posting_ranges)
#
#add_executable(test_coding test_coding.cpp)
#target_link_libraries(test_coding ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(CodingTests test_coding)
#
#add_executable(test_io test_io.cpp)
#target_link_libraries(test_io ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(IOTests test_io)
#
#add_executable(test_warc test_warc.cpp)
#target_link_libraries(test_warc ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(WarcTests test_warc)
#
#add_executable(test_argparser test_argparser.cpp)
#target_link_libraries(test_argparser ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(ArgumentParserTests test_argparser)
#
#add_executable(test_mutablebittrie test_mutablebittrie.cpp)
#target_link_libraries(test_mutablebittrie ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(MutableBitTrieTests test_mutablebittrie)
#
#add_executable(test_immutablebittrie test_immutablebittrie.cpp)
#target_link_libraries(test_immutablebittrie ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(ImmutableBitTrieTests test_immutablebittrie)
#
#configure_file(randstr.txt randstr.txt COPYONLY)
#add_executable(test_prefixmap test_prefixmap.cpp)
#target_link_libraries(test_prefixmap ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES} rax)
#add_test(PrefixMapTests test_prefixmap)
#
#add_executable(test_bitptr test_bitptr.cpp)
#target_link_libraries(test_bitptr ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(BitPointerTests test_bitptr)
#
#add_executable(test_index_integration test_index_integration.cpp)
#target_link_libraries(test_index_integration ${GTEST_BOTH_LIBRARIES} ${Boost_LIBRARIES})
#add_test(IndexIntegrationTests test_index_integration)
