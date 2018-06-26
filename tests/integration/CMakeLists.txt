configure_file(terms.txt terms.txt COPYONLY)
configure_file(collection.txt collection.txt COPYONLY)

set(PREFIXMAP integration_test_prefixmap)
add_executable(${PREFIXMAP} integration_test_prefixmap.cpp)
target_link_libraries(${PREFIXMAP}
    irkit gtest gmock rax ranges Boost::filesystem GSL)
target_compile_features(${PREFIXMAP} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${PREFIXMAP} TEST_PREFIX "integration.")

set(HUTUCKER integration_test_hutucker)
add_executable(${HUTUCKER} integration_test_hutucker.cpp)
target_link_libraries(${HUTUCKER}
    irkit gtest gmock rax ranges Boost::filesystem GSL Threads::Threads)
target_compile_features(${HUTUCKER} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${HUTUCKER} TEST_PREFIX "integration.")

set(INVERTED_INDEX integration_test_index)
add_executable(${INVERTED_INDEX} integration_test_index.cpp)
target_link_libraries(${INVERTED_INDEX}
    irkit gtest gmock rax ranges Boost::filesystem Boost::iostreams Boost::log
    nlohmann_json GSL Threads::Threads type_safe debug_assert)
target_compile_features(${INVERTED_INDEX} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${INVERTED_INDEX} TEST_PREFIX "integration.")

set(RADIX_TREE radix_tree)
add_executable(${RADIX_TREE} integration_test_radix_tree.cpp)
target_link_libraries(${RADIX_TREE}
    irkit gtest gmock rax ranges Boost::filesystem Boost::iostreams Boost::log
    nlohmann_json GSL Threads::Threads type_safe debug_assert)
target_compile_features(${RADIX_TREE} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${RADIX_TREE} TEST_PREFIX "integration.")
