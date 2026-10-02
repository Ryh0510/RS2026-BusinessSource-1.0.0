cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SMROBOT_SOURCE_ROOT OR NOT IS_DIRECTORY "${SMROBOT_SOURCE_ROOT}")
    message(FATAL_ERROR "SMROBOT_SOURCE_ROOT must name the repository root.")
endif()

file(TO_CMAKE_PATH "${SMROBOT_SOURCE_ROOT}" SMROBOT_SOURCE_ROOT)

set(_smrobot_production_extensions
    "*.h"
    "*.hpp"
    "*.cpp"
    "*.cxx"
    "*.cc"
)

set(_smrobot_excluded_path_pattern
    "/(regression|tests?|examples?|feature_probes|diagnostics|external_validation|external|thirdparty|build|install_headers)/")

set(_smrobot_qt_app_workbench_include_pattern
    "^(Q[A-Z][A-Za-z0-9_/]*|Qt[A-Za-z0-9_/]*|RobotQt[A-Za-z0-9_/]*|SMRobotApps/|SMRobotWorkbench[A-Za-z0-9_/]*|SimWorkbench/)")

set(_smrobot_platform_include_pattern
    "^(AssetCore/|CameraCore/|ProjectSimulationSDK/|RenderCore/|RobotPlatformAuthorization/|RobotRenderBridge/|SceneCore/|SensorCore/|SensorSimulation/|SimulationProject/|SimulationRuntime/|VisualizationSDK/|SMRobotPlatform/)")

function(_smrobot_check_include_boundaries layer relative_root forbid_platform)
    set(_root "${SMROBOT_SOURCE_ROOT}/${relative_root}")
    set(_files)
    foreach(_extension IN LISTS _smrobot_production_extensions)
        file(GLOB_RECURSE _extension_files LIST_DIRECTORIES FALSE "${_root}/${_extension}")
        list(APPEND _files ${_extension_files})
    endforeach()
    list(REMOVE_DUPLICATES _files)

    set(_violations)
    set(_checked_file_count 0)
    foreach(_file IN LISTS _files)
        file(TO_CMAKE_PATH "${_file}" _normalized_file)
        if(_normalized_file MATCHES "${_smrobot_excluded_path_pattern}")
            continue()
        endif()

        math(EXPR _checked_file_count "${_checked_file_count} + 1")
        file(READ "${_file}" _content)
        string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"]+[>\"]" _include_lines "${_content}")
        foreach(_include_line IN LISTS _include_lines)
            string(REGEX REPLACE "^[^<\"]*[<\"]([^>\"]+)[>\"].*$" "\\1" _include "${_include_line}")
            set(_forbidden FALSE)
            if(_include MATCHES "${_smrobot_qt_app_workbench_include_pattern}")
                set(_forbidden TRUE)
            elseif(forbid_platform AND _include MATCHES "${_smrobot_platform_include_pattern}")
                set(_forbidden TRUE)
            endif()

            if(_forbidden)
                file(RELATIVE_PATH _relative_file "${SMROBOT_SOURCE_ROOT}" "${_file}")
                list(APPEND _violations "${_relative_file}: ${_include}")
            endif()
        endforeach()
    endforeach()

    if(_violations)
        list(JOIN _violations "\n  " _violation_text)
        message(FATAL_ERROR
            "${layer} contains forbidden upward includes:\n  ${_violation_text}")
    endif()

    message(STATUS "[Architecture] ${layer}: checked ${_checked_file_count} production files")
endfunction()

_smrobot_check_include_boundaries("SMRobotCore" "SMRobotCore" TRUE)
_smrobot_check_include_boundaries("SMRobotPlatform" "SMRobotPlatform" FALSE)
_smrobot_check_include_boundaries("SMRobotMotionPlanning" "SMRobotMotionPlanning" FALSE)
_smrobot_check_include_boundaries("SMRobotSpray" "SMRobotSpray" FALSE)

if(NOT EXISTS "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/PackageConfigSetting.cmake")
    message(STATUS "[Architecture] Business Source: public business-layer include checks passed; private SDK source checks unavailable")
    return()
endif()

