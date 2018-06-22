configure_file(terms.txt terms.txt COPYONLY)
configure_file(collection.txt collection.txt COPYONLY)

# TODO: See Issue #16
#set(PREFIXMAP integration_test_prefixmap)
#add_executable(${PREFIXMAP} integration_test_prefixmap.cpp)
#target_link_libraries(${PREFIXMAP}
#    irkit gtest gmock rax ranges Boost::filesystem GSL)
#target_compile_features(${PREFIXMAP} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
#gtest_add_tests(TARGET ${PREFIXMAP} TEST_PREFIX "integration.")

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
    nlohmann_json GSL Threads::Threads type_safe)
target_compile_features(${INVERTED_INDEX} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
gtest_add_tests(TARGET ${INVERTED_INDEX} TEST_PREFIX "integration.")
