cmake_minimum_required(VERSION 3.20)

set(SMROBOT_SDK_PROFILE "core_platform_thin_internal" CACHE STRING
    "SDK install profile to apply.")
set(SMROBOT_SDK_PROFILE_BUILD_DIR "" CACHE PATH
    "Build directory passed to cmake --install.")
set(SMROBOT_SDK_PROFILE_PREFIX "" CACHE PATH
    "SDK install prefix.")
set(SMROBOT_SDK_PROFILE_CONFIG "Release" CACHE STRING
    "Build configuration to install.")
set(SMROBOT_SDK_PROFILE_CONFIGS "" CACHE STRING
    "Semicolon-separated build configurations to install. When empty, SMROBOT_SDK_PROFILE_CONFIG is used.")
option(SMROBOT_SDK_PROFILE_WRITE_MANIFEST
    "Write an SDK manifest after installing the selected profile."
    ON
)
set(SMROBOT_SDK_PROFILE_MANIFEST_FILE "" CACHE FILEPATH
    "SDK manifest file path. Defaults to <prefix>/SMRobotSDKManifest.json.")

if(NOT SMROBOT_SDK_PROFILE_BUILD_DIR)
    message(FATAL_ERROR "SMROBOT_SDK_PROFILE_BUILD_DIR is required.")
endif()

if(NOT SMROBOT_SDK_PROFILE_PREFIX)
    message(FATAL_ERROR "SMROBOT_SDK_PROFILE_PREFIX is required.")
endif()

get_filename_component(_smrobot_profile_build_dir "${SMROBOT_SDK_PROFILE_BUILD_DIR}" ABSOLUTE)
get_filename_component(_smrobot_profile_prefix "${SMROBOT_SDK_PROFILE_PREFIX}" ABSOLUTE)

if(SMROBOT_SDK_PROFILE_CONFIGS)
    set(_smrobot_profile_configs ${SMROBOT_SDK_PROFILE_CONFIGS})
else()
    set(_smrobot_profile_configs ${SMROBOT_SDK_PROFILE_CONFIG})
endif()

if(NOT SMROBOT_SDK_PROFILE_MANIFEST_FILE)
    set(SMROBOT_SDK_PROFILE_MANIFEST_FILE "${_smrobot_profile_prefix}/SMRobotSDKManifest.json")
endif()

function(_smrobot_json_array OUT_VAR)
    set(_smrobot_json "")
    foreach(_smrobot_item IN LISTS ARGN)
        string(REPLACE "\\" "\\\\" _smrobot_escaped "${_smrobot_item}")
        string(REPLACE "\"" "\\\"" _smrobot_escaped "${_smrobot_escaped}")
        if(_smrobot_json)
            string(APPEND _smrobot_json ",\n")
        endif()
        string(APPEND _smrobot_json "    \"${_smrobot_escaped}\"")
    endforeach()
    set(${OUT_VAR} "${_smrobot_json}" PARENT_SCOPE)
endfunction()

function(_smrobot_cache_value OUT_VAR CACHE_FILE KEY)
    set(_smrobot_value "")
    if(EXISTS "${CACHE_FILE}")
        file(STRINGS "${CACHE_FILE}" _smrobot_cache_lines REGEX "^${KEY}:.*=")
        if(_smrobot_cache_lines)
            list(GET _smrobot_cache_lines 0 _smrobot_cache_line)
            string(REGEX REPLACE "^[^=]*=" "" _smrobot_value "${_smrobot_cache_line}")
        endif()
    endif()
    set(${OUT_VAR} "${_smrobot_value}" PARENT_SCOPE)
endfunction()

