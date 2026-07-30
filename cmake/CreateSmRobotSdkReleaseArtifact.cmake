cmake_minimum_required(VERSION 3.20)

set(SMROBOT_SDK_RELEASE_SDK_PREFIX "" CACHE PATH
    "Installed SMRobot SDK prefix to archive.")
set(SMROBOT_SDK_RELEASE_OUTPUT_DIR "" CACHE PATH
    "Directory where the SDK release archive will be written.")
set(SMROBOT_SDK_RELEASE_ARCHIVE_NAME "" CACHE STRING
    "Optional SDK release archive file name.")
set(SMROBOT_SDK_RELEASE_EXPORT_BASELINE "" CACHE FILEPATH
    "Optional SDK export snapshot baseline.")
set(SMROBOT_SDK_RELEASE_REQUIRE_EXPORT_BASELINE ON CACHE BOOL
    "Require SDK export snapshot baseline to exist and match before creating the release artifact.")
set(SMROBOT_SDK_RELEASE_DUMPBIN "" CACHE FILEPATH
    "Optional explicit dumpbin.exe path.")

if(NOT SMROBOT_SDK_RELEASE_SDK_PREFIX)
    message(FATAL_ERROR "SMROBOT_SDK_RELEASE_SDK_PREFIX is required.")
endif()

if(NOT SMROBOT_SDK_RELEASE_OUTPUT_DIR)
    message(FATAL_ERROR "SMROBOT_SDK_RELEASE_OUTPUT_DIR is required.")
endif()

get_filename_component(_smrobot_release_sdk_prefix "${SMROBOT_SDK_RELEASE_SDK_PREFIX}" ABSOLUTE)
get_filename_component(_smrobot_release_output_dir "${SMROBOT_SDK_RELEASE_OUTPUT_DIR}" ABSOLUTE)

set(_smrobot_release_manifest "${_smrobot_release_sdk_prefix}/SMRobotSDKManifest.json")
if(NOT EXISTS "${_smrobot_release_manifest}")
    message(FATAL_ERROR "SDK manifest is missing: ${_smrobot_release_manifest}")
endif()

set(_smrobot_release_snapshot "${_smrobot_release_sdk_prefix}/SMRobotSDKExportSnapshot.txt")

if(NOT SMROBOT_SDK_RELEASE_EXPORT_BASELINE)
    set(_smrobot_release_default_baseline
        "${CMAKE_CURRENT_LIST_DIR}/sdk_abi_baselines/SMRobotSDKExportSnapshot.v2.txt")
    if(EXISTS "${_smrobot_release_default_baseline}")
        set(SMROBOT_SDK_RELEASE_EXPORT_BASELINE "${_smrobot_release_default_baseline}")
    endif()
endif()

set(_smrobot_release_snapshot_args
    "-DSMROBOT_SDK_ABI_SDK_PREFIX=${_smrobot_release_sdk_prefix}"
    "-DSMROBOT_SDK_ABI_OUTPUT=${_smrobot_release_snapshot}"
    "-DSMROBOT_SDK_ABI_REQUIRE_BASELINE=${SMROBOT_SDK_RELEASE_REQUIRE_EXPORT_BASELINE}"
)

if(SMROBOT_SDK_RELEASE_EXPORT_BASELINE)
    list(APPEND _smrobot_release_snapshot_args
        "-DSMROBOT_SDK_ABI_BASELINE=${SMROBOT_SDK_RELEASE_EXPORT_BASELINE}"
    )
endif()

if(SMROBOT_SDK_RELEASE_DUMPBIN)
    list(APPEND _smrobot_release_snapshot_args
        "-DSMROBOT_SDK_ABI_DUMPBIN=${SMROBOT_SDK_RELEASE_DUMPBIN}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        ${_smrobot_release_snapshot_args}
        -P "${CMAKE_CURRENT_LIST_DIR}/CheckSmRobotSdkExportSnapshot.cmake"
    RESULT_VARIABLE _smrobot_release_snapshot_result
)

if(NOT _smrobot_release_snapshot_result EQUAL 0)
    message(FATAL_ERROR "SDK export snapshot check failed; release artifact was not created.")
endif()

file(MAKE_DIRECTORY "${_smrobot_release_output_dir}")

if(NOT SMROBOT_SDK_RELEASE_ARCHIVE_NAME)
    string(TIMESTAMP _smrobot_release_timestamp "%Y%m%d-%H%M%S" UTC)
    set(SMROBOT_SDK_RELEASE_ARCHIVE_NAME "SMRobotSDK-${_smrobot_release_timestamp}.zip")
endif()

if(NOT SMROBOT_SDK_RELEASE_ARCHIVE_NAME MATCHES "\\.zip$")
    set(SMROBOT_SDK_RELEASE_ARCHIVE_NAME "${SMROBOT_SDK_RELEASE_ARCHIVE_NAME}.zip")
endif()

set(_smrobot_release_archive "${_smrobot_release_output_dir}/${SMROBOT_SDK_RELEASE_ARCHIVE_NAME}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${_smrobot_release_archive}" --format=zip .
    WORKING_DIRECTORY "${_smrobot_release_sdk_prefix}"
    RESULT_VARIABLE _smrobot_release_archive_result
)

if(NOT _smrobot_release_archive_result EQUAL 0)
    message(FATAL_ERROR "Failed to create SDK release archive: ${_smrobot_release_archive}")
endif()

if(NOT EXISTS "${_smrobot_release_archive}")
    get_filename_component(_smrobot_release_archive_ext "${_smrobot_release_archive}" EXT)
    if(_smrobot_release_archive_ext)
        string(REGEX REPLACE "${_smrobot_release_archive_ext}$" "" _smrobot_release_archive_without_ext "${_smrobot_release_archive}")
        if(EXISTS "${_smrobot_release_archive_without_ext}")
            file(RENAME "${_smrobot_release_archive_without_ext}" "${_smrobot_release_archive}")
        endif()
    endif()
endif()

if(NOT EXISTS "${_smrobot_release_archive}")
    message(FATAL_ERROR "SDK release archive was not created at expected path: ${_smrobot_release_archive}")
endif()

file(SHA256 "${_smrobot_release_archive}" _smrobot_release_sha256)
file(WRITE "${_smrobot_release_archive}.sha256"
    "${_smrobot_release_sha256}  ${SMROBOT_SDK_RELEASE_ARCHIVE_NAME}\n")

file(WRITE "${_smrobot_release_archive}.release.txt"
    "SMRobot SDK release artifact\n"
    "archive=${_smrobot_release_archive}\n"
    "sha256=${_smrobot_release_sha256}\n"
    "sdk_prefix=${_smrobot_release_sdk_prefix}\n"
    "manifest=SMRobotSDKManifest.json\n"
    "export_snapshot=SMRobotSDKExportSnapshot.txt\n")

message(STATUS "SMRobot SDK release archive written: ${_smrobot_release_archive}")
message(STATUS "SMRobot SDK release archive SHA256: ${_smrobot_release_sha256}")
