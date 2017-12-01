include_directories(${GTEST_INCLUDE_DIRS})
set(CMAKE_EXE_LINKER_FLAGS "-pthread")

add_executable(test_heap test_heap.cpp)
target_link_libraries(test_heap ${GTEST_BOTH_LIBRARIES})
add_test(HeapTests test_heap)

add_executable(test_index test_index.cpp)
target_link_libraries(test_index ${GTEST_BOTH_LIBRARIES} stdc++fs)
add_test(IndexTests test_index)
