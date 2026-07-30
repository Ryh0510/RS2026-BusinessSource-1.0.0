#############################################################
# CMake requirement
#############################################################
cmake_minimum_required(VERSION 3.20)

rs_example_category_enabled( _build_example Tutorials ${PACKAGE_NAME} )
if( NOT _build_example )
    return()
endif()

set(EXAMPLE_TARGET_NAME RobotSDKQuickStart)

add_executable(${EXAMPLE_TARGET_NAME} main.cpp)

target_compile_features(${EXAMPLE_TARGET_NAME} PRIVATE cxx_std_17)

target_include_directories(${EXAMPLE_TARGET_NAME}
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
)

target_link_libraries(${EXAMPLE_TARGET_NAME}
    PRIVATE
        SMRobotCore::RobotSDK
)

set_target_properties(${EXAMPLE_TARGET_NAME} PROPERTIES
    FOLDER "Tutorials/${PACKAGE_NAME}/RobotSDK"
)

rs_robot_sdk_deploy_tutorial_runtime(${EXAMPLE_TARGET_NAME})

if( ExportPacakge${PACKAGE_NAME} )
    install(TARGETS ${EXAMPLE_TARGET_NAME}
        RUNTIME DESTINATION ${PACKAGE_NAME}/bin/examples
    )
endif()
