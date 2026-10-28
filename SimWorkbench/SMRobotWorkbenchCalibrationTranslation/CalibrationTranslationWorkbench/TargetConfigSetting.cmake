cmake_minimum_required(VERSION 3.20)

set(${TARGET_NAME}_RequiredLibsPublic
    SMRobotWorkbenchCommon::WorkbenchCommon
    SMRobotSpray::CalibrationInstructionTranslation
)
set(${TARGET_NAME}_RequiredLibsPrivate Qt5::Widgets)
