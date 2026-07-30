cmake_minimum_required(VERSION 3.20)

set(SMROBOT_SDK_MATRIX_SDK_PREFIX "" CACHE PATH
    "Installed SMRobot SDK prefix to validate.")
set(SMROBOT_SDK_MATRIX_BUILD_ROOT "" CACHE PATH
    "Build root for generated consumer matrix builds.")
set(SMROBOT_SDK_MATRIX_CONFIGS "Release" CACHE STRING
    "Semicolon-separated configurations to build and test.")
set(SMROBOT_SDK_MATRIX_GENERATOR "" CACHE STRING
    "Optional CMake generator for package example matrix builds.")
set(SMROBOT_SDK_MATRIX_GENERATOR_PLATFORM "" CACHE STRING
    "Optional CMake generator platform for package example matrix builds.")
set(SMROBOT_SDK_MATRIX_SOURCE_DIR "" CACHE PATH
    "Optional package examples source directory.")
set(SMROBOT_SDK_MATRIX_ABI_BASELINE "" CACHE FILEPATH
    "SDK ABI export snapshot baseline passed to package example CTest checks.")
set(SMROBOT_SDK_MATRIX_REQUIRE_ABI_BASELINE ON CACHE BOOL
    "Require SDK ABI export snapshot baseline during package example matrix checks.")

if(NOT SMROBOT_SDK_MATRIX_SDK_PREFIX)
    message(FATAL_ERROR "SMROBOT_SDK_MATRIX_SDK_PREFIX is required.")
endif()

if(NOT SMROBOT_SDK_MATRIX_BUILD_ROOT)
    message(FATAL_ERROR "SMROBOT_SDK_MATRIX_BUILD_ROOT is required.")
endif()

if(NOT SMROBOT_SDK_MATRIX_SOURCE_DIR)
    get_filename_component(_smrobot_matrix_repo_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    set(SMROBOT_SDK_MATRIX_SOURCE_DIR "${_smrobot_matrix_repo_root}/tests/package_examples")
endif()

get_filename_component(_smrobot_matrix_sdk_prefix "${SMROBOT_SDK_MATRIX_SDK_PREFIX}" ABSOLUTE)
get_filename_component(_smrobot_matrix_build_root "${SMROBOT_SDK_MATRIX_BUILD_ROOT}" ABSOLUTE)
get_filename_component(_smrobot_matrix_source_dir "${SMROBOT_SDK_MATRIX_SOURCE_DIR}" ABSOLUTE)

if(NOT EXISTS "${_smrobot_matrix_sdk_prefix}/SMRobotSDKManifest.json")
    message(FATAL_ERROR "SDK manifest is missing: ${_smrobot_matrix_sdk_prefix}/SMRobotSDKManifest.json")
endif()

foreach(_smrobot_matrix_config IN LISTS SMROBOT_SDK_MATRIX_CONFIGS)
    string(TOLOWER "${_smrobot_matrix_config}" _smrobot_matrix_config_lower)
    set(_smrobot_matrix_build_dir "${_smrobot_matrix_build_root}/${_smrobot_matrix_config_lower}")

    set(_smrobot_matrix_configure_command
        "${CMAKE_COMMAND}"
        -S "${_smrobot_matrix_source_dir}"
        -B "${_smrobot_matrix_build_dir}"
        "-DCMAKE_PREFIX_PATH=${_smrobot_matrix_sdk_prefix}"
        "-DSMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX=${_smrobot_matrix_sdk_prefix}"
        -DSMROBOT_PACKAGE_EXAMPLES_BUILD_LEGACY_GEN2=OFF
        -DSMROBOT_PACKAGE_EXAMPLES_BUILD_LICENSE=OFF
        -DSMROBOTGEN2_PACKAGE_EXAMPLES_BUILD_RENDER=OFF
        "-DSMROBOT_PACKAGE_EXAMPLES_REQUIRE_ABI_BASELINE=${SMROBOT_SDK_MATRIX_REQUIRE_ABI_BASELINE}"
    )

    if(SMROBOT_SDK_MATRIX_ABI_BASELINE)
        list(APPEND _smrobot_matrix_configure_command
            "-DSMROBOT_PACKAGE_EXAMPLES_SDK_ABI_BASELINE=${SMROBOT_SDK_MATRIX_ABI_BASELINE}"
        )
    elseif(NOT SMROBOT_SDK_MATRIX_REQUIRE_ABI_BASELINE)
        list(APPEND _smrobot_matrix_configure_command
            "-DSMROBOT_PACKAGE_EXAMPLES_SDK_ABI_BASELINE="
        )
    endif()

    if(SMROBOT_SDK_MATRIX_GENERATOR)
        list(APPEND _smrobot_matrix_configure_command -G "${SMROBOT_SDK_MATRIX_GENERATOR}")
    endif()

    if(SMROBOT_SDK_MATRIX_GENERATOR_PLATFORM)
        list(APPEND _smrobot_matrix_configure_command -A "${SMROBOT_SDK_MATRIX_GENERATOR_PLATFORM}")
    endif()

    message(STATUS "Configuring SDK consumer matrix (${_smrobot_matrix_config})")
    execute_process(
        COMMAND ${_smrobot_matrix_configure_command}
        RESULT_VARIABLE _smrobot_matrix_configure_result
    )
    if(NOT _smrobot_matrix_configure_result EQUAL 0)
        message(FATAL_ERROR "SDK consumer matrix configure failed for ${_smrobot_matrix_config}.")
    endif()

    message(STATUS "Building SDK consumer matrix (${_smrobot_matrix_config})")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${_smrobot_matrix_build_dir}" --config "${_smrobot_matrix_config}"
        RESULT_VARIABLE _smrobot_matrix_build_result
    )
    if(NOT _smrobot_matrix_build_result EQUAL 0)
        message(FATAL_ERROR "SDK consumer matrix build failed for ${_smrobot_matrix_config}.")
    endif()

    message(STATUS "Testing SDK consumer matrix (${_smrobot_matrix_config})")
    execute_process(
        COMMAND "${CMAKE_CTEST_COMMAND}"
            --test-dir "${_smrobot_matrix_build_dir}"
            -C "${_smrobot_matrix_config}"
            --output-on-failure
        RESULT_VARIABLE _smrobot_matrix_test_result
    )
    if(NOT _smrobot_matrix_test_result EQUAL 0)
        message(FATAL_ERROR "SDK consumer matrix tests failed for ${_smrobot_matrix_config}.")
    endif()
endforeach()

message(STATUS "SMRobot SDK consumer matrix passed: ${SMROBOT_SDK_MATRIX_CONFIGS}")
