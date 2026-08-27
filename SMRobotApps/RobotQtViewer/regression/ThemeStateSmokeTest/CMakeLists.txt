cmake_minimum_required(VERSION 3.20)

set(TEST_TARGET_NAME RobotQtViewer-ThemeStateSmoke)
set(ROBOT_QT_VIEWER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(${TEST_TARGET_NAME}
    main.cpp
    "${ROBOT_QT_VIEWER_SOURCE_DIR}/RobotQtViewerTheme.cpp"
    "${ROBOT_QT_VIEWER_SOURCE_DIR}/RobotQtViewerTheme.h"
    "${ROBOT_QT_VIEWER_SOURCE_DIR}/RobotQtViewerResources.qrc"
)

target_include_directories(${TEST_TARGET_NAME}
    PRIVATE
        "${ROBOT_QT_VIEWER_SOURCE_DIR}"
)

target_link_libraries(${TEST_TARGET_NAME}
    PRIVATE
        Qt5::Widgets
        SMRobotWorkbenchCommon::RobotQtModulesCoreWidgets
)
if(WIN32)
    target_link_libraries(${TEST_TARGET_NAME} PRIVATE dwmapi)
endif()

target_compile_features(${TEST_TARGET_NAME} PRIVATE cxx_std_17)
set_target_properties(${TEST_TARGET_NAME} PROPERTIES
    AUTORCC ON
    FOLDER "Regression/SMRobotApps/RobotQtViewer"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX}
)

copy_runtime_dlls(${TEST_TARGET_NAME})
if(COMMAND smrobot_deploy_runtime_licenses)
    smrobot_deploy_runtime_licenses(${TEST_TARGET_NAME})
endif()

add_test(
    NAME SMRobotGen2RobotQtViewerThemeStateSmokeTest
    COMMAND $<TARGET_FILE:${TEST_TARGET_NAME}>
)
if(WIN32)
    set(_theme_test_qpa_platform windows)
else()
    set(_theme_test_qpa_platform offscreen)
endif()
set_tests_properties(SMRobotGen2RobotQtViewerThemeStateSmokeTest PROPERTIES
    LABELS "SMRobotGen2;RobotQtViewer;Regression;Theme;UI"
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_TARGET_NAME}>
    ENVIRONMENT "QT_QPA_PLATFORM=${_theme_test_qpa_platform}"
)
unset(_theme_test_qpa_platform)
