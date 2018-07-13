add_executable(irk irk.cpp)
target_link_libraries(irk CLI11::CLI11)

set(COMMON_LIBRARIES
    irkit
    CLI11::CLI11
    Boost::iostreams
    Boost::filesystem
    Boost::log
    GSL
    debug_assert
    type_safe
    ranges
    nlohmann_json
    streamvbyte_static)

add_executable(irk-warc irk-warc.cpp)
target_link_libraries(irk-warc PUBLIC ${COMMON_LIBRARIES} ${ZLIB_LIBRARIES} ${gumbo_LIBRARIES})
target_compile_features(irk-warc PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
target_compile_options(irk-warc PRIVATE -Wall)

add_executable(irk-part irk-part.cpp)
target_link_libraries(irk-part ${COMMON_LIBRARIES})
target_compile_features(irk-part PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-lexicon irk-lexicon.cpp)
target_link_libraries(irk-lexicon ${COMMON_LIBRARIES} rax)
target_compile_features(irk-lexicon PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-table irk-table.cpp)
target_link_libraries(irk-table ${COMMON_LIBRARIES})
target_compile_features(irk-table PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-buildindex irk-buildindex.cpp)
target_link_libraries(irk-buildindex ${COMMON_LIBRARIES} rax Threads::Threads)
target_compile_features(irk-buildindex PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-score irk-score.cpp)
target_link_libraries(irk-score ${COMMON_LIBRARIES} rax Threads::Threads)
target_compile_features(irk-score PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-postings irk-postings.cpp)
target_link_libraries(irk-postings ${COMMON_LIBRARIES} rax Threads::Threads)
target_compile_features(irk-postings PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-query irk-query.cpp)
target_link_libraries(irk-query ${COMMON_LIBRARIES} rax Threads::Threads)
target_compile_features(irk-query PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-lookup irk-lookup.cpp)
target_link_libraries(irk-lookup ${COMMON_LIBRARIES} rax Threads::Threads)
target_compile_features(irk-lookup PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-hits irk-hits.cpp)
target_link_libraries(irk-hits CLI11::CLI11 Boost::log Threads::Threads)
target_compile_features(irk-hits PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

install(
    TARGETS
        irk
        irk-warc
        irk-part
        irk-buildindex
        irk-score
        irk-postings
        irk-query
        irk-table
        irk-lexicon
        irk-hits
    DESTINATION bin)
