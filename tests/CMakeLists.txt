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

#add_executable(test_views test_views.cpp)
#target_link_libraries(test_views ${GTEST_BOTH_LIBRARIES})
#add_test(ViewTests test_views)
