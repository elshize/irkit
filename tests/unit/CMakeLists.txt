set(VECTOR_INVERTED_LISTS vector_inverted_lists)
add_executable(${VECTOR_INVERTED_LISTS} unit_test_vector_inverted_lists.cpp)
target_link_libraries(${VECTOR_INVERTED_LISTS}
    irkit gtest gmock rax Boost::filesystem)
target_compile_features(${VECTOR_INVERTED_LISTS} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${VECTOR_INVERTED_LISTS} TEST_PREFIX "unit.")
