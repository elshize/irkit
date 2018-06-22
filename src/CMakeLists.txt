add_executable(irk irk.cpp)
target_link_libraries(irk CLI11::CLI11)

#add_executable(irk-merge irk-merge.cpp)
#target_link_libraries(irk-merge ${Boost_LIBRARIES})

#add_executable(irk-compress irk-compress.cpp)
#target_link_libraries(irk-compress ${Boost_LIBRARIES})
#
#add_executable(irk-uncompress irk-uncompress.cpp)
#target_link_libraries(irk-uncompress ${Boost_LIBRARIES})
#
#add_executable(irk-query irk-query.cpp)
#target_link_libraries(irk-query ${Boost_LIBRARIES})

add_executable(irk-warc irk-warc.cpp)
target_link_libraries(irk-warc PUBLIC
    irkit CLI11::CLI11 ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${gumbo_LIBRARIES} GSL type_safe)
target_compile_features(irk-warc PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
target_compile_options(irk-warc PRIVATE -Wall)

add_executable(irk-part irk-part.cpp)
target_link_libraries(irk-part irkit CLI11::CLI11 ${Boost_LIBRARIES} GSL type_safe)
target_compile_features(irk-part PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-prefmap irk-prefmap.cpp)
target_link_libraries(irk-prefmap irkit CLI11::CLI11 ranges ${Boost_LIBRARIES} rax GSL type_safe)
target_compile_features(irk-prefmap PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-table irk-table.cpp)
target_link_libraries(irk-table irkit CLI11::CLI11 ${Boost_LIBRARIES} GSL type_safe)
target_compile_features(irk-table PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-buildindex irk-buildindex.cpp)
target_link_libraries(irk-buildindex
    irkit CLI11::CLI11 ranges ${Boost_LIBRARIES} rax nlohmann_json GSL Threads::Threads type_safe)
target_compile_features(irk-buildindex PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-score irk-score.cpp)
target_link_libraries(irk-score
    irkit CLI11::CLI11 ranges ${Boost_LIBRARIES} rax nlohmann_json GSL Threads::Threads type_safe)
target_compile_features(irk-score PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

add_executable(irk-postings irk-postings.cpp)
target_link_libraries(irk-postings
    irkit Boost::iostreams Boost::filesystem CLI11::CLI11
    rax GSL Threads::Threads nlohmann_json ranges type_safe)
target_compile_features(irk-postings PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})

install(
    TARGETS irk irk-warc irk-part irk-buildindex irk-score irk-postings irk-table
    DESTINATION bin)
