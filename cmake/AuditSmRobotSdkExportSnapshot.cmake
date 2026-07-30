cmake_minimum_required(VERSION 3.20)

set(SMROBOT_SDK_ABI_AUDIT_SNAPSHOT "" CACHE FILEPATH
    "SMRobot SDK export snapshot file to audit.")
set(SMROBOT_SDK_ABI_AUDIT_OUTPUT "" CACHE FILEPATH
    "Optional output audit report file.")
set(SMROBOT_SDK_ABI_AUDIT_DLL_REGEX ".*" CACHE STRING
    "Regex used to select DLL sections from the export snapshot.")
set(SMROBOT_SDK_ABI_AUDIT_FAIL_ON_FINDINGS ON CACHE BOOL
    "Fail when suspicious exported symbols are found.")
set(SMROBOT_SDK_ABI_AUDIT_LARGE_EXPORT_THRESHOLD "1000" CACHE STRING
    "Per-DLL symbol count threshold reported as large export surface.")
set(SMROBOT_SDK_ABI_AUDIT_SYMBOL_SCOPE_REGEX
    "rendercore|scenecore|robot_render|simulation_runtime|sensorsimulation|simulation_project|assetcore|cameracore|sensorcore|robotinstance|robotruntime|robot|collision"
    CACHE STRING
    "Regex used to limit suspicious-symbol findings to project-owned symbols. Empty means all symbols.")

if(NOT SMROBOT_SDK_ABI_AUDIT_SNAPSHOT)
    message(FATAL_ERROR "SMROBOT_SDK_ABI_AUDIT_SNAPSHOT is required.")
endif()

if(NOT EXISTS "${SMROBOT_SDK_ABI_AUDIT_SNAPSHOT}")
    message(FATAL_ERROR "SDK ABI export snapshot does not exist: ${SMROBOT_SDK_ABI_AUDIT_SNAPSHOT}")
endif()

if(NOT SMROBOT_SDK_ABI_AUDIT_OUTPUT)
    set(SMROBOT_SDK_ABI_AUDIT_OUTPUT "${SMROBOT_SDK_ABI_AUDIT_SNAPSHOT}.audit.txt")
endif()

file(READ "${SMROBOT_SDK_ABI_AUDIT_SNAPSHOT}" _smrobot_abi_audit_snapshot)
string(REPLACE "\r\n" "\n" _smrobot_abi_audit_snapshot "${_smrobot_abi_audit_snapshot}")
string(REPLACE "\r" "\n" _smrobot_abi_audit_snapshot "${_smrobot_abi_audit_snapshot}")
string(REPLACE "\n" ";" _smrobot_abi_audit_lines "${_smrobot_abi_audit_snapshot}")

set(_smrobot_abi_audit_report "# SMRobot SDK ABI export audit\n")
string(APPEND _smrobot_abi_audit_report
    "\n"
    "snapshot=${SMROBOT_SDK_ABI_AUDIT_SNAPSHOT}\n"
    "dll_regex=${SMROBOT_SDK_ABI_AUDIT_DLL_REGEX}\n"
    "fail_on_findings=${SMROBOT_SDK_ABI_AUDIT_FAIL_ON_FINDINGS}\n"
    "large_export_threshold=${SMROBOT_SDK_ABI_AUDIT_LARGE_EXPORT_THRESHOLD}\n"
    "symbol_scope_regex=${SMROBOT_SDK_ABI_AUDIT_SYMBOL_SCOPE_REGEX}\n"
    "\n")

set(_smrobot_abi_audit_current_dll "")
set(_smrobot_abi_audit_current_selected OFF)
set(_smrobot_abi_audit_current_symbol_count 0)
set(_smrobot_abi_audit_selected_dll_count 0)
set(_smrobot_abi_audit_total_symbol_count 0)
set(_smrobot_abi_audit_total_finding_count 0)
set(_smrobot_abi_audit_current_findings)
set(_smrobot_abi_audit_all_large_sections)

function(_smrobot_abi_audit_flush_current_dll)
    if(NOT _smrobot_abi_audit_current_dll)
        return()
    endif()

    if(NOT _smrobot_abi_audit_current_selected)
        return()
    endif()

    string(APPEND _smrobot_abi_audit_report
        "\n"
        "[dll] ${_smrobot_abi_audit_current_dll}\n"
        "symbol_count=${_smrobot_abi_audit_current_symbol_count}\n")

    if(_smrobot_abi_audit_current_symbol_count GREATER SMROBOT_SDK_ABI_AUDIT_LARGE_EXPORT_THRESHOLD)
        string(APPEND _smrobot_abi_audit_report
            "large_export_surface=true\n")
        list(APPEND _smrobot_abi_audit_all_large_sections
            "${_smrobot_abi_audit_current_dll}:${_smrobot_abi_audit_current_symbol_count}")
    else()
        string(APPEND _smrobot_abi_audit_report
            "large_export_surface=false\n")
    endif()

    if(_smrobot_abi_audit_current_findings)
        string(APPEND _smrobot_abi_audit_report "findings:\n")
        foreach(_smrobot_abi_audit_finding IN LISTS _smrobot_abi_audit_current_findings)
            string(APPEND _smrobot_abi_audit_report "  ${_smrobot_abi_audit_finding}\n")
        endforeach()
    else()
        string(APPEND _smrobot_abi_audit_report "findings: none\n")
    endif()

    set(_smrobot_abi_audit_report "${_smrobot_abi_audit_report}" PARENT_SCOPE)
    set(_smrobot_abi_audit_all_large_sections "${_smrobot_abi_audit_all_large_sections}" PARENT_SCOPE)
