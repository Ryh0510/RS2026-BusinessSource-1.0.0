cmake_minimum_required(VERSION 3.20)

set(SMROBOT_SDK_LAYOUT_PREFIX "" CACHE PATH "Installed SMRobot SDK prefix to validate.")

if(NOT SMROBOT_SDK_LAYOUT_PREFIX)
    message(FATAL_ERROR "SMROBOT_SDK_LAYOUT_PREFIX is required.")
endif()

get_filename_component(_sdk_prefix "${SMROBOT_SDK_LAYOUT_PREFIX}" ABSOLUTE)

function(_smrobot_require_path path description)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing ${description}: ${path}")
    endif()
endfunction()

function(_smrobot_validate_package package_name)
    set(_package_root "${_sdk_prefix}/${package_name}")
    set(_manifest "${_package_root}/manifest.json")

    foreach(_required_path
        "${_package_root}/README.md"
        "${_manifest}"
        "${_package_root}/include"
        "${_package_root}/lib"
        "${_package_root}/docs"
        "${_package_root}/examples"
        "${_package_root}/lib/cmake/${package_name}/${package_name}Config.cmake"
        "${_package_root}/lib/cmake/${package_name}/${package_name}ConfigVersion.cmake"
    )
        _smrobot_require_path("${_required_path}" "${package_name} SDK package content")
    endforeach()

    file(READ "${_manifest}" _manifest_content)
    string(JSON _schema ERROR_VARIABLE _schema_error GET "${_manifest_content}" schema)
    string(JSON _name ERROR_VARIABLE _name_error GET "${_manifest_content}" name)
    string(JSON _version ERROR_VARIABLE _version_error GET "${_manifest_content}" version)
    string(JSON _runtime_directory ERROR_VARIABLE _runtime_error
        GET "${_manifest_content}" runtimeDirectory)
    string(JSON _component_count ERROR_VARIABLE _component_error
        LENGTH "${_manifest_content}" publicComponents)
    string(JSON _required_directory_count ERROR_VARIABLE _required_directory_error
        LENGTH "${_manifest_content}" requiredDirectories)

    if(_schema_error OR NOT _schema STREQUAL "SMRobotPackageManifest.v1")
        message(FATAL_ERROR "Invalid package manifest schema: ${_manifest}")
    endif()
    if(_name_error OR NOT _name STREQUAL package_name)
        message(FATAL_ERROR "Package manifest name mismatch: ${_manifest}")
    endif()
    if(_version_error OR NOT _version STREQUAL _sdk_version)
        message(FATAL_ERROR "Package manifest version mismatch: ${_manifest}")
    endif()
    if(_runtime_error OR NOT _runtime_directory MATCHES "^(bin|lib)$")
        message(FATAL_ERROR "Invalid runtimeDirectory in ${_manifest}")
    endif()
    if(_component_error OR _component_count LESS 1)
        message(FATAL_ERROR "No public components declared in ${_manifest}")
    endif()
    if(_required_directory_error OR _required_directory_count LESS 4)
        message(FATAL_ERROR "Invalid requiredDirectories in ${_manifest}")
    endif()

    math(EXPR _required_directory_last "${_required_directory_count} - 1")
    foreach(_required_directory_index RANGE 0 ${_required_directory_last})
        string(JSON _required_directory ERROR_VARIABLE _required_directory_item_error
            GET "${_manifest_content}" requiredDirectories ${_required_directory_index})
        if(_required_directory_item_error
           OR NOT _required_directory MATCHES "^[A-Za-z0-9._-]+$")
            message(FATAL_ERROR "Invalid required directory in ${_manifest}")
        endif()
        _smrobot_require_path(
            "${_package_root}/${_required_directory}"
            "${package_name} manifest-required directory")
    endforeach()

    _smrobot_require_path(
        "${_package_root}/${_runtime_directory}"
        "${package_name} runtime directory")

    file(GLOB_RECURSE _headers "${_package_root}/include/*.h" "${_package_root}/include/*.hpp")
    file(GLOB_RECURSE _target_files "${_package_root}/lib/cmake/*/*Targets.cmake")
    file(GLOB_RECURSE _docs "${_package_root}/docs/*.md")
    file(GLOB_RECURSE _example_projects "${_package_root}/examples/CMakeLists.txt")
    file(GLOB_RECURSE _example_sources "${_package_root}/examples/*.cpp")

    if(NOT _headers)
        message(FATAL_ERROR "${package_name} contains no public headers.")
    endif()
    if(NOT _target_files)
        message(FATAL_ERROR "${package_name} contains no installed CMake targets.")
    endif()
    if(NOT _docs)
        message(FATAL_ERROR "${package_name} contains no Markdown documentation.")
    endif()
    if(NOT _example_projects OR NOT _example_sources)
        message(FATAL_ERROR "${package_name} contains no independently buildable example.")
    endif()

    if(_runtime_directory STREQUAL "bin")
        file(GLOB _runtime_libraries "${_package_root}/bin/*.dll")
    else()
        file(GLOB _runtime_libraries
            "${_package_root}/lib/*.so"
            "${_package_root}/lib/*.so.*"
            "${_package_root}/lib/*.dylib")
    endif()
    if(NOT _runtime_libraries)
        message(FATAL_ERROR "${package_name} contains no shared runtime libraries.")
    endif()

    file(GLOB_RECURSE _package_files RELATIVE "${_package_root}" "${_package_root}/*")
    foreach(_relative_file IN LISTS _package_files)
        string(REPLACE "\\" "/" _normalized_file "${_relative_file}")
        if(_normalized_file MATCHES "(^|/)(src|test|tests|CMakeFiles|\\.git)(/|$)"
           OR _normalized_file MATCHES "\\.(obj|vcxproj|vcxproj\\.filters|sln)$"
           OR _normalized_file MATCHES "(^|/)(private[-_]?key|signing[-_]?key)(/|\\.|$)")
            message(FATAL_ERROR
                "Forbidden internal or sensitive path in ${package_name}: ${_normalized_file}")
        endif()
    endforeach()

    list(LENGTH _headers _header_count)
    list(LENGTH _example_projects _example_count)
    message(STATUS
        "${package_name} package layout passed: components=${_component_count}, "
        "headers=${_header_count}, examples=${_example_count}")
