execute_process(COMMAND git submodule update --init
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
                OUTPUT_QUIET)

# rax: radix tree
add_library(rax STATIC submodules/rax/rax.c)
set(rax_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/rax)

# Guideline Support Library
set(GSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/GSL/include)

# Mongoose
add_library(mongoose STATIC submodules/mongoose/mongoose.c)
set(mongoose_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/mongoose)

# Experimental Range library; mainly used for Concepts
set(range_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/range-v3/include)

# JSON
set(json_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/json/include)

# debug_assert
set(debug_assert_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/debug_assert)

# debug_assert
set(type_safe_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/type_safe)

# Gumbo HTML5 parser
#set(GUMBO_DIR "gumbo-parser")
#execute_process(COMMAND "./autogen.sh" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#execute_process(COMMAND "./configure" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#add_custom_command(OUTPUT libgumbo.la
#                   COMMAND "make" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#add_custom_target(gumbo DEPENDS libgumbo.la)
