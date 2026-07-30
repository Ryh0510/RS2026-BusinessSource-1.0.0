cmake_minimum_required(VERSION 3.20)

set(SMROBOT_SDK_ABI_SDK_PREFIX "" CACHE PATH
    "Installed SMRobot SDK prefix to inspect.")
set(SMROBOT_SDK_ABI_OUTPUT "" CACHE FILEPATH
    "Output export snapshot file.")
set(SMROBOT_SDK_ABI_BASELINE "" CACHE FILEPATH
    "Optional baseline export snapshot file to compare against.")
set(SMROBOT_SDK_ABI_REQUIRE_BASELINE OFF CACHE BOOL
    "Require SMROBOT_SDK_ABI_BASELINE to exist.")
set(SMROBOT_SDK_ABI_DUMPBIN "" CACHE FILEPATH
    "Optional explicit dumpbin.exe path.")
set(SMROBOT_SDK_ABI_DLL_EXCLUDE_REGEX
    "^SMRobotPlatform/bin/GLRuntime_shared_.*\\.dll$"
    CACHE STRING
    "Regex for installed private runtime DLLs to exclude from the official SDK ABI snapshot.")

if(NOT SMROBOT_SDK_ABI_SDK_PREFIX)
    message(FATAL_ERROR "SMROBOT_SDK_ABI_SDK_PREFIX is required.")
endif()

get_filename_component(_smrobot_abi_sdk_prefix "${SMROBOT_SDK_ABI_SDK_PREFIX}" ABSOLUTE)

if(NOT EXISTS "${_smrobot_abi_sdk_prefix}/SMRobotSDKManifest.json")
    message(FATAL_ERROR "SDK manifest is missing: ${_smrobot_abi_sdk_prefix}/SMRobotSDKManifest.json")
endif()

if(NOT SMROBOT_SDK_ABI_OUTPUT)
    set(SMROBOT_SDK_ABI_OUTPUT "${_smrobot_abi_sdk_prefix}/SMRobotSDKExportSnapshot.txt")
endif()

if(NOT SMROBOT_SDK_ABI_DUMPBIN)
    find_program(SMROBOT_SDK_ABI_DUMPBIN dumpbin)
endif()

if(NOT SMROBOT_SDK_ABI_DUMPBIN)
    file(GLOB_RECURSE _smrobot_dumpbin_candidates
        "C:/Program Files/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"
        "C:/Program Files (x86)/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"
    )
    if(_smrobot_dumpbin_candidates)
        list(SORT _smrobot_dumpbin_candidates)
        list(REVERSE _smrobot_dumpbin_candidates)
        list(GET _smrobot_dumpbin_candidates 0 SMROBOT_SDK_ABI_DUMPBIN)
    endif()
endif()

if(NOT SMROBOT_SDK_ABI_DUMPBIN OR NOT EXISTS "${SMROBOT_SDK_ABI_DUMPBIN}")
    message(FATAL_ERROR
        "dumpbin.exe was not found. Set SMROBOT_SDK_ABI_DUMPBIN to enable SDK export snapshot checks.")
endif()

file(GLOB _smrobot_abi_core_dlls
    "${_smrobot_abi_sdk_prefix}/SMRobotCore/bin/*_shared_*.dll"
)
file(GLOB _smrobot_abi_platform_dlls
    "${_smrobot_abi_sdk_prefix}/SMRobotPlatform/bin/*_shared_*.dll"
)
set(_smrobot_abi_dlls ${_smrobot_abi_core_dlls} ${_smrobot_abi_platform_dlls})
list(SORT _smrobot_abi_dlls)

if(NOT _smrobot_abi_dlls)
    message(FATAL_ERROR "No SMRobotCore/SMRobotPlatform SDK DLLs were found under ${_smrobot_abi_sdk_prefix}.")
endif()

