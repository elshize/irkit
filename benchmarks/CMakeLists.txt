macro(add_benchmark BENCH_NAME)
    add_executable(${BENCH_NAME} "bench_${BENCH_NAME}.cpp")
    target_link_libraries(${BENCH_NAME} irkit CONAN_PKG::CLI11)
    target_compile_features(${BENCH_NAME} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
endmacro()

configure_file(bench_collection.txt bench_collection.txt COPYONLY)
configure_file(init.sh init.sh COPYONLY)

add_benchmark(posting_list_view)
add_benchmark(varbyte)
add_benchmark(taat)
add_benchmark(queryproc)
