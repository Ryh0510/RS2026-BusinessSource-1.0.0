cmake_minimum_required(VERSION 3.20)

project(SMRobotPlatformComponentsSmoke LANGUAGES CXX)

include(CTest)

find_package(SMRobotPlatform CONFIG REQUIRED COMPONENTS
    AssetCore
    CameraCore
    RenderCore
    RobotRenderBridge
    SceneCore
    SensorCore
    SensorSimulation
    SimulationProject
    SimulationRuntime
    VisualizationSDK
)

add_executable(SMRobotPlatformComponentsSmoke main.cpp)
target_compile_features(SMRobotPlatformComponentsSmoke PRIVATE cxx_std_17)
target_link_libraries(SMRobotPlatformComponentsSmoke PRIVATE
    SMRobotPlatform::AssetCore
    SMRobotPlatform::CameraCore
    SMRobotPlatform::RenderCore
    SMRobotPlatform::RobotRenderBridge
    SMRobotPlatform::SceneCore
    SMRobotPlatform::SensorCore
    SMRobotPlatform::SensorSimulation
    SMRobotPlatform::SimulationProject
    SMRobotPlatform::SimulationRuntime
    SMRobotPlatform::VisualizationSDK
)

add_custom_command(TARGET SMRobotPlatformComponentsSmoke POST_BUILD
    COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:SMRobotPlatformComponentsSmoke>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:SMRobotPlatformComponentsSmoke>;$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::AssetCore>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::CameraCore>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::RenderCore>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::RobotRenderBridge>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::SceneCore>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::SensorCore>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::SensorSimulation>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::SimulationProject>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::SimulationRuntime>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::VisualizationSDK>" "$<TARGET_FILE_DIR:SMRobotPlatformComponentsSmoke>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying SMRobotPlatformComponentsSmoke runtime DLLs"
)

if(BUILD_TESTING)
    add_test(
        NAME SMRobotPlatform.AssetCoreSmoke
        COMMAND SMRobotPlatformComponentsSmoke AssetCore
    )
    add_test(
        NAME SMRobotPlatform.CameraCoreSmoke
        COMMAND SMRobotPlatformComponentsSmoke CameraCore
    )
    add_test(
        NAME SMRobotPlatform.SensorCoreSmoke
        COMMAND SMRobotPlatformComponentsSmoke SensorCore
    )
    add_test(
        NAME SMRobotPlatform.SimulationProjectSmoke
        COMMAND SMRobotPlatformComponentsSmoke SimulationProject
    )
    add_test(
        NAME SMRobotPlatform.SimulationRuntimeSmoke
        COMMAND SMRobotPlatformComponentsSmoke SimulationRuntime
    )
    add_test(
        NAME SMRobotPlatform.SensorSimulationSmoke
        COMMAND SMRobotPlatformComponentsSmoke SensorSimulation
    )
    add_test(
        NAME SMRobotPlatform.RenderCoreSmoke
        COMMAND SMRobotPlatformComponentsSmoke RenderCore
    )
    add_test(
        NAME SMRobotPlatform.SceneCoreSmoke
        COMMAND SMRobotPlatformComponentsSmoke SceneCore
    )
    add_test(
        NAME SMRobotPlatform.RobotRenderBridgeSmoke
        COMMAND SMRobotPlatformComponentsSmoke RobotRenderBridge
    )
    add_test(
        NAME SMRobotPlatform.VisualizationSDKSmoke
        COMMAND SMRobotPlatformComponentsSmoke VisualizationSDK
    )
endif()
