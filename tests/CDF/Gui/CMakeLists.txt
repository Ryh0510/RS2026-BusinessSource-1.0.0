cmake_minimum_required(VERSION 3.20)

set(TARGET_NAME CDFQtPlanningDemos)

find_package(Qt5 COMPONENTS Widgets OpenGL QUIET)

set(${TARGET_NAME}_REQUIRED_TARGETS
    Qt5::Widgets
    Qt5::OpenGL
    Common::GLRuntime
    Common::CustomLog
    Common::Utility
    SMRobotPlatform::AssetCore
    SMRobotPlatform::CameraCore
    SMRobotCore::Collision
    SMRobotCore::Kinematics
    SMRobotPlatform::RenderCore
    SMRobotCore::RobotCore
    SMRobotCore::RobotIO
    SMRobotCore::RobotInstance
    SMRobotPlatform::RobotRenderBridge
    SMRobotCore::RobotSDK
    SMRobotCore::RobotRuntime
    SMRobotPlatform::SceneCore
    SMRobotPlatform::SimulationProject
    SMRobotPlatform::SimulationRuntime
    SMRobotApps::RobotViewerCore
)

set(${TARGET_NAME}_MISSING_TARGETS)
foreach(required_target_ IN LISTS ${TARGET_NAME}_REQUIRED_TARGETS)
    if(NOT TARGET ${required_target_})
        list(APPEND ${TARGET_NAME}_MISSING_TARGETS ${required_target_})
    endif()
endforeach()

if(${TARGET_NAME}_MISSING_TARGETS)
    message(STATUS
        "Skip ${TARGET_NAME} because required targets are missing: "
        "${${TARGET_NAME}_MISSING_TARGETS}"
    )
    return()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}" CDF_PROJECT_SOURCE_PATH_FOR_CPP)
file(TO_CMAKE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../three_dof_robot_3r_description/meshes/link1.STL"
    CDF_3R_LINK1_MESH_PATH_FOR_URDF)
file(TO_CMAKE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../three_dof_robot_3r_description/meshes/link2.STL"
    CDF_3R_LINK2_MESH_PATH_FOR_URDF)
file(TO_CMAKE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../three_dof_robot_3r_description/meshes/link3.STL"
    CDF_3R_LINK3_MESH_PATH_FOR_URDF)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/three_dof_robot_3r_viewer.urdf.in"
    "${CMAKE_CURRENT_BINARY_DIR}/three_dof_robot_3r_viewer.urdf"
    @ONLY
)
file(TO_CMAKE_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/three_dof_robot_3r_viewer.urdf"
    CDF_3R_URDF_PATH_FOR_CPP)
file(TO_CMAKE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThreeDOF_Robot_ObstacleScene.SLDASM/meshes/base_link.STL"
    CDF_OBSTACLE_SCENE_MESH_PATH_FOR_URDF)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ThreeDOF_Robot_ObstacleScene_viewer.urdf.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ThreeDOF_Robot_ObstacleScene_viewer.urdf"
    @ONLY
)
file(TO_CMAKE_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/ThreeDOF_Robot_ObstacleScene_viewer.urdf"
    CDF_OBSTACLE_SCENE_URDF_PATH_FOR_CPP)

function(cdf_qt_demo_deploy_runtime TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${TARGET_NAME}>
            $<TARGET_FILE_DIR:${TARGET_NAME}>
        COMMAND_EXPAND_LISTS
    )

    if(WIN32)
        get_target_property(_qmake Qt5::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qmake}" DIRECTORY)

        find_program(WINDEPLOYQT_EXECUTABLE
            windeployqt
            HINTS "${_qt_bin_dir}"
        )

        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E env
                    "PATH=$<TARGET_FILE_DIR:${TARGET_NAME}>\;${_qt_bin_dir}"
                    "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-compiler-runtime
                    "$<TARGET_FILE:${TARGET_NAME}>"
                COMMENT "Deploying Qt runtime for ${TARGET_NAME}"
            )
        endif()
    endif()
endfunction()

add_executable(${TARGET_NAME}
    src/CdfDemoController.cpp
    src/CdfDemoController.h
    src/CdfDemoMain.cpp
    src/CdfDemoViewModel.h
    src/CdfDemoViewModelBuilder.cpp
    src/CdfDemoViewModelBuilder.h
    src/CdfDemoWindow.cpp
    src/CdfDemoWindow.h
    src/CdfPathCanvasWidget.cpp
    src/CdfPathCanvasWidget.h
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        CDF::Algorithms
        ${${TARGET_NAME}_REQUIRED_TARGETS}
)

target_compile_definitions(${TARGET_NAME}
    PRIVATE
        CDF_PROJECT_SOURCE_PATH="${CDF_PROJECT_SOURCE_PATH_FOR_CPP}"
        CDF_3R_URDF_PATH="${CDF_3R_URDF_PATH_FOR_CPP}"
        CDF_OBSTACLE_SCENE_MESH_PATH="${CDF_OBSTACLE_SCENE_MESH_PATH_FOR_URDF}"
        CDF_OBSTACLE_SCENE_URDF_PATH="${CDF_OBSTACLE_SCENE_URDF_PATH_FOR_CPP}"
)

target_compile_features(${TARGET_NAME}
    PRIVATE
        cxx_std_17
)

set_target_properties(${TARGET_NAME}
    PROPERTIES
        FOLDER "tests/CDF/Gui"
)

cdf_qt_demo_deploy_runtime(${TARGET_NAME})

add_executable(CDFProjectCollisionSmoke
    smoke/CdfProjectCollisionSmoke.cpp
)

target_link_libraries(CDFProjectCollisionSmoke
    PRIVATE
        CDF::Algorithms
        SMRobotPlatform::SimulationProject
        SMRobotPlatform::SimulationRuntime
        SMRobotCore::Collision
        SMRobotCore::Kinematics
        SMRobotCore::RobotCore
        SMRobotCore::RobotIO
        SMRobotCore::RobotInstance
        SMRobotCore::RobotRuntime
)

target_compile_definitions(CDFProjectCollisionSmoke
    PRIVATE
        CDF_PROJECT_SOURCE_PATH="${CDF_PROJECT_SOURCE_PATH_FOR_CPP}"
        CDF_3R_URDF_PATH="${CDF_3R_URDF_PATH_FOR_CPP}"
        CDF_OBSTACLE_SCENE_MESH_PATH="${CDF_OBSTACLE_SCENE_MESH_PATH_FOR_URDF}"
)

target_compile_features(CDFProjectCollisionSmoke
    PRIVATE
        cxx_std_17
)

set_target_properties(CDFProjectCollisionSmoke
    PROPERTIES
        FOLDER "tests/CDF/Gui"
)