endfunction()

foreach(_smrobot_abi_audit_line IN LISTS _smrobot_abi_audit_lines)
    if(_smrobot_abi_audit_line MATCHES "^\\[dll\\] (.+)$")
        _smrobot_abi_audit_flush_current_dll()

        set(_smrobot_abi_audit_current_dll "${CMAKE_MATCH_1}")
        if(_smrobot_abi_audit_current_dll MATCHES "${SMROBOT_SDK_ABI_AUDIT_DLL_REGEX}")
            set(_smrobot_abi_audit_current_selected ON)
            math(EXPR _smrobot_abi_audit_selected_dll_count "${_smrobot_abi_audit_selected_dll_count} + 1")
        else()
            set(_smrobot_abi_audit_current_selected OFF)
        endif()

        set(_smrobot_abi_audit_current_symbol_count 0)
        set(_smrobot_abi_audit_current_findings)
        continue()
    endif()

    if(NOT _smrobot_abi_audit_current_selected)
        continue()
    endif()

    if(_smrobot_abi_audit_line MATCHES "^  (.+)$")
        set(_smrobot_abi_audit_symbol "${CMAKE_MATCH_1}")
        math(EXPR _smrobot_abi_audit_current_symbol_count "${_smrobot_abi_audit_current_symbol_count} + 1")
        math(EXPR _smrobot_abi_audit_total_symbol_count "${_smrobot_abi_audit_total_symbol_count} + 1")

        set(_smrobot_abi_audit_symbol_findings)
        set(_smrobot_abi_audit_symbol_in_scope ON)
        if(SMROBOT_SDK_ABI_AUDIT_SYMBOL_SCOPE_REGEX AND
           NOT _smrobot_abi_audit_symbol MATCHES "${SMROBOT_SDK_ABI_AUDIT_SYMBOL_SCOPE_REGEX}")
            set(_smrobot_abi_audit_symbol_in_scope OFF)
        endif()

        if(_smrobot_abi_audit_symbol_in_scope AND
           NOT _smrobot_abi_audit_symbol MATCHES "^\\?\\?\\$" AND
           _smrobot_abi_audit_symbol MATCHES "^\\?+[^@]+@([^@]+@)+@(AE|IE)")
            list(APPEND _smrobot_abi_audit_symbol_findings "private_or_protected_member")
        endif()

        if(_smrobot_abi_audit_symbol_in_scope AND
           _smrobot_abi_audit_symbol MATCHES "^\\?\\?0.*@AEBV")
            list(APPEND _smrobot_abi_audit_symbol_findings "copy_constructor")
        endif()

        if(_smrobot_abi_audit_symbol_in_scope AND
           _smrobot_abi_audit_symbol MATCHES "^\\?\\?4")
            list(APPEND _smrobot_abi_audit_symbol_findings "copy_assignment")
        endif()

        foreach(_smrobot_abi_audit_symbol_finding IN LISTS _smrobot_abi_audit_symbol_findings)
            list(APPEND _smrobot_abi_audit_current_findings
                "${_smrobot_abi_audit_symbol_finding}: ${_smrobot_abi_audit_symbol}")
            math(EXPR _smrobot_abi_audit_total_finding_count "${_smrobot_abi_audit_total_finding_count} + 1")
        endforeach()
    endif()
endforeach()

_smrobot_abi_audit_flush_current_dll()

string(APPEND _smrobot_abi_audit_report
    "\n"
    "[summary]\n"
    "selected_dll_count=${_smrobot_abi_audit_selected_dll_count}\n"
    "selected_symbol_count=${_smrobot_abi_audit_total_symbol_count}\n"
    "finding_count=${_smrobot_abi_audit_total_finding_count}\n")

if(_smrobot_abi_audit_all_large_sections)
    string(APPEND _smrobot_abi_audit_report "large_sections:\n")
    foreach(_smrobot_abi_audit_large_section IN LISTS _smrobot_abi_audit_all_large_sections)
        string(APPEND _smrobot_abi_audit_report "  ${_smrobot_abi_audit_large_section}\n")
    endforeach()
else()
    string(APPEND _smrobot_abi_audit_report "large_sections: none\n")
endif()

file(WRITE "${SMROBOT_SDK_ABI_AUDIT_OUTPUT}" "${_smrobot_abi_audit_report}")
message(STATUS "SMRobot SDK ABI audit report written: ${SMROBOT_SDK_ABI_AUDIT_OUTPUT}")
message(STATUS
    "SMRobot SDK ABI audit selected ${_smrobot_abi_audit_selected_dll_count} DLL(s), "
    "${_smrobot_abi_audit_total_symbol_count} symbol(s), "
    "${_smrobot_abi_audit_total_finding_count} finding(s).")

if(_smrobot_abi_audit_selected_dll_count EQUAL 0)
    message(FATAL_ERROR
        "SDK ABI audit did not select any DLL sections. Regex: ${SMROBOT_SDK_ABI_AUDIT_DLL_REGEX}")
endif()

if(SMROBOT_SDK_ABI_AUDIT_FAIL_ON_FINDINGS AND _smrobot_abi_audit_total_finding_count GREATER 0)
    message(FATAL_ERROR
        "SDK ABI audit found suspicious exported symbols. Report: ${SMROBOT_SDK_ABI_AUDIT_OUTPUT}")
endif()