function(_smrobot_copy_coacd_tbb_runtime PROFILE_BUILD_DIR PROFILE_PREFIX)
    _smrobot_cache_value(_smrobot_thirdparty_root "${PROFILE_BUILD_DIR}/CMakeCache.txt" "SMROBOT_THIRDPARTY_ROOT")
    if(NOT _smrobot_thirdparty_root)
        return()
    endif()

    set(_smrobot_coacd_bin "${_smrobot_thirdparty_root}/Windows/CoACD1.0.11x64/bin")
    set(_smrobot_coacd_tbb_release "${_smrobot_coacd_bin}/tbb12.dll")
    set(_smrobot_coacd_tbb_debug "${_smrobot_coacd_bin}/tbb12_debug.dll")

    file(GLOB _smrobot_sdk_package_bin_dirs LIST_DIRECTORIES TRUE
        "${PROFILE_PREFIX}/*/bin"
    )

    foreach(_smrobot_sdk_package_bin_dir IN LISTS _smrobot_sdk_package_bin_dirs)
        set(_smrobot_copied_tbb_files)
        if(EXISTS "${_smrobot_sdk_package_bin_dir}/lib_coacd.dll"
           AND EXISTS "${_smrobot_coacd_tbb_release}")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_smrobot_coacd_tbb_release}"
                    "${_smrobot_sdk_package_bin_dir}"
                RESULT_VARIABLE _smrobot_copy_tbb_release_result
            )
            if(NOT _smrobot_copy_tbb_release_result EQUAL 0)
                message(FATAL_ERROR "Failed to copy CoACD release TBB runtime to ${_smrobot_sdk_package_bin_dir}.")
            endif()
            list(APPEND _smrobot_copied_tbb_files "tbb12.dll")
        endif()

        if(EXISTS "${_smrobot_sdk_package_bin_dir}/lib_coacd-D.dll"
           AND EXISTS "${_smrobot_coacd_tbb_debug}")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_smrobot_coacd_tbb_debug}"
                    "${_smrobot_sdk_package_bin_dir}"
                RESULT_VARIABLE _smrobot_copy_tbb_debug_result
            )
            if(NOT _smrobot_copy_tbb_debug_result EQUAL 0)
                message(FATAL_ERROR "Failed to copy CoACD debug TBB runtime to ${_smrobot_sdk_package_bin_dir}.")
            endif()
            list(APPEND _smrobot_copied_tbb_files "tbb12_debug.dll")
        endif()

        if(_smrobot_copied_tbb_files)
            list(JOIN _smrobot_copied_tbb_files ", " _smrobot_copied_tbb_text)
            message(STATUS "Installed CoACD TBB runtime into ${_smrobot_sdk_package_bin_dir}: ${_smrobot_copied_tbb_text}")
        endif()
    endforeach()
endfunction()

function(_smrobot_git_tree_ref OUT_VAR SOURCE_DIR RELATIVE_PATH)
    set(_smrobot_ref "")
    if(SOURCE_DIR AND EXISTS "${SOURCE_DIR}/.git")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env GIT_OPTIONAL_LOCKS=0
                git -C "${SOURCE_DIR}" rev-parse "HEAD:${RELATIVE_PATH}"
            RESULT_VARIABLE _smrobot_git_result
            OUTPUT_VARIABLE _smrobot_git_output
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_smrobot_git_result EQUAL 0)
            set(_smrobot_ref "${_smrobot_git_output}")
        endif()
    endif()
    set(${OUT_VAR} "${_smrobot_ref}" PARENT_SCOPE)
endfunction()

function(_smrobot_git_worktree_head OUT_VAR WORKTREE_DIR)
    set(_smrobot_ref "")
    if(WORKTREE_DIR AND EXISTS "${WORKTREE_DIR}/.git")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env GIT_OPTIONAL_LOCKS=0
                git -C "${WORKTREE_DIR}" rev-parse HEAD
            RESULT_VARIABLE _smrobot_git_result
            OUTPUT_VARIABLE _smrobot_git_output
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_smrobot_git_result EQUAL 0)
            set(_smrobot_ref "${_smrobot_git_output}")
        endif()
    endif()
    set(${OUT_VAR} "${_smrobot_ref}" PARENT_SCOPE)
endfunction()

set(_smrobot_profile_components_core_platform_dll_e1
    smrobot_sdk_bootstrap
    smrobot_sdk_thirdparty_header_only
    smrobot_sdk_verification
    smrobot_sdk_robotio_thirdparty
    smrobot_sdk_Common_package
    smrobot_sdk_Common_CustomLog
    smrobot_sdk_Common_GLRuntime
    smrobot_sdk_Common_Utility
    smrobot_sdk_Common_LicenseVerification
    smrobot_sdk_SMRobotCore_package
    smrobot_sdk_SMRobotCore_RobotCoreAuthorization
    smrobot_sdk_SMRobotCore_RobotCore
    smrobot_sdk_SMRobotCore_Kinematics
    smrobot_sdk_SMRobotCore_RobotTrajectoryCore
    smrobot_sdk_SMRobotCore_RobotIO
    smrobot_sdk_SMRobotPlatform_package
    smrobot_sdk_SMRobotPlatform_RobotPlatformAuthorization
    smrobot_sdk_SMRobotPlatform_ProjectSimulationSDK
)

