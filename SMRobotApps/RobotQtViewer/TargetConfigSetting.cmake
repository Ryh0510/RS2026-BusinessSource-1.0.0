##########################################################################
# CMake requirement
##########################################################################
cmake_minimum_required(VERSION 3.20)

set(${TARGET_NAME}_RequiredLibsPublic
    Qt5::Widgets
    Common::CustomLog
    Common::Utility
    SMRobotCore::RobotIO
    SMRobotCore::RobotSDK
    SMRobotCore::RobotRuntime
    SMRobotPlatform::SimulationProject
    SMRobotApps::RobotViewerCore
    SMRobotWorkbenchCommon::RobotQtModulesCoreWidgets
    SMRobotWorkbenchCommon::RobotQtModulesShared
    SMRobotWorkbenchProjectAssembly::ProjectAssemblyWorkbench
    SMRobotWorkbenchCollisionConfig::CollisionConfigWorkbench
    SMRobotWorkbenchRobotRun::RobotRunWorkbench
    SMRobotWorkbenchMotionPlanning::MotionPlanningWorkbench
    SMRobotWorkbenchPaintingAnalysis::PaintingAnalysisWorkbench
    SMRobotWorkbenchSprayProcess::SprayProcessWorkbench
    SMRobotWorkbenchDigitalTwin::DigitalTwinWorkbench
)

set(${TARGET_NAME}_RequiredLibsPrivate)
