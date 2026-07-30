cmake_minimum_required(VERSION 3.20)

if(NOT SMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX)
    message(FATAL_ERROR "SMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX is required.")
endif()

set(_smrobot_manifest "${SMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX}/SMRobotSDKManifest.json")

if(NOT EXISTS "${_smrobot_manifest}")
    message(FATAL_ERROR "SDK manifest is missing: ${_smrobot_manifest}")
endif()

file(READ "${_smrobot_manifest}" _smrobot_manifest_content)

foreach(_smrobot_required_token
    "\"schema\": \"SMRobotSDKManifest.v1\""
    "\"profile\":"
    "\"configs\":"
    "\"install_components\":"
    "\"dll_files\":"
    "\"pdb_policy\":"
    "\"runtime_dependency_policy\":"
)
    string(FIND "${_smrobot_manifest_content}" "${_smrobot_required_token}" _smrobot_token_index)
    if(_smrobot_token_index EQUAL -1)
        message(FATAL_ERROR "SDK manifest is missing token ${_smrobot_required_token}: ${_smrobot_manifest}")
    endif()
endforeach()

message(STATUS "SDK manifest smoke passed: ${_smrobot_manifest}")
