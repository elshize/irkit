include_directories(${GTEST_INCLUDE_DIRS})
set(CMAKE_EXE_LINKER_FLAGS "-pthread")

add_executable(test_heap test_heap.cpp)
target_link_libraries(test_heap ${GTEST_BOTH_LIBRARIES})
add_test(HeapTests test_heap)

add_executable(test_index test_index.cpp)
target_link_libraries(test_index ${GTEST_BOTH_LIBRARIES} stdc++fs)
add_test(IndexTests test_index)

add_executable(test_taat test_taat.cpp)
target_link_libraries(test_taat ${GTEST_BOTH_LIBRARIES})
add_test(TaatTests test_taat)

add_executable(test_daat test_daat.cpp)
target_link_libraries(test_daat ${GTEST_BOTH_LIBRARIES})
add_test(DaatTests test_daat)

add_executable(test_builder test_builder.cpp)
target_link_libraries(test_builder ${GTEST_BOTH_LIBRARIES} stdc++fs)
add_test(IndexBuilderTests test_builder)

add_executable(test_merger test_merger.cpp)
target_link_libraries(test_merger ${GTEST_BOTH_LIBRARIES} stdc++fs)
add_test(IndexMergerTests test_merger)

add_executable(test_index_reading test_index_reading.cpp)
target_link_libraries(test_index_reading ${GTEST_BOTH_LIBRARIES} stdc++fs)
add_test(IndexReadingTests test_index_reading)

add_executable(test_posting_ranges test_posting_ranges.cpp)
target_link_libraries(test_posting_ranges ${GTEST_BOTH_LIBRARIES} stdc++fs)
add_test(PostingRangesTests test_posting_ranges)

add_executable(test_coding test_coding.cpp)
target_link_libraries(test_coding ${GTEST_BOTH_LIBRARIES} stdc++fs)
add_test(CodingTests test_coding)

add_executable(test_index_integration test_index_integration.cpp)
target_link_libraries(test_index_integration ${GTEST_BOTH_LIBRARIES} stdc++fs)
add_test(IndexIntegrationTests test_index_integration)
