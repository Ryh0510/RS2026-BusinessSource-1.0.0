cmake_minimum_required(VERSION 3.20)

set(${TARGET_NAME}_RequiredLibsPublic
    Qt5::Widgets
    Eigen3::Eigen
    SMRobotCore::RobotIO
    SMRobotPlatform::VisualizationSDK
    Common::Utility
    SMRobotPlatform::SimulationProject
    SMRobotPlatform::SimulationRuntime)
set(${TARGET_NAME}_RequiredLibsPrivate Common::CustomLog)
