cmake_minimum_required(VERSION 3.20)
add_executable(TrajectoryOptimizationWorkbenchTest ../../../TrajectoryOptimizationWorkbench/tests/main.cpp)
target_link_libraries(TrajectoryOptimizationWorkbenchTest PRIVATE PaintingAnalysisWorkbench Qt5::Widgets nlohmann_json::nlohmann_json)
target_include_directories(TrajectoryOptimizationWorkbenchTest PRIVATE "${PROJECT_SOURCE_DIR}/SMRobotSpray/SprayTrajectoryOptimization/algorithms/chaomai_plane")
target_compile_features(TrajectoryOptimizationWorkbenchTest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(TrajectoryOptimizationWorkbenchTest PRIVATE /utf-8)
endif()
set_target_properties(TrajectoryOptimizationWorkbenchTest PROPERTIES FOLDER tests/SMRobotWorkbenchPaintingAnalysis
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX} RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX})
copy_runtime_dlls(TrajectoryOptimizationWorkbenchTest)
deploy_qt(TrajectoryOptimizationWorkbenchTest)
add_test(NAME TrajectoryOptimizationWorkbenchTest COMMAND $<TARGET_FILE:TrajectoryOptimizationWorkbenchTest>)
get_target_property(_optimization_qt_core Qt5::Core IMPORTED_LOCATION_RELEASE)
get_filename_component(_optimization_qt_bin "${_optimization_qt_core}" DIRECTORY)
set_tests_properties(TrajectoryOptimizationWorkbenchTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${_optimization_qt_bin}/../plugins/platforms" TIMEOUT 30
    WORKING_DIRECTORY $<TARGET_FILE_DIR:TrajectoryOptimizationWorkbenchTest>)