if(SMROBOT_SDK_ABI_DLL_EXCLUDE_REGEX)
    set(_smrobot_abi_filtered_dlls)
    set(_smrobot_abi_excluded_dlls)
    foreach(_smrobot_abi_dll IN LISTS _smrobot_abi_dlls)
        file(RELATIVE_PATH _smrobot_abi_dll_rel "${_smrobot_abi_sdk_prefix}" "${_smrobot_abi_dll}")
        if(_smrobot_abi_dll_rel MATCHES "${SMROBOT_SDK_ABI_DLL_EXCLUDE_REGEX}")
            list(APPEND _smrobot_abi_excluded_dlls "${_smrobot_abi_dll_rel}")
        else()
            list(APPEND _smrobot_abi_filtered_dlls "${_smrobot_abi_dll}")
        endif()
    endforeach()
    set(_smrobot_abi_dlls ${_smrobot_abi_filtered_dlls})
    if(_smrobot_abi_excluded_dlls)
        list(JOIN _smrobot_abi_excluded_dlls ", " _smrobot_abi_excluded_dlls_text)
        message(STATUS "SMRobot SDK export snapshot excluded private runtime DLLs: ${_smrobot_abi_excluded_dlls_text}")
    endif()
endif()

if(NOT _smrobot_abi_dlls)
    message(FATAL_ERROR "No SDK ABI DLLs remain after SMROBOT_SDK_ABI_DLL_EXCLUDE_REGEX filtering.")
endif()

set(_smrobot_abi_snapshot "# SMRobot SDK export snapshot v2\n")

foreach(_smrobot_abi_dll IN LISTS _smrobot_abi_dlls)
    file(RELATIVE_PATH _smrobot_abi_dll_rel "${_smrobot_abi_sdk_prefix}" "${_smrobot_abi_dll}")
    execute_process(
        COMMAND "${SMROBOT_SDK_ABI_DUMPBIN}" /exports "${_smrobot_abi_dll}"
        RESULT_VARIABLE _smrobot_abi_dump_result
        OUTPUT_VARIABLE _smrobot_abi_dump_output
        ERROR_VARIABLE _smrobot_abi_dump_error
    )

    if(NOT _smrobot_abi_dump_result EQUAL 0)
        message(FATAL_ERROR
            "dumpbin /exports failed for ${_smrobot_abi_dll_rel}: ${_smrobot_abi_dump_error}")
    endif()

    string(REPLACE "\r\n" "\n" _smrobot_abi_dump_output "${_smrobot_abi_dump_output}")
    string(REPLACE "\r" "\n" _smrobot_abi_dump_output "${_smrobot_abi_dump_output}")
    string(REPLACE "\n" ";" _smrobot_abi_dump_lines "${_smrobot_abi_dump_output}")

    set(_smrobot_abi_symbols)
    foreach(_smrobot_abi_line IN LISTS _smrobot_abi_dump_lines)
        if(_smrobot_abi_line MATCHES "^[ \t]*[0-9]+[ \t]+[0-9A-Fa-f]+[ \t]+[0-9A-Fa-f]+[ \t]+([^ \t]+)")
            list(APPEND _smrobot_abi_symbols "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _smrobot_abi_symbols)
    list(SORT _smrobot_abi_symbols)

    string(APPEND _smrobot_abi_snapshot "\n[dll] ${_smrobot_abi_dll_rel}\n")
    foreach(_smrobot_abi_symbol IN LISTS _smrobot_abi_symbols)
        string(APPEND _smrobot_abi_snapshot "  ${_smrobot_abi_symbol}\n")
    endforeach()
endforeach()

file(WRITE "${SMROBOT_SDK_ABI_OUTPUT}" "${_smrobot_abi_snapshot}")
message(STATUS "SMRobot SDK export snapshot written: ${SMROBOT_SDK_ABI_OUTPUT}")

if(SMROBOT_SDK_ABI_REQUIRE_BASELINE AND NOT EXISTS "${SMROBOT_SDK_ABI_BASELINE}")
    message(FATAL_ERROR "Required SDK ABI baseline is missing: ${SMROBOT_SDK_ABI_BASELINE}")
endif()

if(SMROBOT_SDK_ABI_BASELINE AND EXISTS "${SMROBOT_SDK_ABI_BASELINE}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${SMROBOT_SDK_ABI_BASELINE}"
            "${SMROBOT_SDK_ABI_OUTPUT}"
        RESULT_VARIABLE _smrobot_abi_compare_result
    )

    if(NOT _smrobot_abi_compare_result EQUAL 0)
        message(FATAL_ERROR
            "SDK ABI export snapshot differs from baseline.\n"
            "Current: ${SMROBOT_SDK_ABI_OUTPUT}\n"
            "Baseline: ${SMROBOT_SDK_ABI_BASELINE}")
    endif()

    message(STATUS "SMRobot SDK export snapshot matches baseline: ${SMROBOT_SDK_ABI_BASELINE}")
endif()
