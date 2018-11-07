find_package(CLI11 REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(irk irk.cpp)
target_link_libraries(irk CLI11::CLI11)

set(COMMON_LIBRARIES irkit CLI11::CLI11)

function(add_irk TOOL_NAME)
    foreach(arg IN LISTS ARGN)
        LIST(APPEND COMMON_LIBRARIES ${arg})
    endforeach()
    add_executable(irk-${TOOL_NAME} irk-${TOOL_NAME}.cpp)
    target_link_libraries(irk-${TOOL_NAME} PUBLIC ${COMMON_LIBRARIES})
    target_include_directories(irk-${TOOL_NAME} PRIVATE ./)
    target_compile_features(irk-${TOOL_NAME} PRIVATE ${IRKit_CXX_PRIVATE_FEATURES})
    target_compile_options(irk-${TOOL_NAME}
        PRIVATE -Wall -Wno-unused-local-typedefs -Wno-sign-compare -Wno-parentheses)
endfunction()

#add_irk(warc CONAN_PKG::zlib CONAN_PKG::gumbo-parser)
add_irk(part)
add_irk(lexicon)
add_irk(table)
add_irk(buildindex)
add_irk(score)
add_irk(scorestats)
add_irk(postings)
add_irk(query)
add_irk(lookup)
add_irk(disect)
add_irk(hits)
add_irk(docidmap)
add_irk(terminfo)
add_irk(shardmap)
add_irk(reorder)
add_irk(partidx)
add_irk(threshold)
add_irk(stem)

install(
    TARGETS
        irk
        #irk-warc
        irk-part
        irk-buildindex
        irk-score
        irk-scorestats
        irk-postings
        irk-query
        irk-lookup
        irk-disect
        irk-table
        irk-lexicon
        irk-hits
        irk-docidmap
        irk-terminfo
        irk-shardmap
        irk-reorder
        irk-partidx
        irk-threshold
        irk-stem
    DESTINATION bin)
