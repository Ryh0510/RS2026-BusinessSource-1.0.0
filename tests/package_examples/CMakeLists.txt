cmake_minimum_required(VERSION 3.20)

project(SMRobotGen2PackageExamples LANGUAGES CXX)

include(CTest)

option(SMROBOTGEN2_PACKAGE_EXAMPLES_BUILD_RENDER "Build render bridge package consumer example" ON)
option(SMROBOT_PACKAGE_EXAMPLES_BUILD_LEGACY_GEN2 "Build legacy SMRobotGen2 package consumer examples" ON)
option(SMROBOT_PACKAGE_EXAMPLES_BUILD_LICENSE "Build license authorization package consumer example" ON)
option(SMROBOT_PACKAGE_EXAMPLES_BUILD_CORE_PLATFORM_DLL "Build SMRobotCore/SMRobotPlatform DLL component consumer examples" ON)
option(SMROBOT_PACKAGE_EXAMPLES_BUILD_ABI_EXPORT_SNAPSHOT "Run SDK DLL export snapshot smoke checks when validating package examples." ON)
option(SMROBOT_PACKAGE_EXAMPLES_REQUIRE_ABI_BASELINE "Require SDK DLL export snapshot to match a checked-in ABI baseline." ON)
set(SMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX "${CMAKE_PREFIX_PATH}" CACHE STRING
    "SDK prefix used by package-example CTest component checks.")
set(SMROBOT_PACKAGE_EXAMPLES_SDK_ABI_BASELINE
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdk_abi_baselines/SMRobotSDKExportSnapshot.v2.txt"
    CACHE FILEPATH
    "SDK ABI export snapshot baseline used by package-example CTest checks.")

if(SMROBOT_PACKAGE_EXAMPLES_BUILD_LEGACY_GEN2)
    add_subdirectory(robot_sdk_quickstart)
    add_subdirectory(collision_core)
endif()

if(SMROBOT_PACKAGE_EXAMPLES_BUILD_LICENSE)
    add_subdirectory(license_authorization_consumer)
endif()

if(SMROBOT_PACKAGE_EXAMPLES_BUILD_CORE_PLATFORM_DLL)
    add_subdirectory(core_components_smoke)
    add_subdirectory(platform_components_smoke)
    add_subdirectory(robotio_urdf_load)
    add_subdirectory(project_simulation_sdk_consumer)
endif()

if(SMROBOT_PACKAGE_EXAMPLES_BUILD_LEGACY_GEN2 AND SMROBOTGEN2_PACKAGE_EXAMPLES_BUILD_RENDER)
    add_subdirectory(robot_render_bridge_smoke)
endif()

if(BUILD_TESTING AND SMROBOT_PACKAGE_EXAMPLES_BUILD_CORE_PLATFORM_DLL)
    add_test(
        NAME SMRobotSDK.ManifestSmoke
        COMMAND ${CMAKE_COMMAND}
            "-DSMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX=${SMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX}"
            -P "${CMAKE_CURRENT_LIST_DIR}/cmake/CheckSdkManifest.cmake"
    )

    if(WIN32 AND SMROBOT_PACKAGE_EXAMPLES_BUILD_ABI_EXPORT_SNAPSHOT)
        add_test(
            NAME SMRobotSDK.ExportSnapshot
            COMMAND ${CMAKE_COMMAND}
                "-DSMROBOT_SDK_ABI_SDK_PREFIX=${SMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX}"
                "-DSMROBOT_SDK_ABI_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/SMRobotSDKExportSnapshot.txt"
                "-DSMROBOT_SDK_ABI_BASELINE=${SMROBOT_PACKAGE_EXAMPLES_SDK_ABI_BASELINE}"
                "-DSMROBOT_SDK_ABI_REQUIRE_BASELINE=${SMROBOT_PACKAGE_EXAMPLES_REQUIRE_ABI_BASELINE}"
                -P "${CMAKE_CURRENT_LIST_DIR}/../../cmake/CheckSmRobotSdkExportSnapshot.cmake"
        )
    endif()

    add_test(
        NAME SMRobotSDK.ComponentSelection
        COMMAND ${CMAKE_COMMAND}
            "-DSMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX=${SMROBOT_PACKAGE_EXAMPLES_SDK_PREFIX}"
            "-DSMROBOT_PACKAGE_EXAMPLES_CHECK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/d4_component_checks"
            "-DSMROBOT_PACKAGE_EXAMPLES_GENERATOR=${CMAKE_GENERATOR}"
            -P "${CMAKE_CURRENT_LIST_DIR}/cmake/CheckD4PackageComponents.cmake"
    )
endif()
