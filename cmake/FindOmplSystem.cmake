# Adapt distro OMPL variable-only packages without modifying their generated files.
find_package(ompl CONFIG REQUIRED)
if(TARGET ompl::ompl)
    return()
endif()
if(TARGET ompl)
    add_library(ompl::ompl ALIAS ompl)
    return()
endif()
# Ubuntu 26.04 OMPL 1.6 also exports optional build-machine HDF5/MPI paths.
# Keep existing installed includes, and require the core public API explicitly.
set(_rs_ompl_includes)
foreach(_dir IN LISTS OMPL_INCLUDE_DIRS)
    if(IS_DIRECTORY "${_dir}")
        list(APPEND _rs_ompl_includes "${_dir}")
    else()
        message(STATUS "[OMPL system adapter] Ignoring absent optional include: ${_dir}")
    endif()
endforeach()
if(NOT EXISTS "${OMPL_INCLUDE_DIR}/ompl/base/SpaceInformation.h")
    message(FATAL_ERROR "OMPL core development headers are absent: ${OMPL_INCLUDE_DIR}")
endif()
add_library(ompl::ompl INTERFACE IMPORTED)
set_target_properties(ompl::ompl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_rs_ompl_includes}"
    INTERFACE_LINK_LIBRARIES "${OMPL_LIBRARIES}")
message(STATUS "System OMPL ${OMPL_VERSION}: ${OMPL_LIBRARIES}")
