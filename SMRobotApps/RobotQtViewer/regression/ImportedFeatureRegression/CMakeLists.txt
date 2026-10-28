cmake_minimum_required(VERSION 3.20)
set(TEST_TARGET RobotQtViewer-ImportedFeatureRegression)
add_executable(${TEST_TARGET} main.cpp)
target_link_libraries(${TEST_TARGET} PRIVATE
    SMRobotSpray::CalibrationInstructionTranslation
    SMRobotSpray::SprayThicknessPredictionOpenGL Common::GLRuntime Qt5::Gui TrajModule)
target_compile_features(${TEST_TARGET} PRIVATE cxx_std_17)
set_target_properties(${TEST_TARGET} PROPERTIES FOLDER tests/SMRobotApps
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX} RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX})
copy_runtime_dlls(${TEST_TARGET})
deploy_qt(${TEST_TARGET})
foreach(MODE calibration gpu-thickness uv-pipeline)
    add_test(NAME ImportedFeature-${MODE} COMMAND ${TEST_TARGET} --${MODE})
    set_tests_properties(ImportedFeature-${MODE} PROPERTIES TIMEOUT 180
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_TARGET}>)
endforeach()
