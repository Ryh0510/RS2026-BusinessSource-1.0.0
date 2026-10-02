cmake_minimum_required(VERSION 3.20)

set(${TARGET_NAME}_RequiredLibsPublic
    SMRobotWorkbenchCommon::WorkbenchCommon
    SMRobotWorkbenchCommon::CollisionRuntimeResultsView
    SMRobotWorkbenchRobotRun::RobotRunMotionControl
)

set(${TARGET_NAME}_RequiredLibsPrivate)
