# a9380dd lists VirtualBaffle sources which were not published in that commit.
# Keep the replacement in the parent so a fresh recursive clone is buildable.
# Never overwrite an implementation subsequently published by the submodule.
set(_smrobot_freeform "${CMAKE_CURRENT_LIST_DIR}/../SMRobotSpray/SprayFreeformTrajPlan")
if(EXISTS "${_smrobot_freeform}/CMakeLists.txt")
    foreach(_entry IN ITEMS "src/VirtualBaffle.cpp" "include/SlicingTrajPlan/VirtualBaffle.h")
        get_filename_component(_name "${_entry}" NAME)
        set(_compat_managed FALSE)
        if(EXISTS "${_smrobot_freeform}/SlicingTrajPlan/${_entry}")
            file(STRINGS "${_smrobot_freeform}/SlicingTrajPlan/${_entry}" _compat_marker LIMIT_COUNT 1)
            if(_compat_marker STREQUAL "// SMROBOT_PARENT_BAFFLE_COMPAT")
                set(_compat_managed TRUE)
            endif()
        endif()
        if(_compat_managed OR NOT EXISTS "${_smrobot_freeform}/SlicingTrajPlan/${_entry}")
            configure_file("${CMAKE_CURRENT_LIST_DIR}/compat/${_name}"
                "${_smrobot_freeform}/SlicingTrajPlan/${_entry}" COPYONLY)
        endif()
    endforeach()
endif()
unset(_entry)
unset(_name)
unset(_smrobot_freeform)
unset(_compat_managed)
unset(_compat_marker)

# The pinned freeform submodule bundles GLFW with a policy removed in CMake 4.
# Keep the compatibility fix in the parent repository so it is reproducible
# without requiring write access to the upstream algorithm repository.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(_smrobot_glfw_cmake
        "${CMAKE_CURRENT_LIST_DIR}/../SMRobotSpray/SprayFreeformTrajPlan/UVTrajPlan/optcut/ext/libigl/external/glfw/CMakeLists.txt")
    if(EXISTS "${_smrobot_glfw_cmake}")
        file(READ "${_smrobot_glfw_cmake}" _smrobot_glfw_original)
        string(REPLACE "cmake_policy(SET CMP0042 OLD)"
            "cmake_policy(SET CMP0042 NEW)"
            _smrobot_glfw_patched "${_smrobot_glfw_original}")
        if(NOT _smrobot_glfw_patched STREQUAL _smrobot_glfw_original)
            file(WRITE "${_smrobot_glfw_cmake}" "${_smrobot_glfw_patched}")
            message(STATUS "Applied bundled GLFW compatibility fix for CMake 4")
        endif()
        unset(_smrobot_glfw_original)
        unset(_smrobot_glfw_patched)
    endif()
    unset(_smrobot_glfw_cmake)
endif()
