include_guard(GLOBAL)

function(smrobot_stage_workbench_plugin)
    set(options)
    set(one_value_args TARGET MANIFEST_TEMPLATE PACKAGE_ID INSTALL_DESTINATION)
    cmake_parse_arguments(WORKBENCH "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT WORKBENCH_TARGET OR NOT TARGET ${WORKBENCH_TARGET})
        message(FATAL_ERROR "smrobot_stage_workbench_plugin requires an existing TARGET.")
    endif()
    if(NOT WORKBENCH_MANIFEST_TEMPLATE OR NOT EXISTS ${WORKBENCH_MANIFEST_TEMPLATE})
        message(FATAL_ERROR "Workbench manifest template does not exist.")
    endif()
    if(NOT WORKBENCH_PACKAGE_ID)
        message(FATAL_ERROR "Workbench PACKAGE_ID is required.")
    endif()

    file(READ ${WORKBENCH_MANIFEST_TEMPLATE} manifest_template_)
    foreach(manifest_placeholder_ IN ITEMS
            QT_MAJOR QT_MINOR POINTER_BITS)
        string(REPLACE "@${manifest_placeholder_}@" "0"
            manifest_template_ "${manifest_template_}")
    endforeach()
    string(JSON manifest_schema_ ERROR_VARIABLE manifest_error_ GET
        "${manifest_template_}" schema)
    string(JSON manifest_version_ ERROR_VARIABLE manifest_version_error_ GET
        "${manifest_template_}" version)
    if(manifest_error_ OR manifest_version_error_ OR
        NOT manifest_schema_ STREQUAL "smrobot.workbench-package" OR
        NOT manifest_version_ EQUAL 1)
        message(FATAL_ERROR
            "Invalid smrobot.workbench-package v1 manifest template: "
            "${WORKBENCH_MANIFEST_TEMPLATE}")
    endif()

    set(staging_root_
        ${CMAKE_CURRENT_BINARY_DIR}/workbench-staging/$<CONFIG>/workbenches/${WORKBENCH_PACKAGE_ID})
    set(staged_manifest_
        ${staging_root_}/${WORKBENCH_PACKAGE_ID}.workbench.json)
    add_custom_command(TARGET ${WORKBENCH_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${staging_root_}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${WORKBENCH_TARGET}>
            ${staging_root_}/$<TARGET_FILE_NAME:${WORKBENCH_TARGET}>
        COMMAND ${CMAKE_COMMAND}
            -DINPUT=${WORKBENCH_MANIFEST_TEMPLATE}
            -DBINARY=$<TARGET_FILE:${WORKBENCH_TARGET}>
            -DBINARY_FILE=$<TARGET_FILE_NAME:${WORKBENCH_TARGET}>
            -DOUTPUT=${staged_manifest_}
            -DBUILD_CONFIGURATION=$<CONFIG>
            -DQT_MAJOR=${Qt5Core_VERSION_MAJOR}
            -DQT_MINOR=${Qt5Core_VERSION_MINOR}
            -DPOINTER_BITS=$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,64,32>
            -DMSVC_TOOLSET=${MSVC_TOOLSET_VERSION}
            -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateSMRobotWorkbenchManifest.cmake
        VERBATIM
        COMMENT "Stage Workbench plugin ${WORKBENCH_PACKAGE_ID}"
    )

    set(${WORKBENCH_TARGET}_WORKBENCH_STAGING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}/workbench-staging/$<CONFIG>
        PARENT_SCOPE)
    set(${WORKBENCH_TARGET}_WORKBENCH_STAGED_MANIFEST
        ${staged_manifest_}
        PARENT_SCOPE)

    if(WORKBENCH_INSTALL_DESTINATION)
        install(FILES
            $<TARGET_FILE:${WORKBENCH_TARGET}>
            ${staged_manifest_}
            DESTINATION
                ${WORKBENCH_INSTALL_DESTINATION}/${WORKBENCH_PACKAGE_ID}
            CONFIGURATIONS Release
        )
    endif()
endfunction()
