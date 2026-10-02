#############################################################
# Find prebuilt packages in dependency order.
#############################################################

function(rs_find_requested_prebuilt_package PACKAGE_NAME)
    if(NOT UsingPrebuilt_${PACKAGE_NAME})
        message(STATUS "Skip prebuilt package ${PACKAGE_NAME}; UsingPrebuilt_${PACKAGE_NAME}=OFF")
        return()
    endif()

    if(DEFINED ${PACKAGE_NAME}_DepLibs)
        set(_prebuilt_components_to_find ${${PACKAGE_NAME}_DepLibs})
    elseif(DEFINED ${PACKAGE_NAME}_ExportComponents)
        set(_prebuilt_components_to_find ${${PACKAGE_NAME}_ExportComponents})
    else()
        set(_prebuilt_components_to_find ${${PACKAGE_NAME}_PrebuiltComponents})
    endif()

    if(PACKAGE_NAME STREQUAL "SMRobotPlatform" AND "RenderCore" IN_LIST _prebuilt_components_to_find)
        list(APPEND _prebuilt_components_to_find RenderCoreShaderResources)
        list(REMOVE_DUPLICATES _prebuilt_components_to_find)
    endif()

    message(STATUS "Finding Prebuilt Package ${PACKAGE_NAME} with components ${_prebuilt_components_to_find}")
    find_prebuilt_package(
        PACKAGE_NAME ${PACKAGE_NAME}
        COMPONENTS ${_prebuilt_components_to_find}
    )
    # Shader resources are dynamically loaded, not a public link dependency.
    if(PACKAGE_NAME STREQUAL "SMRobotPlatform"
       AND TARGET SMRobotPlatform::RenderCore
       AND TARGET SMRobotPlatform::RenderCoreShaderResources)
        foreach(_config DEBUG RELEASE)
            get_target_property(_shader_runtime SMRobotPlatform::RenderCoreShaderResources
                IMPORTED_LOCATION_${_config})
            if(_shader_runtime)
                set_property(TARGET SMRobotPlatform::RenderCore APPEND PROPERTY
                    SMROBOT_PRIVATE_RUNTIME_FILES_${_config} "${_shader_runtime}")
            endif()
        endforeach()
    endif()
endfunction()
