cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED COLLISION_SOURCE_ROOT OR NOT IS_DIRECTORY "${COLLISION_SOURCE_ROOT}")
    message(FATAL_ERROR "COLLISION_SOURCE_ROOT must name the repository root.")
endif()
if(NOT DEFINED COLLISION_INSTALL_PREFIX OR NOT IS_DIRECTORY "${COLLISION_INSTALL_PREFIX}")
    message(FATAL_ERROR "COLLISION_INSTALL_PREFIX must name an installed SDK root.")
endif()

include("${COLLISION_SOURCE_ROOT}/SMRobotCore/Collision/CollisionPublicApiManifest.cmake")
set(_collision_installed_include
    "${COLLISION_INSTALL_PREFIX}/SMRobotCore/include")
set(_collision_expected_headers ${COLLISION_PUBLIC_HEADER_INVENTORY})
set(_collision_actual_headers)
file(GLOB_RECURSE _collision_installed_headers LIST_DIRECTORIES FALSE
    "${_collision_installed_include}/Collision/*.h"
    "${_collision_installed_include}/Collision/*.hpp")
foreach(_collision_installed_header IN LISTS _collision_installed_headers)
    file(RELATIVE_PATH _collision_installed_header_relative
        "${_collision_installed_include}"
        "${_collision_installed_header}")
    file(TO_CMAKE_PATH "${_collision_installed_header_relative}"
        _collision_installed_header_relative)
    list(APPEND _collision_actual_headers "${_collision_installed_header_relative}")
endforeach()

list(SORT _collision_expected_headers)
list(SORT _collision_actual_headers)
if(NOT _collision_expected_headers STREQUAL _collision_actual_headers)
    message(FATAL_ERROR
        "Installed Collision headers differ from the public manifest.\n"
        "Expected: ${_collision_expected_headers}\n"
        "Actual: ${_collision_actual_headers}")
endif()

message(STATUS "Collision installed header allowlist passed: ${_collision_actual_headers}")
