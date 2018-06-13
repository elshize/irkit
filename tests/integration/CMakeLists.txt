configure_file(terms.txt terms.txt COPYONLY)
configure_file(collection.txt collection.txt COPYONLY)

set(PREFIXMAP integration_test_prefixmap)
add_executable(${PREFIXMAP} integration_test_prefixmap.cpp)
target_link_libraries(${PREFIXMAP}
    irkit gtest gmock rax ranges Boost::filesystem)
target_compile_features(${PREFIXMAP} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${PREFIXMAP} TEST_PREFIX "integration.")

set(INVERTED_INDEX integration_test_index)
add_executable(${INVERTED_INDEX} integration_test_index.cpp)
target_link_libraries(${INVERTED_INDEX}
    irkit gtest gmock rax ranges Boost::filesystem)
target_compile_features(${INVERTED_INDEX} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${INVERTED_INDEX} TEST_PREFIX "integration.")
