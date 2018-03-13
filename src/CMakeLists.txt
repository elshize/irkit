add_subdirectory(bovw)

add_executable(runtaat runtaat.cpp)
target_link_libraries(runtaat ${Boost_LIBRARIES})

add_executable(rundaat rundaat.cpp)
target_link_libraries(rundaat ${Boost_LIBRARIES})

add_executable(build_index build_index.cpp)
target_link_libraries(build_index ${Boost_LIBRARIES})



# irk commands

add_executable(irk irk.cpp)
target_link_libraries(irk)

add_executable(irk-merge irk-merge.cpp)
target_link_libraries(irk-merge ${Boost_LIBRARIES})

add_executable(irk-postings irk-postings.cpp)
target_link_libraries(irk-postings ${Boost_LIBRARIES})

add_executable(irk-compress irk-compress.cpp)
target_link_libraries(irk-compress ${Boost_LIBRARIES})

add_executable(irk-uncompress irk-uncompress.cpp)
target_link_libraries(irk-uncompress ${Boost_LIBRARIES})

add_executable(irk-query irk-query.cpp)
target_link_libraries(irk-query ${Boost_LIBRARIES})

add_executable(irk-warc irk-warc.cpp)
target_link_libraries(irk-warc
    ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${gumbo_LIBRARIES})

add_executable(irk-part irk-part.cpp)
target_link_libraries(irk-part ${Boost_LIBRARIES})

add_executable(irk-prefmap irk-prefmap.cpp)
target_link_libraries(irk-prefmap ${Boost_LIBRARIES} rax)

install(TARGETS irk irk-warc irk-part DESTINATION bin)
