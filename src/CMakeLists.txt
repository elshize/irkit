add_executable(irk irk.cpp)
target_link_libraries(irk CONAN_PKG::CLI11)

set(COMMON_LIBRARIES irkit CONAN_PKG::CLI11)

function(add_irk TOOL_NAME)
    foreach(arg IN LISTS ARGN)
        LIST(APPEND COMMON_LIBRARIES ${arg})
    endforeach()
    add_executable(irk-${TOOL_NAME} irk-${TOOL_NAME}.cpp)
    target_link_libraries(irk-${TOOL_NAME} PUBLIC ${COMMON_LIBRARIES})
    target_compile_features(irk-${TOOL_NAME} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
    target_compile_options(irk-${TOOL_NAME} PRIVATE -Wall -Wnunused-local-typedef)
endfunction()

add_irk(warc CONAN_PKG::zlib CONAN_PKG::gumbo-parser)
add_irk(part)
add_irk(lexicon)
add_irk(table)
add_irk(buildindex)
add_irk(score)
add_irk(postings)
add_irk(query)
add_irk(lookup)
add_irk(disect)
add_irk(hits)

install(
    TARGETS
        irk
        irk-warc
        irk-part
        irk-buildindex
        irk-score
        irk-postings
        irk-query
        irk-lookup
        irk-disect
        irk-table
        irk-lexicon
        irk-hits
    DESTINATION bin)
