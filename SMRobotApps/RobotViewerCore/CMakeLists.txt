cmake_minimum_required(VERSION 3.20)

set(TARGET_NAME RobotViewerCore)

set(${TARGET_NAME}_REQUIRED_TARGETS
    ${${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPublic}
    ${${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPrivate}
)
list(REMOVE_DUPLICATES ${TARGET_NAME}_REQUIRED_TARGETS)

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

add_library(${TARGET_NAME} STATIC
    CameraPreviewWidget.cpp
    CameraPreviewWidget.h
    ProjectCollisionRuntimeProjection.cpp
    ProjectCollisionRuntimeProjection.h
    ProjectRuntimeBuilder.cpp
    ProjectRuntimeBuilder.h
    ProjectRuntimeTypes.cpp
    ProjectRuntimeTypes.h
    ProjectSceneAttachmentVisualSystem.cpp
    ProjectSceneAttachmentVisualSystem.h
    ProjectSceneCameraSystem.cpp
    ProjectSceneCameraSystem.h
    ProjectSceneCollisionPresentationSystem.cpp
    ProjectSceneCollisionPresentationSystem.h
    ProjectSceneDocumentProjectionSystem.cpp
    ProjectSceneDocumentProjectionSystem.h
    ProjectSceneEnvironment.cpp
    ProjectSceneEnvironment.h
    ProjectSceneEnvironmentSystem.cpp
    ProjectSceneEnvironmentSystem.h
    ProjectSceneInteractionSystem.cpp
    ProjectSceneInteractionSystem.h
    ProjectSceneStewartPresentationSystem.cpp
    ProjectSceneStewartPresentationSystem.h
    ProjectScene.cpp
    ProjectScene.h
    ProjectScenePickingService.cpp
    ProjectScenePickingService.h
    ProjectScenePreviewOverlayState.cpp
    ProjectScenePreviewOverlayState.h
    RobotCollisionOverrideApplier.cpp
    RobotCollisionOverrideApplier.h
    RobotCollisionModelInspector.cpp
    RobotCollisionModelInspector.h
    RobotCollisionProxyGenerator.cpp
    RobotCollisionProxyGenerator.h
    RobotViewport.cpp
    RobotViewport.h
)

add_library(SMRobotApps::${TARGET_NAME} ALIAS ${TARGET_NAME})
add_library(Applications::${TARGET_NAME} ALIAS ${TARGET_NAME})

target_compile_features(${TARGET_NAME}
    PUBLIC
        cxx_std_17
)

target_compile_options(${TARGET_NAME}
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        ${${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPublic}
    PRIVATE
        ${${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPrivate}
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${PACKAGE_NAME}/include/RobotViewerCore>
)

set(${TARGET_NAME}_INSTALL_PUBLIC_DEPENDENCIES
    ${${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPublic})
set(${TARGET_NAME}_INSTALL_RESOLVE_DEPENDENCIES
    ${${TARGET_NAME}_REQUIRED_TARGETS})

set_target_properties(${TARGET_NAME}
    PROPERTIES
        FOLDER SMRobotApps
        AUTOMOC ON
)

if(ExportPacakge${PACKAGE_NAME})
    set(${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR ${CMAKE_CURRENT_BINARY_DIR}/install_headers)
    file(MAKE_DIRECTORY ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR})

    function_InstallTarget(
        TARGET_NAME ${TARGET_NAME}
        PACKAGE_NAME ${PACKAGE_NAME}
        PACKAGE_INTERFACE_HEADERS ${${TARGET_NAME}_INSTALL_HEADER_STAGING_DIR}
    )

    install(FILES
        ProjectRuntimeBuilder.h
        ProjectRuntimeTypes.h
        ProjectScene.h
        ProjectSceneEnvironment.h
        ProjectScenePickingService.h
        RobotCollisionOverrideApplier.h
        RobotCollisionModelInspector.h
        RobotCollisionProxyGenerator.h
        RobotViewport.h
        DESTINATION ${PACKAGE_NAME}/include/RobotViewerCore
        COMPONENT smrobot_sdk_${PACKAGE_NAME}_${TARGET_NAME}
    )
endif()

rs_add_example_category_roots()
