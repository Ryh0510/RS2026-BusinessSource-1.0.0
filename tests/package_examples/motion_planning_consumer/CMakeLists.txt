cmake_minimum_required(VERSION 3.20)

project(MotionPlanningConsumer LANGUAGES CXX)

find_package(SMRobotMotionPlanning CONFIG REQUIRED COMPONENTS MotionPlanningCore)
find_package(SMRobotCore CONFIG REQUIRED COMPONENTS RobotRuntime)
find_package(SMRobotPlatform CONFIG REQUIRED COMPONENTS SimulationProject)

add_executable(MotionPlanningConsumer main.cpp)
target_link_libraries(MotionPlanningConsumer
    PRIVATE
        SMRobotMotionPlanning::MotionPlanningCore
        SMRobotCore::RobotRuntime
        SMRobotPlatform::SimulationProject
)
target_compile_features(MotionPlanningConsumer PRIVATE cxx_std_17)
