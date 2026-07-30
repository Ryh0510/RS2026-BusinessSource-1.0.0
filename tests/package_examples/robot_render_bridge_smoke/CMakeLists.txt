cmake_minimum_required(VERSION 3.20)

project(SMRobotGen2RobotRenderBridgeSmoke LANGUAGES CXX)

find_package(SMRobotGen2 CONFIG REQUIRED COMPONENTS
    RobotRenderBridge
    RobotCore
    RobotRuntime
    RobotInstance
    SceneCore
)

add_executable(SMRobotGen2RobotRenderBridgeSmoke main.cpp)
target_compile_features(SMRobotGen2RobotRenderBridgeSmoke PRIVATE cxx_std_17)
target_link_libraries(SMRobotGen2RobotRenderBridgeSmoke PRIVATE
    SMRobotGen2::RobotRenderBridge
    SMRobotGen2::RobotCore
    SMRobotGen2::RobotRuntime
    SMRobotGen2::RobotInstance
    SMRobotGen2::SceneCore
)