endfunction()

set(_sdk_manifest "${_sdk_prefix}/SMRobotSDKManifest.json")
_smrobot_require_path("${_sdk_manifest}" "SDK root manifest")
file(READ "${_sdk_manifest}" _sdk_manifest_content)
string(JSON _sdk_version ERROR_VARIABLE _sdk_version_error
    GET "${_sdk_manifest_content}" version)
if(_sdk_version_error OR NOT _sdk_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+([-.+][0-9A-Za-z.-]+)?$")
    message(FATAL_ERROR "SDK manifest has an invalid version: ${_sdk_manifest}")
endif()

foreach(_private_path_member build_dir source_dir generator_instance)
    string(JSON _private_path_value ERROR_VARIABLE _private_path_error
        GET "${_sdk_manifest_content}" "${_private_path_member}")
    if(_private_path_error OR NOT _private_path_value STREQUAL "")
        message(FATAL_ERROR
            "SDK manifest must not publish ${_private_path_member}: ${_sdk_manifest}")
    endif()
endforeach()

string(JSON _manifest_prefix ERROR_VARIABLE _manifest_prefix_error
    GET "${_sdk_manifest_content}" prefix)
if(_manifest_prefix_error OR NOT _manifest_prefix STREQUAL ".")
    message(FATAL_ERROR "SDK manifest prefix must be relocatable: ${_sdk_manifest}")
endif()

foreach(_dependency_name thirdparty data)
    string(JSON _dependency_build_root ERROR_VARIABLE _dependency_root_error
        GET "${_sdk_manifest_content}"
        dependency_policy "${_dependency_name}" build_root)
    if(_dependency_root_error OR IS_ABSOLUTE "${_dependency_build_root}")
        message(FATAL_ERROR
            "SDK manifest contains an invalid ${_dependency_name} build_root: ${_sdk_manifest}")
    endif()
    if(_dependency_build_root
       AND NOT IS_DIRECTORY "${_sdk_prefix}/${_dependency_build_root}")
        message(FATAL_ERROR
            "SDK manifest dependency path does not exist: ${_dependency_build_root}")
    endif()
endforeach()

string(JSON _pdb_policy ERROR_VARIABLE _pdb_policy_error
    GET "${_sdk_manifest_content}" pdb_policy install_pdb)
file(GLOB_RECURSE _installed_pdbs "${_sdk_prefix}/*.pdb")
if(_pdb_policy_error)
    message(FATAL_ERROR "SDK manifest has no valid PDB policy: ${_sdk_manifest}")
endif()
if(_installed_pdbs AND NOT _pdb_policy MATCHES "^(ON|TRUE|1)$")
    message(FATAL_ERROR
        "SDK contains PDB files without an enabled manifest policy: ${_sdk_manifest}")
endif()

_smrobot_validate_package(SMRobotCore)
_smrobot_validate_package(SMRobotPlatform)

message(STATUS "SMRobot SDK package layout passed: ${_sdk_prefix}")