set(_smrobot_profile_components_core_platform_dll_e2
    ${_smrobot_profile_components_core_platform_dll_e1}
    smrobot_sdk_SMRobotCore_RobotRuntime
    smrobot_sdk_SMRobotCore_RobotInstance
)

set(_smrobot_profile_components_core_platform_dll_e3
    ${_smrobot_profile_components_core_platform_dll_e2}
    smrobot_sdk_SMRobotCore_Collision
)

set(_smrobot_profile_components_core_platform_dll_e4
    ${_smrobot_profile_components_core_platform_dll_e3}
    smrobot_sdk_SMRobotPlatform_AssetCore
)

set(_smrobot_profile_components_core_platform_dll_e5
    ${_smrobot_profile_components_core_platform_dll_e4}
    smrobot_sdk_SMRobotPlatform_CameraCore
    smrobot_sdk_SMRobotPlatform_SensorCore
)

set(_smrobot_profile_components_core_platform_dll_e6
    ${_smrobot_profile_components_core_platform_dll_e5}
    smrobot_sdk_SMRobotPlatform_SimulationProject
    smrobot_sdk_SMRobotPlatform_SimulationRuntime
    smrobot_sdk_SMRobotPlatform_SensorSimulation
)

set(_smrobot_profile_components_core_platform_dll_e7
    ${_smrobot_profile_components_core_platform_dll_e6}
    smrobot_sdk_SMRobotPlatform_RenderCore
    smrobot_sdk_SMRobotPlatform_RenderCoreShaderResources
    smrobot_sdk_SMRobotPlatform_SceneCore
    smrobot_sdk_SMRobotPlatform_RobotRenderBridge
    smrobot_sdk_SMRobotCore_RobotSDK
)

set(_smrobot_profile_components_core_platform_dll_e_final
    ${_smrobot_profile_components_core_platform_dll_e7}
    smrobot_sdk_SMRobotPlatform_VisualizationSDK
)

set(_smrobot_profile_components_core_platform_thin_internal
    smrobot_sdk_bootstrap
    smrobot_sdk_Common_package
    smrobot_sdk_Common_CustomLog
    smrobot_sdk_Common_GLRuntime
    smrobot_sdk_Common_Utility
    smrobot_sdk_Common_LicenseVerification
    smrobot_sdk_SMRobotCore_package
    smrobot_sdk_SMRobotCore_RobotCoreAuthorization
    smrobot_sdk_SMRobotCore_RobotCore
    smrobot_sdk_SMRobotCore_Kinematics
    smrobot_sdk_SMRobotCore_RobotTrajectoryCore
    smrobot_sdk_SMRobotCore_RobotIO
    smrobot_sdk_SMRobotCore_RobotRuntime
    smrobot_sdk_SMRobotCore_RobotInstance
    smrobot_sdk_SMRobotCore_Collision
    smrobot_sdk_SMRobotCore_RobotSDK
    smrobot_sdk_SMRobotPlatform_package
    smrobot_sdk_SMRobotPlatform_RobotPlatformAuthorization
    smrobot_sdk_SMRobotPlatform_ProjectSimulationSDK
    smrobot_sdk_SMRobotPlatform_AssetCore
    smrobot_sdk_SMRobotPlatform_CameraCore
    smrobot_sdk_SMRobotPlatform_SensorCore
    smrobot_sdk_SMRobotPlatform_SimulationProject
    smrobot_sdk_SMRobotPlatform_SimulationRuntime
    smrobot_sdk_SMRobotPlatform_SensorSimulation
    smrobot_sdk_SMRobotPlatform_RenderCore
    smrobot_sdk_SMRobotPlatform_RenderCoreShaderResources
    smrobot_sdk_SMRobotPlatform_SceneCore
    smrobot_sdk_SMRobotPlatform_RobotRenderBridge
    smrobot_sdk_SMRobotPlatform_VisualizationSDK
)

set(_smrobot_profile_components_core_platform_standalone
    ${_smrobot_profile_components_core_platform_dll_e_final}
)

set(_smrobot_profile_var "_smrobot_profile_components_${SMROBOT_SDK_PROFILE}")
if(NOT DEFINED ${_smrobot_profile_var})
    message(FATAL_ERROR "Unsupported SMROBOT_SDK_PROFILE: ${SMROBOT_SDK_PROFILE}")
endif()

