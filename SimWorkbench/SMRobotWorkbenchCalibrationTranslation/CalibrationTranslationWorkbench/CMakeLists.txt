cmake_minimum_required(VERSION 3.20)

set(TARGET_NAME CalibrationTranslationWorkbench)
file(GLOB TRANSLATION_WIDGETS CONFIGURE_DEPENDS Widgets/*.cpp Widgets/*.h Models/*.cpp Models/*.h)
add_library(${TARGET_NAME} STATIC
    ${TRANSLATION_WIDGETS}
    CalibrationTranslationModuleController.cpp
    CalibrationTranslationModuleController.h
    CalibrationTranslationWorkbenchContribution.cpp
    CalibrationTranslationWorkbenchContribution.h
)
add_library(SMRobotWorkbenchCalibrationTranslation::${TARGET_NAME} ALIAS ${TARGET_NAME})
target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)
target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${PACKAGE_NAME}/include/${TARGET_NAME}>
)
target_link_libraries(${TARGET_NAME}
    PUBLIC
        SMRobotWorkbenchCommon::WorkbenchCommon
        SMRobotSpray::CalibrationInstructionTranslation
    PRIVATE Qt5::Widgets SMRobotMotionPlanning::MotionPlanningIk SMRobotMotionPlanning::PostModel RobotViewerCore
)
set_target_properties(${TARGET_NAME} PROPERTIES
    FOLDER SMRobotWorkbenchCalibrationTranslation
    AUTOMOC ON
    SMROBOT_WORKBENCH_REGISTRAR_HEADER "CalibrationTranslationWorkbenchContribution.h"
    SMROBOT_WORKBENCH_REGISTRAR_FUNCTION "robot_qt_viewer::registerCalibrationTranslationWorkbenchContribution"
    EXPORT_PROPERTIES "SMROBOT_WORKBENCH_REGISTRAR_HEADER;SMROBOT_WORKBENCH_REGISTRAR_FUNCTION"
)

if(MSVC)
 target_compile_options(${TARGET_NAME} PRIVATE /utf-8)
endif()