set(_platform_package_config
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/PackageConfigSetting.cmake")
file(READ "${_platform_package_config}" _platform_package_config_content)

foreach(_required_platform_family IN ITEMS
        FoundationComponents
        SimulationComponents
        SensorComponents
        PresentationComponents
        FacadeComponents
        InternalComponents
        RuntimeResourceComponents)
    string(FIND "${_platform_package_config_content}"
        "set( \${PACKAGE_NAME}_${_required_platform_family}"
        _platform_family_index)
    if(_platform_family_index EQUAL -1)
        message(FATAL_ERROR
            "SMRobotPlatform component family is missing: ${_required_platform_family}.")
    endif()
endforeach()

string(FIND "${_platform_package_config_content}"
    "collect_subdirs("
    _platform_automatic_component_scan_index)
if(NOT _platform_automatic_component_scan_index EQUAL -1)
    message(FATAL_ERROR
        "SMRobotPlatform components must use the explicit family manifest, not collect_subdirs().")
endif()

string(REGEX MATCH
    "set\\([ \t\r\n]+\\$\\{PACKAGE_NAME\\}_InternalComponents[ \t\r\n]+RobotPlatformAuthorization[ \t\r\n]+\\)"
    _platform_internal_component_match
    "${_platform_package_config_content}")
if(NOT _platform_internal_component_match)
    message(FATAL_ERROR
        "RobotPlatformAuthorization must remain an internal SMRobotPlatform component.")
endif()

string(REGEX MATCH
    "set\\([ \t\r\n]+\\$\\{PACKAGE_NAME\\}_RuntimeResourceComponents[ \t\r\n]+RenderCoreShaderResources[ \t\r\n]+\\)"
    _platform_runtime_resource_match
    "${_platform_package_config_content}")
if(NOT _platform_runtime_resource_match)
    message(FATAL_ERROR
        "RenderCoreShaderResources must remain classified as a runtime resource component.")
endif()

message(STATUS
    "[Architecture] SMRobotPlatform explicit component families: 7; automatic scan: disabled")

set(_platform_physical_family_directories
    Foundation
    Simulation
    Sensors
    Presentation
    Facades
    Internal)
foreach(_platform_physical_family IN LISTS _platform_physical_family_directories)
    if(NOT IS_DIRECTORY
       "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/${_platform_physical_family}")
        message(FATAL_ERROR
            "SMRobotPlatform physical family directory is missing: "
            "${_platform_physical_family}.")
    endif()
endforeach()

set(_platform_grouped_component_paths
    "AssetCore|Foundation/AssetCore"
    "CameraCore|Foundation/CameraCore"
    "RenderCore|Foundation/RenderCore"
    "SceneCore|Foundation/SceneCore"
    "SimulationProject|Simulation/SimulationProject"
    "SimulationRuntime|Simulation/SimulationRuntime"
    "SensorCore|Sensors/SensorCore"
    "SensorSimulation|Sensors/SensorSimulation"
    "RobotRenderBridge|Presentation/RobotRenderBridge"
    "ProjectSimulationSDK|Facades/ProjectSimulationSDK"
    "VisualizationSDK|Facades/VisualizationSDK"
    "RobotPlatformAuthorization|Internal/RobotPlatformAuthorization"
    "RenderCoreShaderResources|Internal/RuntimeResources/RenderCoreShaderResources")
foreach(_platform_grouped_component IN LISTS _platform_grouped_component_paths)
    string(REPLACE "|" ";" _platform_grouped_component_fields
        "${_platform_grouped_component}")
    list(GET _platform_grouped_component_fields 0 _platform_grouped_component_name)
    list(GET _platform_grouped_component_fields 1 _platform_grouped_component_path)
    if(NOT IS_DIRECTORY
       "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/${_platform_grouped_component_path}")
        message(FATAL_ERROR
            "Grouped SMRobotPlatform component is missing: "
            "${_platform_grouped_component_name} -> ${_platform_grouped_component_path}.")
    endif()
    if(IS_DIRECTORY
       "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/${_platform_grouped_component_name}")
        message(FATAL_ERROR
            "Legacy top-level SMRobotPlatform component directory was restored: "
            "${_platform_grouped_component_name}.")
    endif()
    string(REGEX MATCHALL
        "set\\([ \t\r\n]+_platform_component_source_directory_${_platform_grouped_component_name}[ \t\r\n]+[^\\)]*\\)"
        _platform_grouped_component_maps
        "${_platform_package_config_content}")
    list(LENGTH _platform_grouped_component_maps
        _platform_grouped_component_map_count)
    if(NOT _platform_grouped_component_map_count EQUAL 1)
        message(FATAL_ERROR
            "Grouped SMRobotPlatform component must have exactly one explicit "
            "source-directory map: ${_platform_grouped_component_name}; found "
            "${_platform_grouped_component_map_count}.")
    endif()
    list(GET _platform_grouped_component_maps 0
        _platform_grouped_component_map)
    string(FIND "${_platform_grouped_component_map}"
        "${_platform_grouped_component_path}"
        _platform_grouped_component_path_index)
    if(_platform_grouped_component_path_index EQUAL -1)
        message(FATAL_ERROR
            "Grouped SMRobotPlatform component source-directory map is incorrect: "
            "${_platform_grouped_component_name} must map to "
            "${_platform_grouped_component_path}.")
    endif()
endforeach()

message(STATUS
    "[Architecture] SMRobotPlatform physical families: 6; explicit component paths: 13")

# Stage 8 records a deliberate package-boundary decision. Re-evaluating it
# requires changing this gate together with the package DAG and delivery proof.
if(IS_DIRECTORY "${SMROBOT_SOURCE_ROOT}/SMRobotSimulation")
    message(FATAL_ERROR
        "SMRobotSimulation was introduced while the recorded package cycle still exists. "
        "Complete the stage-8 re-evaluation before creating the package.")
endif()

set(_simulation_package_dependency_evidence
    "SMRobotPlatform/Simulation/SimulationProject/TargetConfigSetting.cmake|SMRobotPlatform::AssetCore"
    "SMRobotPlatform/Simulation/SimulationProject/TargetConfigSetting.cmake|SMRobotPlatform::RobotPlatformAuthorization"
    "SMRobotPlatform/Simulation/SimulationRuntime/TargetConfigSetting.cmake|SMRobotPlatform::SimulationProject"
    "SMRobotPlatform/Simulation/SimulationRuntime/TargetConfigSetting.cmake|SMRobotPlatform::SensorCore"
    "SMRobotPlatform/Presentation/RobotRenderBridge/TargetConfigSetting.cmake|SMRobotPlatform::SimulationRuntime"
    "SMRobotPlatform/Sensors/SensorSimulation/TargetConfigSetting.cmake|SMRobotPlatform::SimulationRuntime"
    "SMRobotPlatform/Facades/ProjectSimulationSDK/TargetConfigSetting.cmake|SMRobotPlatform::SimulationRuntime"
    "SMRobotPlatform/Facades/VisualizationSDK/TargetConfigSetting.cmake|SMRobotPlatform::SimulationRuntime")
foreach(_simulation_package_dependency IN LISTS
        _simulation_package_dependency_evidence)
    string(REPLACE "|" ";" _simulation_package_dependency_fields
        "${_simulation_package_dependency}")
    list(GET _simulation_package_dependency_fields 0
        _simulation_package_dependency_file)
    list(GET _simulation_package_dependency_fields 1
        _simulation_package_dependency_target)
    file(READ "${SMROBOT_SOURCE_ROOT}/${_simulation_package_dependency_file}"
        _simulation_package_dependency_content)
    string(FIND "${_simulation_package_dependency_content}"
        "${_simulation_package_dependency_target}"
        _simulation_package_dependency_index)
    if(_simulation_package_dependency_index EQUAL -1)
        message(FATAL_ERROR
            "The recorded Simulation package DAG changed; re-run the stage-8 package review: "
            "${_simulation_package_dependency_file} no longer references "
            "${_simulation_package_dependency_target}.")
    endif()
endforeach()

message(STATUS
    "[Architecture] Simulation remains an SMRobotPlatform family; package-cycle evidence: enforced")

set(_sensor_core_cmake
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Sensors/SensorCore/CMakeLists.txt")
file(READ "${_sensor_core_cmake}" _sensor_core_cmake_content)
foreach(_sensor_internal_component IN ITEMS
        SensorContractCore
        SensorProcessingCore
        SensorIoCore)
    string(FIND "${_sensor_core_cmake_content}"
        "sensor_core_add_internal_component(${_sensor_internal_component}"
        _sensor_internal_component_index)
    if(_sensor_internal_component_index EQUAL -1)
        message(FATAL_ERROR
            "SensorCore internal component is missing: ${_sensor_internal_component}.")
    endif()
endforeach()

set(_sensor_pcd_compatibility_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Sensors/SensorSimulation/include/SensorSimulation/PcdPointCloudLoader.h")
file(READ "${_sensor_pcd_compatibility_header}" _sensor_pcd_compatibility_content)
if(NOT _sensor_pcd_compatibility_content MATCHES "deprecated")
    message(FATAL_ERROR
        "SensorSimulation PCD compatibility header must remain explicitly deprecated.")
endif()

set(_sensor_pcd_legacy_callers)
foreach(_sensor_consumer_root IN ITEMS
        SMRobotCore
        SMRobotPlatform
        SMRobotApps
        SMRobotMotionPlanning
        SMRobotSpray
        SimWorkbench)
    file(GLOB_RECURSE _sensor_consumer_sources LIST_DIRECTORIES FALSE
        "${SMROBOT_SOURCE_ROOT}/${_sensor_consumer_root}/*.h"
        "${SMROBOT_SOURCE_ROOT}/${_sensor_consumer_root}/*.hpp"
        "${SMROBOT_SOURCE_ROOT}/${_sensor_consumer_root}/*.cpp"
        "${SMROBOT_SOURCE_ROOT}/${_sensor_consumer_root}/*.cxx")
    foreach(_sensor_consumer_source IN LISTS _sensor_consumer_sources)
        file(TO_CMAKE_PATH "${_sensor_consumer_source}" _sensor_consumer_normalized)
        if(_sensor_consumer_normalized STREQUAL _sensor_pcd_compatibility_header
            OR _sensor_consumer_normalized MATCHES "${_smrobot_excluded_path_pattern}")
            continue()
        endif()
        file(READ "${_sensor_consumer_source}" _sensor_consumer_content)
        if(_sensor_consumer_content MATCHES
           "#[ \t]*include[ \t]*[<\"]SensorSimulation/PcdPointCloudLoader[.]h[>\"]")
            file(RELATIVE_PATH _sensor_consumer_relative
                "${SMROBOT_SOURCE_ROOT}" "${_sensor_consumer_source}")
            list(APPEND _sensor_pcd_legacy_callers "${_sensor_consumer_relative}")
        endif()
    endforeach()
endforeach()
if(_sensor_pcd_legacy_callers)
    list(JOIN _sensor_pcd_legacy_callers "\n  " _sensor_pcd_legacy_caller_text)
    message(FATAL_ERROR
        "Production code includes deprecated SensorSimulation PCD IO:\n  "
        "${_sensor_pcd_legacy_caller_text}")
endif()

message(STATUS
    "[Architecture] SensorCore internal boundaries: Contract, Processing, IO; deprecated production callers: 0")

set(_robot_render_bridge_cmake
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Presentation/RobotRenderBridge/CMakeLists.txt")
file(READ "${_robot_render_bridge_cmake}" _robot_render_bridge_cmake_content)
foreach(_render_bridge_internal_component IN ITEMS
        RobotVisualBridgeCore
        AttachmentVisualBridgeCore
        ProjectVisualBridgeCore
        CollisionVisualBridgeCore
        RenderBridgeMaterialCore)
    string(FIND "${_robot_render_bridge_cmake_content}"
        "robot_render_bridge_add_internal_component(\n    ${_render_bridge_internal_component}"
        _render_bridge_internal_component_index)
    if(_render_bridge_internal_component_index EQUAL -1)
        message(FATAL_ERROR
            "RobotRenderBridge internal component is missing: "
            "${_render_bridge_internal_component}.")
    endif()
endforeach()

string(REGEX MATCH
    "set\\(_project_visual_bridge_sources[ \t\r\n]+src/ProjectSceneRenderBridge[.]cpp[ \t\r\n]+\\)"
    _project_visual_bridge_source_match
    "${_robot_render_bridge_cmake_content}")
if(NOT _project_visual_bridge_source_match)
    message(FATAL_ERROR
        "ProjectSceneRenderBridge.cpp must remain owned by ProjectVisualBridgeCore.")
endif()

string(REGEX MATCH
    "set\\(_collision_visual_bridge_sources[ \t\r\n]+src/CollisionRenderBridge[.]cpp[ \t\r\n]+\\)"
    _collision_visual_bridge_source_match
    "${_robot_render_bridge_cmake_content}")
if(NOT _collision_visual_bridge_source_match)
    message(FATAL_ERROR
        "CollisionRenderBridge.cpp must remain owned by CollisionVisualBridgeCore.")
endif()

message(STATUS
    "[Architecture] RobotRenderBridge internal boundaries: Robot, Attachment, Project, Collision, Material")

set(_simulation_runtime_cmake
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationRuntime/CMakeLists.txt")
file(READ "${_simulation_runtime_cmake}" _simulation_runtime_cmake_content)
foreach(_simulation_runtime_internal_component IN ITEMS
        SimulationRuntimeEntityCore
        SimulationRuntimeCollisionCore
        SimulationRuntimeParallelCore
        SimulationRuntimeSelectionCore)
    string(FIND "${_simulation_runtime_cmake_content}"
        "simulation_runtime_add_internal_component(\n    ${_simulation_runtime_internal_component}"
        _simulation_runtime_internal_component_index)
    if(_simulation_runtime_internal_component_index EQUAL -1)
        message(FATAL_ERROR
            "SimulationRuntime internal component is missing: "
            "${_simulation_runtime_internal_component}.")
    endif()
endforeach()

foreach(_simulation_runtime_owned_source IN ITEMS
        "_simulation_runtime_entity_sources;src/ProjectSimulationRuntime.cpp"
        "_simulation_runtime_entity_sources;src/ProjectRuntimeQuery.cpp"
        "_simulation_runtime_collision_sources;src/ProjectCollisionRuntime.cpp"
        "_simulation_runtime_parallel_sources;src/ProjectParallelMechanismRuntime.cpp"
        "_simulation_runtime_selection_sources;src/ProjectSelectionState.cpp")
    list(GET _simulation_runtime_owned_source 0 _simulation_runtime_owner_list)
    list(GET _simulation_runtime_owned_source 1 _simulation_runtime_source)
    string(REGEX MATCH
        "set\\(${_simulation_runtime_owner_list}[^)]*${_simulation_runtime_source}[^)]*\\)"
        _simulation_runtime_owner_match
        "${_simulation_runtime_cmake_content}")
    if(NOT _simulation_runtime_owner_match)
        message(FATAL_ERROR
            "SimulationRuntime source owner changed: ${_simulation_runtime_source} must remain in "
            "${_simulation_runtime_owner_list}.")
    endif()
endforeach()

set(_runtime_query_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationRuntime/include/SimulationRuntime/ProjectRuntimeQuery.h")
file(READ "${_runtime_query_header}" _runtime_query_header_content)
if(_runtime_query_header_content MATCHES
   "#[ 	]*include[ 	]*[<\"](AssetCore|Collision|RenderCore|RobotCore|RobotInstance|SceneCore|SensorCore|SimulationProject)/")
    message(FATAL_ERROR
        "ProjectRuntimeQuery.h must remain independent of domain implementation headers.")
endif()

set(_project_document_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationProject/include/SimulationProject/ProjectDocument.h")
file(READ "${_project_document_header}" _project_document_header_content)
if(_project_document_header_content MATCHES
   "#[ 	]*include[ 	]*[<\"](AssetCore|RenderCore|SceneCore|SimulationRuntime)/")
    message(FATAL_ERROR
        "ProjectDocument.h must remain a persistence-only value contract.")
endif()

message(STATUS
    "[Architecture] SimulationRuntime internal boundaries: Entity, Collision, Parallel, Selection; neutral query contract")

foreach(_scene_overlay_pass IN ITEMS MeshPass PrimitivePass TrajectoryPass)
    set(_scene_overlay_pass_header
        "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Foundation/SceneCore/include/SceneCore/RenderPass/${_scene_overlay_pass}.h")
    file(READ "${_scene_overlay_pass_header}" _scene_overlay_pass_content)
    if(NOT _scene_overlay_pass_content MATCHES
       "#[ 	]*include[ 	]*[<\"]SceneCore/OverlayRenderConfig[.]h[>\"]")
        message(FATAL_ERROR
            "${_scene_overlay_pass}.h must consume the neutral OverlayRenderConfig contract.")
    endif()
    if(_scene_overlay_pass_content MATCHES
       "#[ 	]*include[ 	]*[<\"]SceneCore/CollisionOverlayRenderConfig[.]h[>\"]")
        message(FATAL_ERROR
            "${_scene_overlay_pass}.h must not directly include the collision compatibility config.")
    endif()
endforeach()

set(_project_scene_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotViewerCore/ProjectScene.cpp")
file(READ "${_project_scene_source}" _project_scene_source_content)
if(_project_scene_source_content MATCHES
   "SceneCore/CollisionOverlayRenderConfig[.]h|setCollisionOverlayConfig")
    message(FATAL_ERROR
        "RobotViewerCore production collision overlays must use RobotRenderBridge-owned filters.")
endif()

set(_collision_render_bridge_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Presentation/RobotRenderBridge/include/RobotRenderBridge/CollisionRenderBridge.h")
file(READ "${_collision_render_bridge_header}" _collision_render_bridge_header_content)
foreach(_collision_overlay_factory IN ITEMS
        geometryOverlayConfig
        primitiveOverlayConfig
        lineOverlayConfig)
    if(NOT _collision_render_bridge_header_content MATCHES "${_collision_overlay_factory}")
        message(FATAL_ERROR
            "CollisionRenderBridge must own ${_collision_overlay_factory}.")
    endif()
endforeach()

file(GLOB_RECURSE _scene_core_public_headers LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Foundation/SceneCore/include/SceneCore/*.h"
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Foundation/SceneCore/include/SceneCore/*.hpp")
foreach(_scene_core_public_header IN LISTS _scene_core_public_headers)
    file(READ "${_scene_core_public_header}" _scene_core_public_header_content)
    if(_scene_core_public_header_content MATCHES
       "#[ 	]*include[ 	]*[<\"](Collision|RobotCore|RobotInstance|SimulationProject|SimulationRuntime)/")
        file(RELATIVE_PATH _scene_core_public_relative
            "${SMROBOT_SOURCE_ROOT}" "${_scene_core_public_header}")
        message(FATAL_ERROR
            "SceneCore public header contains a domain implementation include: "
            "${_scene_core_public_relative}")
    endif()
endforeach()

message(STATUS
    "[Architecture] SceneCore overlays: neutral pass config; collision filter ownership in RobotRenderBridge")

set(_project_simulation_sdk_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Facades/ProjectSimulationSDK/include/ProjectSimulationSDK/ProjectSimulationSdk.h")
set(_project_simulation_sdk_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Facades/ProjectSimulationSDK/src/ProjectSimulationSdk.cpp")
set(_project_simulation_sdk_dependencies
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Facades/ProjectSimulationSDK/TargetConfigSetting.cmake")
file(READ "${_project_simulation_sdk_header}" _project_simulation_sdk_header_content)
file(READ "${_project_simulation_sdk_source}" _project_simulation_sdk_source_content)
file(READ "${_project_simulation_sdk_dependencies}" _project_simulation_sdk_dependencies_content)
if(_project_simulation_sdk_header_content MATCHES
   "#[ 	]*include[ 	]*[<\"](AssetCore|Collision|RenderCore|RobotCore|RobotInstance|SceneCore|SensorCore|SimulationProject|SimulationRuntime)/")
    message(FATAL_ERROR
        "ProjectSimulationSDK public contract must remain independent of implementation modules.")
endif()
foreach(_project_facade_contract IN ITEMS
        IProjectRuntimeSession
        RuntimeEntityRef
        RuntimeSummary
        createProjectRuntimeSession)
    if(NOT _project_simulation_sdk_header_content MATCHES "${_project_facade_contract}")
        message(FATAL_ERROR
            "ProjectSimulationSDK runtime facade contract is missing: ${_project_facade_contract}.")
    endif()
endforeach()
if(NOT _project_simulation_sdk_dependencies_content MATCHES
   "RequiredLibsPrivate[^)]*SMRobotPlatform::SimulationRuntime")
    message(FATAL_ERROR
        "ProjectSimulationSDK must consume SimulationRuntime as a private implementation dependency.")
endif()
if(NOT _project_simulation_sdk_source_content MATCHES
   "ProjectRuntimeQueryService")
    message(FATAL_ERROR
        "ProjectSimulationSDK runtime facade must map the authoritative runtime query service.")
endif()

set(_visualization_sdk_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Facades/VisualizationSDK/include/VisualizationSDK/ProjectVisualizationSession.h")
set(_visualization_sdk_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Facades/VisualizationSDK/src/ProjectVisualizationSession.cpp")
file(READ "${_visualization_sdk_header}" _visualization_sdk_header_content)
file(READ "${_visualization_sdk_source}" _visualization_sdk_source_content)
foreach(_visualization_facade_contract IN ITEMS
        VisualizationResult
        VisualizationSessionState
        buildSession
        syncSession
        lastError
        getVisualizationSdkAbiVersion)
    if(NOT _visualization_sdk_header_content MATCHES "${_visualization_facade_contract}")
        message(FATAL_ERROR
            "VisualizationSDK lifecycle contract is missing: ${_visualization_facade_contract}.")
    endif()
endforeach()
foreach(_visualization_compatibility_contract IN ITEMS
        "simulation_runtime::Result build("
        "simulation_runtime::Result sync(")
    string(FIND "${_visualization_sdk_header_content}"
        "${_visualization_compatibility_contract}"
        _visualization_compatibility_contract_index)
    if(_visualization_compatibility_contract_index EQUAL -1)
        message(FATAL_ERROR
            "VisualizationSDK compatibility API was removed: "
            "${_visualization_compatibility_contract}.")
    endif()
endforeach()
if(NOT _visualization_sdk_source_content MATCHES
   "collision debug overlay is not supported by the facade yet")
    message(FATAL_ERROR
        "VisualizationSDK must explicitly reject unsupported collision debug requests.")
endif()

message(STATUS
    "[Architecture] Facades: neutral Project runtime DTO; explicit Visualization lifecycle and errors")

file(GLOB_RECURSE _workbench_cmake_files LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/CMakeLists.txt")
file(GLOB_RECURSE _nested_workbench_cmake_files LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/*/CMakeLists.txt")
list(APPEND _workbench_cmake_files ${_nested_workbench_cmake_files})
list(REMOVE_DUPLICATES _workbench_cmake_files)

set(_cross_workbench_violations)
foreach(_file IN LISTS _workbench_cmake_files)
    file(TO_CMAKE_PATH "${_file}" _normalized_file)
    file(RELATIVE_PATH _relative_file "${SMROBOT_SOURCE_ROOT}" "${_normalized_file}")
    if(NOT _relative_file MATCHES "^SimWorkbench/SMRobotWorkbench([^/]+)/")
        continue()
    endif()
    set(_owner_package "${CMAKE_MATCH_1}")

    file(READ "${_file}" _content)
    string(REGEX MATCHALL "SMRobotWorkbench[A-Za-z0-9]+::[A-Za-z0-9]+" _dependencies "${_content}")
    foreach(_dependency IN LISTS _dependencies)
        if(NOT _dependency MATCHES "^SMRobotWorkbench([A-Za-z0-9]+)::")
            continue()
        endif()
        set(_dependency_package "${CMAKE_MATCH_1}")
        if(_dependency_package STREQUAL _owner_package OR _dependency_package STREQUAL "Common")
            continue()
        endif()

        list(APPEND _cross_workbench_violations
            "${_relative_file}: ${_dependency}")
    endforeach()
endforeach()

if(_cross_workbench_violations)
    list(JOIN _cross_workbench_violations "\n  " _violation_text)
    message(FATAL_ERROR
        "Workbench packages contain unapproved cross-package dependencies:\n  ${_violation_text}")
endif()

message(STATUS "[Architecture] Workbench dependencies: no cross-package references")

set(_viewport_services_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotQtModules/Shared/RobotQtViewerViewportServices.h")
file(READ "${_viewport_services_header}" _viewport_services_content)
string(REGEX MATCHALL "[\r\n][ \t]*virtual[ \t]" _viewport_virtual_declarations
    "${_viewport_services_content}")
list(LENGTH _viewport_virtual_declarations _viewport_virtual_count)
if(NOT _viewport_virtual_count EQUAL 67)
    message(FATAL_ERROR
        "RobotQtViewerViewportServices compatibility facade changed from its 67 virtual declarations "
        "to ${_viewport_virtual_count}. This installed compatibility API is frozen until a "
        "versioned public-API retirement is approved.")
endif()
message(STATUS
    "[Architecture] RobotQtViewerViewportServices virtual declarations: ${_viewport_virtual_count}/67")

set(_viewport_ports_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotQtModules/Shared/RobotQtViewerViewportPorts.h")
file(READ "${_viewport_ports_header}" _viewport_ports_content)

function(_smrobot_check_viewport_port class_name next_class_name ceiling)
    string(FIND "${_viewport_ports_content}" "class ${class_name}" _class_start)
    if(_class_start EQUAL -1)
        message(FATAL_ERROR "Missing viewport port ${class_name}.")
    endif()

    if(next_class_name STREQUAL "")
        string(LENGTH "${_viewport_ports_content}" _class_end)
    else()
        string(FIND "${_viewport_ports_content}" "class ${next_class_name}" _class_end)
        if(_class_end EQUAL -1)
            message(FATAL_ERROR
                "Missing viewport port boundary ${next_class_name} after ${class_name}.")
        endif()
    endif()
    math(EXPR _class_length "${_class_end} - ${_class_start}")
    string(SUBSTRING "${_viewport_ports_content}" ${_class_start} ${_class_length} _class_content)
    string(REGEX MATCHALL "[\r\n][ \t]*virtual[ \t]" _virtual_declarations "${_class_content}")
    list(LENGTH _virtual_declarations _virtual_count)
    if(_virtual_count GREATER ceiling)
        message(FATAL_ERROR
            "${class_name} grew beyond its phase-2 ceiling: ${_virtual_count}/${ceiling}.")
    endif()
    message(STATUS "[Architecture] ${class_name}: ${_virtual_count}/${ceiling}")
endfunction()

_smrobot_check_viewport_port(
    IRobotQtViewerDocumentViewportPort IRobotQtViewerSelectionViewportPort 2)
_smrobot_check_viewport_port(
    IRobotQtViewerSelectionViewportPort IRobotQtViewerAssemblyViewportPort 14)
_smrobot_check_viewport_port(
    IRobotQtViewerAssemblyViewportPort IRobotQtViewerCollisionViewportPort 20)
_smrobot_check_viewport_port(
    IRobotQtViewerCollisionViewportPort IRobotQtViewerVisualizationViewportPort 28)
_smrobot_check_viewport_port(
    IRobotQtViewerVisualizationViewportPort "" 5)

set(_legacy_viewport_compatibility_files
    "SMRobotApps/RobotQtModules/Shared/RobotQtViewerViewportServices.h"
    "SMRobotApps/RobotQtModules/Shared/RobotQtViewerDocumentContext.h"
    "SMRobotApps/RobotQtModules/Shared/RobotQtViewerDocumentContext.cpp"
    "SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/SceneExplorer/SceneExplorerModuleController.h"
    "SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/SceneExplorer/SceneExplorerModuleController.cpp"
)
file(GLOB_RECURSE _viewport_consumer_files LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/*.h"
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/*.cpp"
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/*.h"
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/*.cpp")
set(_legacy_viewport_violations)
foreach(_file IN LISTS _viewport_consumer_files)
    file(TO_CMAKE_PATH "${_file}" _normalized_file)
    if(_normalized_file MATCHES "${_smrobot_excluded_path_pattern}")
        continue()
    endif()
    file(RELATIVE_PATH _relative_file "${SMROBOT_SOURCE_ROOT}" "${_normalized_file}")
    if(_relative_file IN_LIST _legacy_viewport_compatibility_files)
        continue()
    endif()

    file(READ "${_file}" _content)
    if(_content MATCHES "RobotQtViewerViewportServices[ \t\r\n]*[&*]" OR
       _content MATCHES "viewportServices[ \t]*\\(")
        list(APPEND _legacy_viewport_violations "${_relative_file}")
    endif()
endforeach()
if(_legacy_viewport_violations)
    list(JOIN _legacy_viewport_violations "\n  " _violation_text)
    message(FATAL_ERROR
        "Production code still consumes the legacy viewport facade:\n  ${_violation_text}")
endif()
message(STATUS "[Architecture] Legacy viewport facade: compatibility files only")

set(_tool_setup_dir
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/ToolSetup")
set(_tool_setup_controller "${_tool_setup_dir}/ToolSetupModuleController.cpp")
file(STRINGS "${_tool_setup_controller}" _tool_setup_controller_lines)
list(LENGTH _tool_setup_controller_lines _tool_setup_controller_line_count)
if(_tool_setup_controller_line_count GREATER 1200)
    message(FATAL_ERROR
        "ToolSetupModuleController grew beyond the phase-7 facade ceiling: "
        "${_tool_setup_controller_line_count}/1200 lines.")
endif()
foreach(_task_source IN ITEMS
    ToolSetupTaskSessionCoordinator.cpp
    ToolSetupMountFrameTaskController.cpp
    ToolSetupInstalledDeviceTaskController.cpp
    ToolSetupAttachmentDefinitionTaskController.cpp
    ToolSetupObjectBindingTaskController.cpp)
    if(NOT EXISTS "${_tool_setup_dir}/${_task_source}")
        message(FATAL_ERROR "Missing Tool Setup task controller: ${_task_source}")
    endif()
endforeach()
file(READ "${_tool_setup_controller}" _tool_setup_controller_content)
if(_tool_setup_controller_content MATCHES
   "m_(hasMountEditSnapshot|mountEditSnapshotIsNew|mountFrameMode|activeMountEditId|mountDraftSourceRobotId|mountDraftSourceLinkName|pinnedRobotMountFrameIds|mountEditSnapshot)")
    message(FATAL_ERROR
        "ToolSetupModuleController still owns a phase-7 task-specific state field.")
endif()
message(STATUS
    "[Architecture] Tool Setup task controllers: 5 owners, facade "
    "${_tool_setup_controller_line_count}/1200 lines")

set(_main_window_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotQtViewer/MainWindow.cpp")
set(_main_window_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotQtViewer/MainWindow.h")
file(READ "${_main_window_source}" _main_window_content)
file(READ "${_main_window_header}" _main_window_header_content)
string(REGEX MATCHALL
    "new[ \t\r\n]+(robot_qt_viewer::)?(SceneExplorer|MotionControl|MotionPlanning|ToolSetup|CollisionWorkbench|CoatingAnalysis)[A-Za-z0-9_:]*"
    _main_window_workbench_constructions
    "${_main_window_content}")
list(LENGTH _main_window_workbench_constructions _main_window_workbench_construction_count)
if(NOT _main_window_workbench_construction_count EQUAL 0)
    message(FATAL_ERROR
        "MainWindow contains ${_main_window_workbench_construction_count} concrete Workbench "
        "construction points after the phase-4 owner-composition migration.")
endif()
message(STATUS
    "[Architecture] MainWindow concrete Workbench construction points: "
    "${_main_window_workbench_construction_count}/0")

set(_main_window_combined_content
    "${_main_window_header_content}\n${_main_window_content}")
set(_main_window_concrete_workbench_types
    SceneExplorerModuleController
    SceneExplorerWidget
    SceneExplorerTaskWidget
    ToolSetupModuleController
    ToolSetupWidget
    CollisionWorkbenchModuleController
    CollisionWorkbenchPanel
    MotionControlModuleController
    MotionControlWidget
    CollisionRuntimeResultsWidget
    MotionPlanningEditorWidget
    CoatingAnalysisModuleController
    CoatingAnalysisPanel
)
set(_main_window_concrete_workbench_violations)
foreach(_type IN LISTS _main_window_concrete_workbench_types)
    if(_main_window_combined_content MATCHES "(^|[^A-Za-z0-9_])${_type}([^A-Za-z0-9_]|$)")
        list(APPEND _main_window_concrete_workbench_violations "${_type}")
    endif()
endforeach()
if(_main_window_concrete_workbench_violations)
    list(JOIN _main_window_concrete_workbench_violations ", " _violation_text)
    message(FATAL_ERROR
        "MainWindow contains concrete Workbench Widget/Controller types after phase 4: "
        "${_violation_text}")
endif()
message(STATUS "[Architecture] MainWindow concrete Workbench types: 0")

if(_main_window_content MATCHES "switch[ \t\r\n]*\\([^\\)]*rightPanel" OR
   _main_window_content MATCHES "m_dynamicWorkbenchPanels" OR
   _main_window_content MATCHES "initializeWorkbenchLifecycles" OR
   _main_window_content MATCHES
       "make_unique[ \t\r\n]*<[ \t\r\n]*(robot_qt_viewer::)?[A-Za-z0-9_]*WorkbenchLifecycle")
    message(FATAL_ERROR
        "MainWindow bypasses the phase-3 Workbench runtime contribution host.")
endif()
if(NOT _main_window_content MATCHES "panelForWorkbench" OR
   NOT _main_window_content MATCHES "runtimeContributionFactories")
    message(FATAL_ERROR
        "MainWindow does not compose built-in and plugin panels through the contribution host.")
endif()
message(STATUS "[Architecture] MainWindow Workbench runtime paths use the contribution host")

set(_plugin_loader_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotQtModules/Shared/RobotQtViewerWorkbenchPlugin.cpp")
file(READ "${_plugin_loader_source}" _plugin_loader_content)
if(_plugin_loader_content MATCHES "bindWorkbenchLifecycle" OR
   _plugin_loader_content MATCHES "bindWorkbenchLanguageParticipant")
    message(FATAL_ERROR
        "Workbench plugin loader binds runtime pointers outside the contribution host.")
endif()
message(STATUS "[Architecture] Workbench plugin v1 runtime uses contribution factories")

function(_smrobot_extract_target_block file_path marker output_variable)
    file(READ "${file_path}" _content)
    string(FIND "${_content}" "${marker}" _block_start)
    if(_block_start EQUAL -1)
        message(FATAL_ERROR "Missing dependency block '${marker}' in ${file_path}.")
    endif()

    string(SUBSTRING "${_content}" ${_block_start} -1 _block_tail)
    string(FIND "${_block_tail}" ")" _block_end)
    if(_block_end EQUAL -1)
        message(FATAL_ERROR "Unterminated dependency block '${marker}' in ${file_path}.")
    endif()
    string(SUBSTRING "${_block_tail}" 0 ${_block_end} _block_content)
    string(REGEX MATCHALL "[A-Za-z0-9]+::[A-Za-z0-9]+" _targets "${_block_content}")
    list(REMOVE_DUPLICATES _targets)
    list(SORT _targets)
    set(${output_variable} "${_targets}" PARENT_SCOPE)
endfunction()

set(_robot_viewer_core_cmake
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotViewerCore/CMakeLists.txt")
set(_robot_viewer_core_target_config
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotViewerCore/TargetConfigSetting.cmake")
file(READ "${_robot_viewer_core_cmake}" _robot_viewer_core_cmake_content)
foreach(_metadata_reference IN ITEMS
        [=[${${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPublic}]=]
        [=[${${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPrivate}]=]
        [=[$<INSTALL_INTERFACE:${PACKAGE_NAME}/include/RobotViewerCore>]=])
    string(FIND "${_robot_viewer_core_cmake_content}"
        "${_metadata_reference}" _metadata_reference_index)
    if(_metadata_reference_index EQUAL -1)
        message(FATAL_ERROR
            "RobotViewerCore no longer consumes its TargetConfig metadata directly: "
            "missing ${_metadata_reference}.")
    endif()
endforeach()
_smrobot_extract_target_block(
    "${_robot_viewer_core_target_config}"
    [=[set(${TARGET_NAME}_RequiredLibsPublic]=]
    _robot_viewer_core_public_targets)
_smrobot_extract_target_block(
    "${_robot_viewer_core_target_config}"
    [=[set(${TARGET_NAME}_RequiredLibsPrivate]=]
    _robot_viewer_core_private_targets)
foreach(_target IN LISTS _robot_viewer_core_public_targets)
    if(_target IN_LIST _robot_viewer_core_private_targets)
        message(FATAL_ERROR
            "RobotViewerCore dependency is classified as both PUBLIC and PRIVATE: ${_target}.")
    endif()
endforeach()
message(STATUS
    "[Architecture] RobotViewerCore dependency metadata drift: 0/0")

string(REGEX MATCHALL
    "[$]<TARGET_PROPERTY:[$][{]required_target_[}],INTERFACE_[A-Z_]+>"
    _robot_viewer_core_property_forwarding
    "${_robot_viewer_core_cmake_content}")
list(REMOVE_DUPLICATES _robot_viewer_core_property_forwarding)
list(LENGTH _robot_viewer_core_property_forwarding
    _robot_viewer_core_property_forwarding_count)
if(NOT _robot_viewer_core_property_forwarding_count EQUAL 0)
    message(FATAL_ERROR
        "RobotViewerCore target-property forwarding must remain removed: "
        "${_robot_viewer_core_property_forwarding_count} occurrence(s).")
endif()
message(STATUS
    "[Architecture] RobotViewerCore manual target-property forwarding: "
    "${_robot_viewer_core_property_forwarding_count}/0")

set(_robot_viewer_core_consumer_cmake_files
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotViewerCore/regression/ProjectSceneSystemsTest/CMakeLists.txt"
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotViewerCore/regression/CollisionModelWorkflowSmokeTest/CMakeLists.txt"
    "${SMROBOT_SOURCE_ROOT}/tests/prebuilt_external_validation/robot_viewer_core_consumer/CMakeLists.txt")
foreach(_consumer_cmake IN LISTS _robot_viewer_core_consumer_cmake_files)
    file(READ "${_consumer_cmake}" _consumer_content)
    if(_consumer_content MATCHES "RobotViewerCore_REQUIRED_TARGETS")
        message(FATAL_ERROR
            "RobotViewerCore consumer compensates for facade dependencies: ${_consumer_cmake}.")
    endif()
    if(NOT _consumer_content MATCHES "SMRobotApps::RobotViewerCore")
        message(FATAL_ERROR
            "RobotViewerCore consumer does not link the facade target: ${_consumer_cmake}.")
    endif()
endforeach()

set(_robot_qt_viewer_target_config
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotQtViewer/TargetConfigSetting.cmake")
_smrobot_extract_target_block(
    "${_robot_qt_viewer_target_config}"
    [=[set(${TARGET_NAME}_RequiredLibsPublic]=]
    _robot_qt_viewer_direct_targets)
list(LENGTH _robot_qt_viewer_direct_targets _robot_qt_viewer_direct_target_count)
if(_robot_qt_viewer_direct_target_count GREATER 17)
    message(FATAL_ERROR
        "RobotQtViewer direct target closure grew beyond the stage-1 ceiling: "
        "${_robot_qt_viewer_direct_target_count}/17.")
endif()
message(STATUS
    "[Architecture] RobotQtViewer direct targets: "
    "${_robot_qt_viewer_direct_target_count}/17")

set(_robot_qt_viewer_cmake
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotQtViewer/CMakeLists.txt")
file(READ "${_robot_qt_viewer_cmake}" _robot_qt_viewer_cmake_content)
if(_robot_qt_viewer_cmake_content MATCHES
   "SMRobotWorkbenchProjectAssembly::ProjectAssemblyWorkbench")
    message(FATAL_ERROR
        "RobotQtViewer CMakeLists reintroduced a duplicate built-in Workbench target list.")
endif()
if(NOT _robot_qt_viewer_cmake_content MATCHES
   "SMROBOT_WORKBENCH_REGISTRAR_HEADER")
    message(FATAL_ERROR
        "RobotQtViewer no longer derives the built-in Workbench catalog from target metadata.")
endif()

set(_simulation_project_cmake
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationProject/CMakeLists.txt")
set(_simulation_project_target_config
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationProject/TargetConfigSetting.cmake")
file(READ "${_simulation_project_cmake}" _simulation_project_cmake_content)
file(READ "${_simulation_project_target_config}" _simulation_project_target_config_content)
if(_simulation_project_cmake_content MATCHES
   "file[(]GLOB[ \t\r\n]+SOURCES[ \t\r\n]+src/[*][.]cpp")
    message(FATAL_ERROR
        "SimulationProject restored a glob-owned production source list.")
endif()
if(NOT _simulation_project_cmake_content MATCHES
   "add_library[(][$][{]component_name[}][ \t\r\n]+OBJECT")
    message(FATAL_ERROR
        "SimulationProject internal component helper no longer creates OBJECT targets.")
endif()
set(_simulation_project_internal_targets
    SimulationProjectDocumentCore
    SimulationProjectAssemblyCore
    SimulationProjectIoCore
    SimulationProjectSessionCore
    SimulationProjectCompatibility)
foreach(_internal_target IN LISTS _simulation_project_internal_targets)
    string(REGEX MATCHALL
        "simulation_project_add_internal_component[(][ \t\r\n]*${_internal_target}"
        _internal_target_declarations
        "${_simulation_project_cmake_content}")
    list(LENGTH _internal_target_declarations _internal_target_declaration_count)
    if(NOT _internal_target_declaration_count EQUAL 1)
        message(FATAL_ERROR
            "SimulationProject internal target declaration drifted: "
            "${_internal_target}=${_internal_target_declaration_count}/1.")
    endif()
    string(FIND "${_simulation_project_cmake_content}"
        "$<TARGET_OBJECTS:${_internal_target}>" _internal_target_object_index)
    if(_internal_target_object_index EQUAL -1)
        message(FATAL_ERROR
            "SimulationProject shared facade no longer aggregates ${_internal_target}.")
    endif()
    string(FIND "${_simulation_project_target_config_content}"
        "${_internal_target}" _internal_target_export_index)
    if(NOT _internal_target_export_index EQUAL -1)
        message(FATAL_ERROR
            "SimulationProject internal target leaked into package metadata: ${_internal_target}.")
    endif()
endforeach()
message(STATUS
    "[Architecture] SimulationProject internal OBJECT components: 5/5")

string(REGEX MATCHALL
    "set[(]_simulation_project_session_sources[^)]*src/ProjectTransaction[.]cpp[^)]*[)]"
    _simulation_project_transaction_session_entries
    "${_simulation_project_cmake_content}")
list(LENGTH _simulation_project_transaction_session_entries
    _simulation_project_transaction_session_entry_count)
if(NOT _simulation_project_transaction_session_entry_count EQUAL 1)
    message(FATAL_ERROR
        "ProjectTransaction.cpp must be owned exactly once by SimulationProjectSessionCore: "
        "${_simulation_project_transaction_session_entry_count}/1.")
endif()
message(STATUS
    "[Architecture] SimulationProject transaction owner: SessionCore")

foreach(_io_source IN ITEMS
        ProjectIo.cpp
        ProjectIoSchemaCodec.cpp
        ProjectLoadMigration.cpp
        ProjectRobotPackageCodec.cpp
        ProjectRobotPackageRepository.cpp)
    string(REGEX MATCHALL
        "set[(]_simulation_project_io_sources[^)]*src/${_io_source}[^)]*[)]"
        _simulation_project_io_owner_entries
        "${_simulation_project_cmake_content}")
    list(LENGTH _simulation_project_io_owner_entries
        _simulation_project_io_owner_entry_count)
    if(NOT _simulation_project_io_owner_entry_count EQUAL 1)
        message(FATAL_ERROR
            "${_io_source} must be owned exactly once by SimulationProjectIoCore: "
            "${_simulation_project_io_owner_entry_count}/1.")
    endif()
endforeach()

string(REGEX MATCHALL
    "set[(]_simulation_project_document_sources[^)]*src/ProjectCollisionMigration[.]cpp[^)]*[)]"
    _simulation_project_collision_migration_owner_entries
    "${_simulation_project_cmake_content}")
list(LENGTH _simulation_project_collision_migration_owner_entries
    _simulation_project_collision_migration_owner_entry_count)
if(NOT _simulation_project_collision_migration_owner_entry_count EQUAL 1)
    message(FATAL_ERROR
        "ProjectCollisionMigration.cpp must be owned exactly once by "
        "SimulationProjectDocumentCore: "
        "${_simulation_project_collision_migration_owner_entry_count}/1.")
endif()

set(_project_io_repository_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationProject/src/ProjectIo.cpp")
file(READ "${_project_io_repository_source}" _project_io_repository_content)
foreach(_forbidden_project_io_implementation IN ITEMS
        "saveRobotPackageDocument("
        "importRobotPackageDocument("
        "readRobot("
        "readProjectAttachmentSet("
        "ProjectAssetStore::")
    string(FIND "${_project_io_repository_content}"
        "${_forbidden_project_io_implementation}"
        _forbidden_project_io_implementation_index)
    if(NOT _forbidden_project_io_implementation_index EQUAL -1)
        message(FATAL_ERROR
            "ProjectIo.cpp restored package/schema/asset implementation: "
            "${_forbidden_project_io_implementation}.")
    endif()
endforeach()

set(_project_session_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationProject/src/ProjectSession.cpp")
file(READ "${_project_session_source}" _project_session_content)
string(FIND "${_project_session_content}"
    "ensureCollisionDetectors(" _session_collision_migration_index)
if(NOT _session_collision_migration_index EQUAL -1)
    message(FATAL_ERROR
        "ProjectSession restored direct compatibility collision migration.")
endif()
foreach(_private_io_header IN ITEMS
        ProjectIoSchemaCodec.h
        ProjectLoadMigration.h
        ProjectRobotPackageCodec.h)
    string(FIND "${_simulation_project_target_config_content}"
        "${_private_io_header}" _private_io_header_export_index)
    if(NOT _private_io_header_export_index EQUAL -1)
        message(FATAL_ERROR
            "SimulationProject private IO header leaked into package metadata: "
            "${_private_io_header}.")
    endif()
endforeach()
message(STATUS
    "[Architecture] SimulationProject IO repository/migration ownership: enforced")

set(_document_controller_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotQtModules/Shared/RobotQtViewerDocumentController.cpp")
file(READ "${_document_controller_source}" _document_controller_content)
foreach(_forbidden_live_mutation IN ITEMS
        "ProjectDocumentService service(m_session.document())"
        "setDocument(previousDocument")
    string(FIND "${_document_controller_content}"
        "${_forbidden_live_mutation}" _forbidden_live_mutation_index)
    if(NOT _forbidden_live_mutation_index EQUAL -1)
        message(FATAL_ERROR
            "RobotQtViewerDocumentController restored live mutation/rollback: "
            "${_forbidden_live_mutation}.")
    endif()
endforeach()
message(STATUS
    "[Architecture] DocumentController candidate transaction path: enforced")

file(GLOB_RECURSE _transaction_app_workbench_production_files LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/*.h"
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/*.cpp"
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/*.h"
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/*.cpp")
set(_project_mutate_call_count 0)
foreach(_file IN LISTS _transaction_app_workbench_production_files)
    file(TO_CMAKE_PATH "${_file}" _normalized_file)
    if(_normalized_file MATCHES "${_smrobot_excluded_path_pattern}")
        continue()
    endif()
    file(READ "${_file}" _content)
    string(REGEX MATCHALL "[.]mutateProject[(]" _project_mutate_calls "${_content}")
    list(LENGTH _project_mutate_calls _file_mutate_call_count)
    math(EXPR _project_mutate_call_count
        "${_project_mutate_call_count} + ${_file_mutate_call_count}")
endforeach()
if(_project_mutate_call_count GREATER 59)
    message(FATAL_ERROR
        "Compatibility mutateProject() production calls grew beyond the stage-4 ceiling: "
        "${_project_mutate_call_count}/59.")
endif()
message(STATUS
    "[Architecture] Compatibility mutateProject() production calls: "
    "${_project_mutate_call_count}/59")

string(FIND "${_simulation_project_cmake_content}"
    "src/ProjectFrameBindingService.cpp" _frame_binding_service_index)
if(_frame_binding_service_index EQUAL -1)
    message(FATAL_ERROR
        "SimulationProject Assembly component no longer owns ProjectFrameBindingService.cpp.")
endif()
set(_object_binding_controller
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/ToolSetup/ToolSetupObjectBindingTaskController.cpp")
file(READ "${_object_binding_controller}" _object_binding_controller_content)
foreach(_forbidden_binding_implementation IN ITEMS
        "inverseTransform("
        "assetMountToVisual"
        "AttachmentAssetDesc"
        "MountedAttachmentDesc"
        ".bindAttachment("
        "makeAsciiSlug(")
    string(FIND "${_object_binding_controller_content}"
        "${_forbidden_binding_implementation}" _forbidden_binding_implementation_index)
    if(NOT _forbidden_binding_implementation_index EQUAL -1)
        message(FATAL_ERROR
            "Qt Object Binding controller reintroduced domain frame/descriptor logic: "
            "${_forbidden_binding_implementation}.")
    endif()
endforeach()
foreach(_required_binding_intent IN ITEMS
        "BindFramesRequest"
        ".bindFrames(")
    string(FIND "${_object_binding_controller_content}"
        "${_required_binding_intent}" _required_binding_intent_index)
    if(_required_binding_intent_index EQUAL -1)
        message(FATAL_ERROR
            "Qt Object Binding controller no longer submits typed frame intent: "
            "${_required_binding_intent}.")
    endif()
endforeach()
message(STATUS
    "[Architecture] Object binding frame alignment owned by SimulationProject Assembly")

set(_raw_document_compatibility_files
    "SMRobotApps/RobotQtModules/Shared/RobotQtViewerAppController.cpp"
    "SMRobotApps/RobotQtModules/Shared/SceneEntityWorkflowController.cpp"
    "SMRobotApps/RobotQtViewer/MainWindow.cpp"
    "SimWorkbench/SMRobotWorkbenchCollisionConfig/CollisionConfigWorkbench/CollisionDetectorConfig/CollisionDetectorWorkbenchDocumentFacade.cpp"
    "SimWorkbench/SMRobotWorkbenchCollisionConfig/CollisionConfigWorkbench/CollisionDetectorConfig/CollisionSelectionSetDocumentFacade.cpp"
    "SimWorkbench/SMRobotWorkbenchCollisionConfig/CollisionConfigWorkbench/CollisionLinkModelSetup/CollisionExportDocumentFacade.cpp"
    "SimWorkbench/SMRobotWorkbenchCollisionConfig/CollisionConfigWorkbench/CollisionLinkModelSetup/CollisionLinkModelVariantCommandController.cpp"
    "SimWorkbench/SMRobotWorkbenchCollisionConfig/CollisionConfigWorkbench/CollisionLinkModelSetup/CollisionRequestDocumentFacade.cpp"
    "SimWorkbench/SMRobotWorkbenchMotionPlanning/MotionPlanningWorkbench/Editor/MotionPlanningModuleController.cpp"
    "SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/ToolSetup/ToolSetupAttachmentDefinitionTaskController.cpp"
    "SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/ToolSetup/ToolSetupInstalledDeviceTaskController.cpp"
    "SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/ToolSetup/ToolSetupModuleController.cpp"
    "SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/ToolSetup/ToolSetupMountFrameTaskController.cpp"
    "SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench/ToolSetup/ToolSetupObjectBindingTaskController.cpp"
    "SimWorkbench/SMRobotWorkbenchRobotRun/RobotRunWorkbench/MotionControl/MotionControlModuleController.cpp")
file(GLOB_RECURSE _app_workbench_production_files LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/*.h"
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/*.cpp"
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/*.h"
    "${SMROBOT_SOURCE_ROOT}/SimWorkbench/*.cpp")
set(_raw_document_access_count 0)
set(_raw_document_access_files)
foreach(_file IN LISTS _app_workbench_production_files)
    file(TO_CMAKE_PATH "${_file}" _normalized_file)
    if(_normalized_file MATCHES "${_smrobot_excluded_path_pattern}")
        continue()
    endif()
    file(READ "${_file}" _content)
    string(REGEX MATCHALL "service[.]document[(][)]" _raw_document_accesses "${_content}")
    list(LENGTH _raw_document_accesses _file_access_count)
    if(_file_access_count EQUAL 0)
        continue()
    endif()
    math(EXPR _raw_document_access_count
        "${_raw_document_access_count} + ${_file_access_count}")
    file(RELATIVE_PATH _relative_file "${SMROBOT_SOURCE_ROOT}" "${_normalized_file}")
    if(NOT _relative_file IN_LIST _raw_document_compatibility_files)
        list(APPEND _raw_document_access_files "${_relative_file}")
    endif()
endforeach()
if(_raw_document_access_files)
    list(JOIN _raw_document_access_files "\n  " _raw_document_access_file_text)
    message(FATAL_ERROR
        "New production files access ProjectDocumentService::document():\n  "
        "${_raw_document_access_file_text}")
endif()
if(_raw_document_access_count GREATER 53)
    message(FATAL_ERROR
        "Raw ProjectDocumentService::document() calls grew beyond the stage-0 ceiling: "
        "${_raw_document_access_count}/53.")
endif()
message(STATUS
    "[Architecture] Raw service.document() production calls: "
    "${_raw_document_access_count}/53 across compatibility files")

file(GLOB_RECURSE _app_production_headers LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/*.h"
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/*.hpp")
set(_viewer_runtime_robot_owner_count 0)
set(_viewer_runtime_object_owner_count 0)
set(_viewer_runtime_builder_owner_count 0)
foreach(_file IN LISTS _app_production_headers)
    file(TO_CMAKE_PATH "${_file}" _normalized_file)
    if(_normalized_file MATCHES "${_smrobot_excluded_path_pattern}")
        continue()
    endif()
    file(READ "${_file}" _content)
    if(_content MATCHES "struct[ \t\r\n]+RuntimeRobot[ \t\r\n]*[{]")
        math(EXPR _viewer_runtime_robot_owner_count
            "${_viewer_runtime_robot_owner_count} + 1")
    endif()
    if(_content MATCHES "struct[ \t\r\n]+RuntimeSceneObject[ \t\r\n]*[{]")
        math(EXPR _viewer_runtime_object_owner_count
            "${_viewer_runtime_object_owner_count} + 1")
    endif()
    if(_content MATCHES "class[ \t\r\n]+ProjectRuntimeBuilder[ \t\r\n]*[{]")
        math(EXPR _viewer_runtime_builder_owner_count
            "${_viewer_runtime_builder_owner_count} + 1")
    endif()
endforeach()
if(_viewer_runtime_robot_owner_count GREATER 1 OR
   _viewer_runtime_object_owner_count GREATER 1 OR
   _viewer_runtime_builder_owner_count GREATER 1)
    message(FATAL_ERROR
        "Viewer project runtime owners grew beyond the stage-0 baseline: "
        "RuntimeRobot=${_viewer_runtime_robot_owner_count}/1, "
        "RuntimeSceneObject=${_viewer_runtime_object_owner_count}/1, "
        "ProjectRuntimeBuilder=${_viewer_runtime_builder_owner_count}/1.")
endif()
message(STATUS
    "[Architecture] Viewer project runtime compatibility owners: "
    "RuntimeRobot=${_viewer_runtime_robot_owner_count}/1, "
    "RuntimeSceneObject=${_viewer_runtime_object_owner_count}/1, "
    "ProjectRuntimeBuilder=${_viewer_runtime_builder_owner_count}/1")

set(_project_scene_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotViewerCore/ProjectScene.cpp")
file(READ "${_project_scene_source}" _project_scene_content)
foreach(_forbidden_project_scene_runtime_owner IN ITEMS
        "ProjectDocument projectDocument"
        "projectDocumentSet"
        "ProjectRuntimeBuilder::loadSingleRobot"
        "make_shared<robotinstance::RobotInstance>"
        "ProjectRuntimeBuilder::applyAutoMotion"
        "ProjectRuntimeBuilder::setJointValue"
        "ProjectRuntimeBuilder::getJointValue")
    string(FIND "${_project_scene_content}"
        "${_forbidden_project_scene_runtime_owner}"
        _forbidden_project_scene_runtime_owner_index)
    if(NOT _forbidden_project_scene_runtime_owner_index EQUAL -1)
        message(FATAL_ERROR
            "ProjectScene restored document/runtime domain ownership: "
            "${_forbidden_project_scene_runtime_owner}.")
    endif()
endforeach()
foreach(_required_project_scene_runtime_path IN ITEMS
        "ProjectSimulationRuntime"
        "ProjectParallelMechanismRuntime"
        "ProjectCollisionQueryService"
        "ProjectScenePreviewOverlayState")
    string(FIND "${_project_scene_content}"
        "${_required_project_scene_runtime_path}"
        _required_project_scene_runtime_path_index)
    if(_required_project_scene_runtime_path_index EQUAL -1)
        message(FATAL_ERROR
            "ProjectScene no longer uses the converged runtime/preview path: "
            "${_required_project_scene_runtime_path}.")
    endif()
endforeach()

foreach(_forbidden_project_scene_collision_query IN ITEMS
        ".checkCollision("
        ".distance(")
    string(FIND "${_project_scene_content}"
        "${_forbidden_project_scene_collision_query}"
        _forbidden_project_scene_collision_query_index)
    if(NOT _forbidden_project_scene_collision_query_index EQUAL -1)
        message(FATAL_ERROR
            "ProjectScene bypassed SimulationRuntime collision query ownership: "
            "${_forbidden_project_scene_collision_query}.")
    endif()
endforeach()

set(_public_compatibility_source_roots
    Common
    SMRobotCore
    SMRobotPlatform
    SMRobotApps
    SMRobotMotionPlanning
    SMRobotSpray
    SimWorkbench
    tests)
set(_public_compatibility_callers)
foreach(_public_compatibility_source_root IN LISTS
        _public_compatibility_source_roots)
    foreach(_extension IN LISTS _smrobot_production_extensions)
        file(GLOB_RECURSE _public_compatibility_extension_files
            LIST_DIRECTORIES FALSE
            "${SMROBOT_SOURCE_ROOT}/${_public_compatibility_source_root}/${_extension}")
        list(APPEND _public_compatibility_callers
            ${_public_compatibility_extension_files})
    endforeach()
endforeach()
list(REMOVE_DUPLICATES _public_compatibility_callers)

set(_public_compatibility_forbidden_calls
    "ProjectRuntimeBuilder::loadSingleRobot"
    "ProjectRuntimeBuilder::applyInitialJoints"
    "ProjectRuntimeBuilder::setJointValue"
    "ProjectRuntimeBuilder::getJointValue"
    "ProjectRuntimeBuilder::applyAutoMotion"
    ".dirtyFlag("
    "->dirtyFlag("
    ".requiresSaveAsFlag("
    "->requiresSaveAsFlag("
    "ModelManager::instance().loadModel("
    "ModelManager::instance().clear(")
set(_public_compatibility_violation_count 0)
foreach(_public_compatibility_caller IN LISTS _public_compatibility_callers)
    file(TO_CMAKE_PATH "${_public_compatibility_caller}"
        _public_compatibility_normalized_caller)
    if(_public_compatibility_normalized_caller MATCHES
            "/(ProjectRuntimeBuilder|ProjectSession|ModelManager)\\.(cpp|h)$")
        continue()
    endif()

    file(READ "${_public_compatibility_caller}"
        _public_compatibility_caller_content)
    foreach(_public_compatibility_forbidden_call IN LISTS
            _public_compatibility_forbidden_calls)
        string(FIND "${_public_compatibility_caller_content}"
            "${_public_compatibility_forbidden_call}"
            _public_compatibility_forbidden_call_index)
        if(NOT _public_compatibility_forbidden_call_index EQUAL -1)
            math(EXPR _public_compatibility_violation_count
                "${_public_compatibility_violation_count} + 1")
            message(SEND_ERROR
                "Deprecated public compatibility API regained a repository caller: "
                "${_public_compatibility_forbidden_call} in "
                "${_public_compatibility_caller}.")
        endif()
    endforeach()
endforeach()
if(_public_compatibility_violation_count GREATER 0)
    message(FATAL_ERROR
        "Deprecated public compatibility APIs have "
        "${_public_compatibility_violation_count} repository caller(s).")
endif()

foreach(_retired_project_example IN ITEMS
        "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationRuntime/regression/HeadlessProjectCollisionExample/CMakeLists.txt"
        "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Presentation/RobotRenderBridge/feature_probes/ProjectGlfwViewer/CMakeLists.txt")
    if(EXISTS "${_retired_project_example}")
        message(FATAL_ERROR
            "Retired project example CMake definition was restored without a maintained entry point: "
            "${_retired_project_example}.")
    endif()
endforeach()
message(STATUS
    "[Architecture] Deprecated public compatibility API callers: 0; retired project examples: absent")

set(_collision_public_api_manifest
    "${SMROBOT_SOURCE_ROOT}/SMRobotCore/Collision/CollisionPublicApiManifest.cmake")
if(NOT EXISTS "${_collision_public_api_manifest}")
    message(FATAL_ERROR
        "Collision public API manifest is missing: ${_collision_public_api_manifest}")
endif()
include("${_collision_public_api_manifest}")

set(_collision_manifest_headers)
foreach(_collision_manifest_header IN LISTS COLLISION_PUBLIC_HEADER_INVENTORY)
    list(FIND _collision_manifest_headers "${_collision_manifest_header}"
        _collision_manifest_duplicate_index)
    if(NOT _collision_manifest_duplicate_index EQUAL -1)
        message(FATAL_ERROR
            "Collision public API manifest contains a duplicate header: "
            "${_collision_manifest_header}")
    endif()

    if(NOT EXISTS
       "${SMROBOT_SOURCE_ROOT}/SMRobotCore/Collision/include/${_collision_manifest_header}")
        message(FATAL_ERROR
            "Collision public API manifest references a missing header: "
            "${_collision_manifest_header}")
    endif()
    list(APPEND _collision_manifest_headers "${_collision_manifest_header}")
endforeach()

file(GLOB_RECURSE _collision_public_headers LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SMRobotCore/Collision/include/*.h"
    "${SMROBOT_SOURCE_ROOT}/SMRobotCore/Collision/include/*.hpp")
set(_collision_actual_headers)
foreach(_collision_public_header IN LISTS _collision_public_headers)
    file(RELATIVE_PATH _collision_public_header_relative
        "${SMROBOT_SOURCE_ROOT}/SMRobotCore/Collision/include"
        "${_collision_public_header}")
    file(TO_CMAKE_PATH "${_collision_public_header_relative}"
        _collision_public_header_relative)
    list(APPEND _collision_actual_headers "${_collision_public_header_relative}")

    file(READ "${_collision_public_header}" _collision_public_header_content)
    if(_collision_public_header_content MATCHES "fcl[/\\\\]|fcl::")
        message(FATAL_ERROR
            "Collision public header leaks the FCL backend: ${_collision_public_header}")
    endif()
endforeach()

list(SORT _collision_manifest_headers)
list(SORT _collision_actual_headers)
if(NOT _collision_manifest_headers STREQUAL _collision_actual_headers)
    message(FATAL_ERROR
        "Collision public header inventory differs from include/Collision.\n"
        "Manifest: ${_collision_manifest_headers}\n"
        "Actual: ${_collision_actual_headers}")
endif()
list(LENGTH _collision_actual_headers _collision_public_header_count)
message(STATUS
    "[Architecture] Collision public header inventory: ${_collision_public_header_count} headers")

set(_collision_private_header_names
    PointCloudVoxelGrid.h
    CollisionDetectorRuntime.h
    CollisionDetectorRegistry.h)
foreach(_collision_private_header_name IN LISTS _collision_private_header_names)
    if(EXISTS
       "${SMROBOT_SOURCE_ROOT}/SMRobotCore/Collision/include/Collision/${_collision_private_header_name}")
        message(FATAL_ERROR
            "Collision private header leaked back into the public include tree: "
            "${_collision_private_header_name}")
    endif()

    foreach(_collision_public_header IN LISTS _collision_public_headers)
        file(READ "${_collision_public_header}" _collision_public_header_content)
        if(_collision_public_header_content MATCHES "${_collision_private_header_name}")
            message(FATAL_ERROR
                "Collision public header references private compatibility header "
                "${_collision_private_header_name}: ${_collision_public_header}")
        endif()
    endforeach()
endforeach()
message(STATUS
    "[Architecture] Collision detector runtime/registry and voxel grid headers are private")

file(GLOB_RECURSE _collision_backend_interface_headers LIST_DIRECTORIES FALSE
    "${SMROBOT_SOURCE_ROOT}/SMRobotCore/Collision/*/ICollisionBackend.h")
list(LENGTH _collision_backend_interface_headers _collision_backend_interface_count)
if(NOT _collision_backend_interface_count EQUAL 1)
    message(FATAL_ERROR
        "Collision must define exactly one ICollisionBackend header; found "
        "${_collision_backend_interface_count}: ${_collision_backend_interface_headers}")
endif()
file(TO_CMAKE_PATH "${_collision_backend_interface_headers}"
    _collision_backend_interface_header_normalized)
if(NOT _collision_backend_interface_header_normalized MATCHES
   "/SMRobotCore/Collision/include/Collision/ICollisionBackend.h$")
    message(FATAL_ERROR
        "ICollisionBackend is not owned by the Collision public interface: "
        "${_collision_backend_interface_header_normalized}")
endif()

set(_collision_consumer_roots
    SMRobotCore
    SMRobotPlatform
    SMRobotApps
    SMRobotMotionPlanning
    SMRobotSpray
    SimWorkbench)
foreach(_collision_consumer_root IN LISTS _collision_consumer_roots)
    file(GLOB_RECURSE _collision_consumer_sources LIST_DIRECTORIES FALSE
        "${SMROBOT_SOURCE_ROOT}/${_collision_consumer_root}/*.h"
        "${SMROBOT_SOURCE_ROOT}/${_collision_consumer_root}/*.hpp"
        "${SMROBOT_SOURCE_ROOT}/${_collision_consumer_root}/*.cpp"
        "${SMROBOT_SOURCE_ROOT}/${_collision_consumer_root}/*.cxx")
    foreach(_collision_consumer_source IN LISTS _collision_consumer_sources)
        file(TO_CMAKE_PATH "${_collision_consumer_source}" _collision_consumer_normalized)
        if(_collision_consumer_normalized MATCHES "/(regression|feature_probes)/")
            continue()
        endif()
        file(READ "${_collision_consumer_source}" _collision_consumer_content)
        if(_collision_consumer_content MATCHES "fcl[/\\\\]|fcl::")
            if(NOT _collision_consumer_normalized MATCHES
               "/SMRobotCore/Collision/src/FclCollisionBackend\\.(h|cpp)$")
                message(FATAL_ERROR
                    "Production code uses FCL outside FclCollisionBackend: "
                    "${_collision_consumer_source}")
            endif()
        endif()
    endforeach()
endforeach()
message(STATUS
    "[Architecture] One ICollisionBackend interface; FCL is isolated to FclCollisionBackend")

set(_parallel_runtime_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationRuntime/include/SimulationRuntime/ProjectParallelMechanismRuntime.h")
set(_parallel_runtime_source
    "${SMROBOT_SOURCE_ROOT}/SMRobotPlatform/Simulation/SimulationRuntime/src/ProjectParallelMechanismRuntime.cpp")
file(READ "${_parallel_runtime_header}" _parallel_runtime_header_content)
file(READ "${_parallel_runtime_source}" _parallel_runtime_source_content)
set(_parallel_runtime_content
    "${_parallel_runtime_header_content}\n${_parallel_runtime_source_content}")
foreach(_forbidden_parallel_dependency IN ITEMS
        "RenderCore/"
        "SceneCore/"
        "RobotRenderBridge/"
        "VisualizationSDK/"
        "glad/"
        "GL/"
        "Qt"
        "QWidget")
    string(FIND "${_parallel_runtime_content}"
        "${_forbidden_parallel_dependency}"
        _forbidden_parallel_dependency_index)
    if(NOT _forbidden_parallel_dependency_index EQUAL -1)
        message(FATAL_ERROR
            "ProjectParallelMechanismRuntime depends on presentation/UI code: "
            "${_forbidden_parallel_dependency}.")
    endif()
endforeach()

set(_viewer_runtime_types_header
    "${SMROBOT_SOURCE_ROOT}/SMRobotApps/RobotViewerCore/ProjectRuntimeTypes.h")
file(READ "${_viewer_runtime_types_header}" _viewer_runtime_types_content)
foreach(_required_runtime_reference IN ITEMS
        "ProjectParallelRobotState* parallelState"
        "robot::RobotModel& model"
        "std::shared_ptr<robotinstance::RobotInstance>& instance"
        "bool& parallelControlEnabled")
    string(FIND "${_viewer_runtime_types_content}"
        "${_required_runtime_reference}"
        _required_runtime_reference_index)
    if(_required_runtime_reference_index EQUAL -1)
        message(FATAL_ERROR
            "Viewer robot presentation no longer references unified runtime state: "
            "${_required_runtime_reference}.")
    endif()
endforeach()

foreach(_private_viewer_header IN ITEMS
        ProjectScenePreviewOverlayState.h
        ProjectSceneStewartPresentationSystem.h)
    string(FIND "${_robot_viewer_core_cmake_content}"
        "install(FILES\n        ${_private_viewer_header}"
        _private_viewer_header_install_index)
    if(NOT _private_viewer_header_install_index EQUAL -1)
        message(FATAL_ERROR
            "RobotViewerCore private presentation header leaked into install surface: "
            "${_private_viewer_header}.")
    endif()
endforeach()
message(STATUS
    "[Architecture] SimulationRuntime/ProjectScene convergence ownership: enforced")

message(STATUS "[Architecture] software module boundary checks passed")
