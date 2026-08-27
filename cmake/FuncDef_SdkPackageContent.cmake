include_guard(GLOBAL)

include(CMakeParseArguments)

function(_smrobot_sdk_manifest_json_array out_var)
    set(_json "")
    foreach(_item IN LISTS ARGN)
        string(REPLACE "\\" "\\\\" _escaped "${_item}")
        string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
        if(_json)
            string(APPEND _json ",\n")
        endif()
        string(APPEND _json "    \"${_escaped}\"")
    endforeach()
    set(${out_var} "${_json}" PARENT_SCOPE)
endfunction()

function(smrobot_install_sdk_package_content)
    set(_one_value_args
        PACKAGE_NAME
        VERSION
        README_FILE
        DOCS_DIRECTORY
        EXAMPLES_DIRECTORY
        INSTALL_COMPONENT
    )
    set(_multi_value_args COMPONENTS)
    cmake_parse_arguments(SDK_CONTENT "" "${_one_value_args}" "${_multi_value_args}" ${ARGN})

    foreach(_required_arg
        PACKAGE_NAME
        VERSION
        README_FILE
        DOCS_DIRECTORY
        EXAMPLES_DIRECTORY
        INSTALL_COMPONENT
    )
        if(NOT SDK_CONTENT_${_required_arg})
            message(FATAL_ERROR
                "smrobot_install_sdk_package_content requires ${_required_arg}.")
        endif()
    endforeach()

    if(NOT SDK_CONTENT_COMPONENTS)
        message(FATAL_ERROR
            "smrobot_install_sdk_package_content requires at least one public component.")
    endif()

    foreach(_required_path
        "${SDK_CONTENT_README_FILE}"
        "${SDK_CONTENT_DOCS_DIRECTORY}"
        "${SDK_CONTENT_EXAMPLES_DIRECTORY}"
    )
        if(NOT EXISTS "${_required_path}")
            message(FATAL_ERROR "SDK package content path does not exist: ${_required_path}")
        endif()
    endforeach()

    if(WIN32)
        set(_runtime_directory "bin")
    else()
        set(_runtime_directory "lib")
    endif()

    _smrobot_sdk_manifest_json_array(_component_json ${SDK_CONTENT_COMPONENTS})
    set(_required_directories include lib docs examples)
    if(NOT _runtime_directory IN_LIST _required_directories)
        list(APPEND _required_directories "${_runtime_directory}")
    endif()
    _smrobot_sdk_manifest_json_array(
        _required_directory_json ${_required_directories})
    set(_manifest_file
        "${CMAKE_CURRENT_BINARY_DIR}/${SDK_CONTENT_PACKAGE_NAME}PackageManifest.json")
    file(WRITE "${_manifest_file}" "{
  \"schema\": \"SMRobotPackageManifest.v1\",
  \"formatVersion\": 1,
  \"name\": \"${SDK_CONTENT_PACKAGE_NAME}\",
  \"version\": \"${SDK_CONTENT_VERSION}\",
  \"artifactKind\": \"shared-library-sdk\",
  \"cmakePackage\": \"${SDK_CONTENT_PACKAGE_NAME}\",
  \"targetNamespace\": \"${SDK_CONTENT_PACKAGE_NAME}::\",
  \"runtimeDirectory\": \"${_runtime_directory}\",
  \"requiredDirectories\": [
${_required_directory_json}
  ],
  \"publicComponents\": [
${_component_json}
  ],
  \"excludedContent\": [
    \"private headers\",
    \"implementation sources\",
    \"internal tests\",
    \"credentials and private keys\"
  ]
}
")

    install(FILES
        "${SDK_CONTENT_README_FILE}"
        DESTINATION "${SDK_CONTENT_PACKAGE_NAME}"
        COMPONENT "${SDK_CONTENT_INSTALL_COMPONENT}"
    )
    install(FILES
        "${_manifest_file}"
        DESTINATION "${SDK_CONTENT_PACKAGE_NAME}"
        RENAME manifest.json
        COMPONENT "${SDK_CONTENT_INSTALL_COMPONENT}"
    )
    install(DIRECTORY
        "${SDK_CONTENT_DOCS_DIRECTORY}/"
        DESTINATION "${SDK_CONTENT_PACKAGE_NAME}/docs"
        COMPONENT "${SDK_CONTENT_INSTALL_COMPONENT}"
        PATTERN ".gitkeep" EXCLUDE
    )
    install(DIRECTORY
        "${SDK_CONTENT_EXAMPLES_DIRECTORY}/"
        DESTINATION "${SDK_CONTENT_PACKAGE_NAME}/examples"
        COMPONENT "${SDK_CONTENT_INSTALL_COMPONENT}"
        PATTERN ".gitkeep" EXCLUDE
    )
endfunction()
