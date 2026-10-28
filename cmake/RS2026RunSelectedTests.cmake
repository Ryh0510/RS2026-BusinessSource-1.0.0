cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED RS2026_TEST_DIRS_FILE OR NOT EXISTS "${RS2026_TEST_DIRS_FILE}")
    message(FATAL_ERROR "RS2026_TEST_DIRS_FILE is missing")
endif()
if(NOT DEFINED RS2026_TEST_CONFIG OR RS2026_TEST_CONFIG STREQUAL "")
    message(FATAL_ERROR "RS2026_TEST_CONFIG is missing")
endif()
if(NOT DEFINED RS2026_TEST_BUILD_ROOT OR NOT IS_DIRECTORY "${RS2026_TEST_BUILD_ROOT}")
    message(FATAL_ERROR "RS2026_TEST_BUILD_ROOT is missing")
endif()

find_program(_ctest_command NAMES ctest REQUIRED)
include("${RS2026_TEST_DIRS_FILE}")
if(NOT RS2026_SELECTED_TEST_DIRS)
    message(FATAL_ERROR "This test set has no selected directories; it cannot be reported as passed")
endif()

if(WIN32)
    set(_test_temp "${RS2026_TEST_BUILD_ROOT}/test_tmp")
    file(MAKE_DIRECTORY "${_test_temp}")
    set(ENV{TEMP} "${_test_temp}")
    set(ENV{TMP} "${_test_temp}")
endif()

string(TOUPPER "${RS2026_TEST_CONFIG}" _config_upper)
set(_runtime_dirs "${RS2026_TEST_RUNTIME_DIRS_${_config_upper}}")
if(_runtime_dirs)
    if(WIN32)
        list(JOIN _runtime_dirs ";" _runtime_path)
        set(ENV{PATH} "${_runtime_path};$ENV{PATH}")
    else()
        list(JOIN _runtime_dirs ":" _runtime_path)
        set(ENV{LD_LIBRARY_PATH} "${_runtime_path}:$ENV{LD_LIBRARY_PATH}")
    endif()
endif()

execute_process(
    COMMAND "${_ctest_command}" --test-dir "${RS2026_TEST_BUILD_ROOT}"
        -C "${RS2026_TEST_CONFIG}"
        -R "^SoftwareArchitecture-BoundaryCheck$" --output-on-failure
    RESULT_VARIABLE _architecture_result)
if(NOT _architecture_result EQUAL 0)
    message(FATAL_ERROR "Architecture gate failed with exit code ${_architecture_result}")
endif()

set(_total_cases 0)
set(_failed_dirs)
foreach(_dir IN LISTS RS2026_SELECTED_TEST_DIRS)
    execute_process(
        COMMAND "${_ctest_command}" --test-dir "${_dir}"
            -C "${RS2026_TEST_CONFIG}" -N
        RESULT_VARIABLE _list_result
        OUTPUT_VARIABLE _list_output
        ERROR_VARIABLE _list_error)
    if(NOT _list_result EQUAL 0 OR
       "${_list_output}${_list_error}" MATCHES "Could not find executable")
        message(FATAL_ERROR "Selected test preflight failed in ${_dir}:\n${_list_output}\n${_list_error}")
    endif()
    string(REGEX MATCH "Total Tests: ([0-9]+)" _count_match "${_list_output}")
    if(NOT _count_match OR CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "Selected test directory has no registered cases: ${_dir}")
    endif()
    math(EXPR _total_cases "${_total_cases} + ${CMAKE_MATCH_1}")
    message(STATUS "Run ${CMAKE_MATCH_1} selected tests from ${_dir}")
    execute_process(
        COMMAND "${_ctest_command}" --test-dir "${_dir}"
            -C "${RS2026_TEST_CONFIG}" --output-on-failure
        RESULT_VARIABLE _test_result
        OUTPUT_VARIABLE _test_output
        ERROR_VARIABLE _test_error)
    message("${_test_output}${_test_error}")
    if(NOT _test_result EQUAL 0 OR
       "${_test_output}${_test_error}" MATCHES "Tests not run|Not Run|Skipped")
        list(APPEND _failed_dirs "${_dir}")
    endif()
endforeach()
if(_failed_dirs)
    message(FATAL_ERROR
        "Selected tests failed or were skipped in ${_failed_dirs}; checked ${_total_cases} cases (${RS2026_TEST_CONFIG})")
endif()
message(STATUS "Selected test cases passed: ${_total_cases} (${RS2026_TEST_CONFIG})")
