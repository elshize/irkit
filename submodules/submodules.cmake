execute_process(COMMAND git submodule update --init
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
                OUTPUT_QUIET)

# rax: radix tree
add_library(rax STATIC submodules/rax/rax.c)
target_include_directories(rax INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/submodules/rax)

add_library(mongoose STATIC submodules/mongoose/mongoose.c)
target_include_directories(mongoose INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/submodules/mongoose)

add_library(ranges INTERFACE)
target_include_directories(ranges INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/submodules/range-v3/include)

set(JSON_BuildTests OFF CACHE INTERNAL "" FORCE)
add_subdirectory(submodules/json EXCLUDE_FROM_ALL)
add_subdirectory(submodules/debug_assert EXCLUDE_FROM_ALL)
add_subdirectory(submodules/CLI11 EXCLUDE_FROM_ALL)

# TODO: How to disable the test when adding it as a subdirectory?
#add_subdirectory(submodules/type_safe EXCLUDE_FROM_ALL)
add_library(type_safe INTERFACE)
target_include_directories(type_safe INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/submodules/type_safe/include)

# TODO: Work around it: why is it failing when I simply add the subdirectory?
#add_subdirectory(submodules/GSL EXCLUDE_FROM_ALL)
set(GSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/GSL/include)
include_directories(${GSL_INCLUDE_DIR})

# Google Test
add_subdirectory(submodules/googletest EXCLUDE_FROM_ALL)

# Gumbo HTML5 parser
#set(GUMBO_DIR "gumbo-parser")
#execute_process(COMMAND "./autogen.sh" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#execute_process(COMMAND "./configure" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#add_custom_command(OUTPUT libgumbo.la
#                   COMMAND "make" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#add_custom_target(gumbo DEPENDS libgumbo.la)
