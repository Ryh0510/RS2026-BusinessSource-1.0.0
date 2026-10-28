cmake_minimum_required(VERSION 3.20)
if(NOT TARGET MotionPlanningEditor OR NOT TARGET PaintingAnalysisWorkbench OR NOT TARGET FreeformTrajPlanWorkbench)
    return()
endif()
set(TEST_TARGET RobotQtViewer-TrajectoryTransferSmoke)
set(VIEWER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_executable(${TEST_TARGET} main.cpp
    "${VIEWER_SOURCE_DIR}/RobotQtViewerViewportServicesAdapter.cpp"
    "${VIEWER_SOURCE_DIR}/RobotQtViewerViewportEventController.cpp")
target_include_directories(${TEST_TARGET} PRIVATE "${VIEWER_SOURCE_DIR}")
target_link_libraries(${TEST_TARGET} PRIVATE
    SMRobotApps::RobotViewerCore MotionPlanningEditor PaintingAnalysisWorkbench
    FreeformTrajPlanWorkbench SMRobotMotionPlanning::ProjectMotionPlanning
    nlohmann_json::nlohmann_json ${RobotQtViewer_REQUIRED_TARGETS})
target_compile_features(${TEST_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${TEST_TARGET} PRIVATE /utf-8 /bigobj)
endif()
if(WIN32)
    target_link_libraries(${TEST_TARGET} PRIVATE dwmapi)
endif()
set_target_properties(${TEST_TARGET} PROPERTIES FOLDER tests/SMRobotApps
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX} RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX})
copy_runtime_dlls(${TEST_TARGET})
deploy_qt(${TEST_TARGET})
foreach(MODE data workbenches cdf receiving)
    add_test(NAME RobotQtViewerTrajectoryTransfer-${MODE} COMMAND ${TEST_TARGET} --${MODE})
    set_tests_properties(RobotQtViewerTrajectoryTransfer-${MODE} PROPERTIES
        LABELS "RobotQtViewer;Regression;TrajectoryTransfer"
        TIMEOUT 180 WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_TARGET}>)
    if(MODE STREQUAL "cdf")
        # Production GA evaluates 5000 initial postures per source control.
        set_tests_properties(RobotQtViewerTrajectoryTransfer-${MODE} PROPERTIES TIMEOUT 1800)
    endif()
endforeach()
