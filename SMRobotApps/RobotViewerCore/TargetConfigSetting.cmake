##########################################################################
# CMake requirement
##########################################################################
cmake_minimum_required(VERSION 3.20)

set(${TARGET_NAME}_RequiredLibsPublic
    Qt5::Widgets
    Eigen3::Eigen
    glm::glm
    SMRobotCore::Collision
    SMRobotCore::Kinematics
    SMRobotPlatform::RenderCore
    SMRobotCore::RobotCore
    SMRobotCore::RobotInstance
    SMRobotPlatform::RobotRenderBridge
    SMRobotPlatform::SceneCore
    SMRobotPlatform::SimulationProject
    SMRobotPlatform::SimulationRuntime
    SMRobotPlatform::VisualizationSDK
)

set(${TARGET_NAME}_RequiredLibsPrivate
    Common::GLRuntime
    Common::CustomLog
    Common::Utility
    SMRobotPlatform::AssetCore
    SMRobotPlatform::CameraCore
    SMRobotCore::RobotIO
    SMRobotPlatform::SensorCore
    SMRobotPlatform::SensorSimulation
)
