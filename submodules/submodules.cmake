execute_process(COMMAND git submodule update --init
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
                OUTPUT_QUIET)

# rax: radix tree
add_library(rax STATIC submodules/rax/rax.c)
target_include_directories(rax INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/submodules/rax)
if (IRKit_INSTALL_SUBMODULES)
    install(FILES submodules/rax/rax.h DESTINATION include)
    install(TARGETS rax DESTINATION lib)
endif()

add_library(mongoose STATIC submodules/mongoose/mongoose.c)
target_include_directories(mongoose INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/submodules/mongoose)
if (IRKit_INSTALL_SUBMODULES)
    install(FILES submodules/mongoose/mongoose.h DESTINATION include)
    install(TARGETS mongoose DESTINATION lib)
endif()

add_library(ranges INTERFACE)
set(RANGES_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/range-v3/include)
target_include_directories(ranges INTERFACE ${RANGES_INCLUDE_DIR})
if (IRKit_INSTALL_SUBMODULES)
    install(DIRECTORY ${RANGES_INCLUDE_DIR}/range DESTINATION include)
    install(DIRECTORY ${RANGES_INCLUDE_DIR}/meta DESTINATION include)
endif()

set(JSON_BuildTests OFF CACHE INTERNAL "" FORCE)
add_subdirectory(submodules/json EXCLUDE_FROM_ALL)
add_subdirectory(submodules/debug_assert EXCLUDE_FROM_ALL)
add_subdirectory(submodules/CLI11 EXCLUDE_FROM_ALL)
if (IRKit_INSTALL_SUBMODULES)
    install(DIRECTORY submodules/json/include/nlohmann DESTINATION include)
    install(FILES submodules/debug_assert/debug_assert.hpp DESTINATION include)
endif()

# TODO: How to disable the test when adding it as a subdirectory?
#add_subdirectory(submodules/type_safe EXCLUDE_FROM_ALL)
add_library(type_safe INTERFACE)
target_include_directories(type_safe INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/submodules/type_safe/include)
if (IRKit_INSTALL_SUBMODULES)
    install(DIRECTORY submodules/type_safe/include/type_safe DESTINATION include)
endif()

# TODO: Work around it: why is it failing when I simply add the subdirectory?
#add_subdirectory(submodules/GSL EXCLUDE_FROM_ALL)
set(GSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/GSL/include)
include_directories(${GSL_INCLUDE_DIR})
if (IRKit_INSTALL_SUBMODULES)
    install(DIRECTORY ${GSL_INCLUDE_DIR}/gsl DESTINATION include)
endif()

# Google Test
add_subdirectory(submodules/googletest EXCLUDE_FROM_ALL)

# Gumbo HTML5 parser
#set(GUMBO_DIR "gumbo-parser")
#execute_process(COMMAND "./autogen.sh" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#execute_process(COMMAND "./configure" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#add_custom_command(OUTPUT libgumbo.la
#                   COMMAND "make" ${GUMBO_DIR} ${CMAKE_SOURCE_DIR})
#add_custom_target(gumbo DEPENDS libgumbo.la)
