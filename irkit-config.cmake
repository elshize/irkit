get_filename_component(SELF_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

find_dependency(Boost 1.55
    COMPONENTS iostreams program_options serialization filesystem system
    REQUIRED)

if(NOT TARGET irkit)
    include(${SELF_DIR}/irkit.cmake)
endif()
