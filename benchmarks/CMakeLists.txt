configure_file(bench_collection.txt bench_collection.txt COPYONLY)
configure_file(init.sh init.sh COPYONLY)

add_executable(posting_list_view bench_posting_list_view.cpp)
target_link_libraries(posting_list_view
    irkit CLI11::CLI11 ranges ${Boost_LIBRARIES} rax nlohmann_json GSL Threads::Threads
    streamvbyte type_safe debug_assert)
target_compile_features(posting_list_view PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(varbyte bench_varbyte.cpp)
target_link_libraries(varbyte
    irkit CLI11::CLI11 ranges ${Boost_LIBRARIES} rax nlohmann_json GSL Threads::Threads
    streamvbyte type_safe debug_assert)
target_compile_features(varbyte PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(taat bench_taat.cpp)
target_link_libraries(taat
    irkit CLI11::CLI11 ranges ${Boost_LIBRARIES} rax nlohmann_json GSL Threads::Threads
    streamvbyte type_safe debug_assert)
target_compile_features(taat PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
