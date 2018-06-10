configure_file(terms.txt terms.txt COPYONLY)

set(PREFIXMAP integration_test_prefixmap)
add_executable(${PREFIXMAP} integration_test_prefixmap.cpp)
target_link_libraries(${PREFIXMAP} irkit gtest gmock rax Boost::filesystem)
target_compile_features(${PREFIXMAP} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${PREFIXMAP} TEST_PREFIX "integration.")
