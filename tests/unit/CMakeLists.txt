set(VECTOR_INVERTED_LISTS vector_inverted_lists)
add_executable(${VECTOR_INVERTED_LISTS} unit_test_vector_inverted_lists.cpp)
target_link_libraries(${VECTOR_INVERTED_LISTS}
    irkit gtest gmock rax Boost::filesystem GSL streamvbyte_static debug_assert)
target_compile_features(${VECTOR_INVERTED_LISTS} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${VECTOR_INVERTED_LISTS} TEST_PREFIX "unit.")

set(POSTING_LIST posting_list)
add_executable(${POSTING_LIST} unit_test_posting_list.cpp)
target_link_libraries(${POSTING_LIST}
    irkit gtest gmock rax Boost::filesystem GSL streamvbyte_static debug_assert)
target_compile_features(${POSTING_LIST} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${POSTING_LIST} TEST_PREFIX "unit.")

set(COMPACT_TABLE compact_table)
add_executable(${COMPACT_TABLE} unit_test_compact_table.cpp)
target_link_libraries(${COMPACT_TABLE}
    irkit gtest gmock rax Boost::filesystem GSL type_safe debug_assert streamvbyte_static)
target_compile_features(${COMPACT_TABLE} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${COMPACT_TABLE} TEST_PREFIX "unit.")

set(NUMERIC_CODEC numeric_codec)
add_executable(${NUMERIC_CODEC} unit_test_numeric_codec.cpp)
target_link_libraries(${NUMERIC_CODEC}
    irkit gtest gmock rax Boost::filesystem GSL type_safe debug_assert streamvbyte_static)
target_compile_features(${NUMERIC_CODEC} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${NUMERIC_CODEC} TEST_PREFIX "unit.")

set(STEMMER stemmer)
add_executable(${STEMMER} unit_test_stemmer.cpp)
target_link_libraries(${STEMMER}
    irkit gtest gmock rax Boost::filesystem GSL type_safe debug_assert streamvbyte_static)
target_compile_features(${STEMMER} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${STEMMER} TEST_PREFIX "unit.")