message(STATUS "Installing SMRobot SDK profile: ${SMROBOT_SDK_PROFILE}")
message(STATUS "Build dir: ${_smrobot_profile_build_dir}")
message(STATUS "Prefix: ${_smrobot_profile_prefix}")
message(STATUS "Configs: ${_smrobot_profile_configs}")

foreach(_smrobot_config IN LISTS _smrobot_profile_configs)
    foreach(_smrobot_component IN LISTS ${_smrobot_profile_var})
        message(STATUS "Installing component: ${_smrobot_component} (${_smrobot_config})")
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                --install "${_smrobot_profile_build_dir}"
                --config "${_smrobot_config}"
                --prefix "${_smrobot_profile_prefix}"
                --component "${_smrobot_component}"
            RESULT_VARIABLE _smrobot_install_result
        )
        if(NOT _smrobot_install_result EQUAL 0)
            message(FATAL_ERROR
                "Failed to install component ${_smrobot_component} for config ${_smrobot_config}; "
                "cmake --install returned ${_smrobot_install_result}.")
        endif()
    endforeach()
endforeach()

_smrobot_copy_coacd_tbb_runtime("${_smrobot_profile_build_dir}" "${_smrobot_profile_prefix}")

if(SMROBOT_SDK_PROFILE_WRITE_MANIFEST)
    file(GLOB_RECURSE _smrobot_manifest_targets
        RELATIVE "${_smrobot_profile_prefix}"
        "${_smrobot_profile_prefix}/*/lib/cmake/*/*Targets.cmake"
    )
    file(GLOB_RECURSE _smrobot_manifest_dlls
        RELATIVE "${_smrobot_profile_prefix}"
        "${_smrobot_profile_prefix}/*/bin/*.dll"
    )
    file(GLOB_RECURSE _smrobot_manifest_libs
        RELATIVE "${_smrobot_profile_prefix}"
        "${_smrobot_profile_prefix}/*/lib/*.lib"
    )
    file(GLOB_RECURSE _smrobot_manifest_pdbs
        RELATIVE "${_smrobot_profile_prefix}"
        "${_smrobot_profile_prefix}/*/bin/*.pdb"
    )

    list(SORT _smrobot_manifest_targets)
    list(SORT _smrobot_manifest_dlls)
    list(SORT _smrobot_manifest_libs)
    list(SORT _smrobot_manifest_pdbs)

    _smrobot_cache_value(_smrobot_manifest_generator "${_smrobot_profile_build_dir}/CMakeCache.txt" "CMAKE_GENERATOR")
    _smrobot_cache_value(_smrobot_manifest_platform "${_smrobot_profile_build_dir}/CMakeCache.txt" "CMAKE_GENERATOR_PLATFORM")
    _smrobot_cache_value(_smrobot_manifest_toolset "${_smrobot_profile_build_dir}/CMakeCache.txt" "CMAKE_GENERATOR_TOOLSET")
    _smrobot_cache_value(_smrobot_manifest_instance "${_smrobot_profile_build_dir}/CMakeCache.txt" "CMAKE_GENERATOR_INSTANCE")
    _smrobot_cache_value(_smrobot_manifest_cxx_compiler "${_smrobot_profile_build_dir}/CMakeCache.txt" "CMAKE_CXX_COMPILER")
    _smrobot_cache_value(_smrobot_manifest_source_dir "${_smrobot_profile_build_dir}/CMakeCache.txt" "CMAKE_HOME_DIRECTORY")
    _smrobot_cache_value(_smrobot_manifest_thirdparty_root "${_smrobot_profile_build_dir}/CMakeCache.txt" "SMROBOT_THIRDPARTY_ROOT")
    _smrobot_cache_value(_smrobot_manifest_data_root "${_smrobot_profile_build_dir}/CMakeCache.txt" "SMROBOT_DATA_ROOT")
    if(NOT _smrobot_manifest_cxx_compiler AND _smrobot_manifest_generator MATCHES "Visual Studio")
        set(_smrobot_manifest_cxx_compiler "${_smrobot_manifest_generator} toolchain")
    endif()
    string(TIMESTAMP _smrobot_manifest_timestamp "%Y-%m-%dT%H:%M:%SZ" UTC)

    if(SMROBOT_SDK_PROFILE STREQUAL "core_platform_thin_internal")
        set(_smrobot_manifest_dependency_mode "external")
    else()
        set(_smrobot_manifest_dependency_mode "embedded")
    endif()
    _smrobot_git_tree_ref(_smrobot_manifest_thirdparty_git_ref "${_smrobot_manifest_source_dir}" "thirdparty")
    _smrobot_git_tree_ref(_smrobot_manifest_data_git_ref "${_smrobot_manifest_source_dir}" "data")
    if(NOT _smrobot_manifest_data_git_ref)
        _smrobot_git_worktree_head(_smrobot_manifest_data_git_ref "${_smrobot_manifest_data_root}")
    endif()

    _smrobot_json_array(_smrobot_manifest_config_json ${_smrobot_profile_configs})
    _smrobot_json_array(_smrobot_manifest_component_json ${${_smrobot_profile_var}})
    _smrobot_json_array(_smrobot_manifest_target_json ${_smrobot_manifest_targets})
    _smrobot_json_array(_smrobot_manifest_dll_json ${_smrobot_manifest_dlls})
    _smrobot_json_array(_smrobot_manifest_lib_json ${_smrobot_manifest_libs})
    _smrobot_json_array(_smrobot_manifest_pdb_json ${_smrobot_manifest_pdbs})

    string(REPLACE "\\" "\\\\" _smrobot_manifest_build_dir_json "${_smrobot_profile_build_dir}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_prefix_json "${_smrobot_profile_prefix}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_generator_json "${_smrobot_manifest_generator}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_platform_json "${_smrobot_manifest_platform}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_toolset_json "${_smrobot_manifest_toolset}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_instance_json "${_smrobot_manifest_instance}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_cxx_compiler_json "${_smrobot_manifest_cxx_compiler}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_source_dir_json "${_smrobot_manifest_source_dir}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_thirdparty_root_json "${_smrobot_manifest_thirdparty_root}")
    string(REPLACE "\\" "\\\\" _smrobot_manifest_data_root_json "${_smrobot_manifest_data_root}")

    file(WRITE "${SMROBOT_SDK_PROFILE_MANIFEST_FILE}" "{
  \"schema\": \"SMRobotSDKManifest.v1\",
  \"generated_utc\": \"${_smrobot_manifest_timestamp}\",
  \"profile\": \"${SMROBOT_SDK_PROFILE}\",
  \"build_dir\": \"${_smrobot_manifest_build_dir_json}\",
  \"prefix\": \"${_smrobot_manifest_prefix_json}\",
  \"generator\": \"${_smrobot_manifest_generator_json}\",
  \"generator_platform\": \"${_smrobot_manifest_platform_json}\",
  \"generator_toolset\": \"${_smrobot_manifest_toolset_json}\",
  \"generator_instance\": \"${_smrobot_manifest_instance_json}\",
  \"cxx_compiler\": \"${_smrobot_manifest_cxx_compiler_json}\",
  \"source_dir\": \"${_smrobot_manifest_source_dir_json}\",
  \"configs\": [
${_smrobot_manifest_config_json}
  ],
  \"dependency_policy\": {
    \"mode\": \"${_smrobot_manifest_dependency_mode}\",
    \"thirdparty\": {
      \"path_hint\": \"thirdparty\",
      \"build_root\": \"${_smrobot_manifest_thirdparty_root_json}\",
      \"git_ref\": \"${_smrobot_manifest_thirdparty_git_ref}\"
    },
    \"data\": {
      \"path_hint\": \"data\",
      \"build_root\": \"${_smrobot_manifest_data_root_json}\",
      \"git_ref\": \"${_smrobot_manifest_data_git_ref}\"
    }
  },
  \"install_components\": [
${_smrobot_manifest_component_json}
  ],
  \"installed_target_files\": [
${_smrobot_manifest_target_json}
  ],
  \"dll_files\": [
${_smrobot_manifest_dll_json}
  ],
  \"import_library_files\": [
${_smrobot_manifest_lib_json}
  ],
  \"pdb_files\": [
${_smrobot_manifest_pdb_json}
  ],
  \"pdb_policy\": {
    \"install_pdb\": \"${SMROBOT_SDK_INSTALL_PDB}\",
    \"configurations\": \"${SMROBOT_SDK_PDB_CONFIGURATIONS}\"
  },
  \"runtime_dependency_policy\": \"Each shared component installs TARGET_RUNTIME_DLLS into the owning package bin directory when CMake can resolve them.\"
}
")
    message(STATUS "SMRobot SDK manifest written: ${SMROBOT_SDK_PROFILE_MANIFEST_FILE}")
endif()

message(STATUS "SMRobot SDK profile installed: ${SMROBOT_SDK_PROFILE}")
