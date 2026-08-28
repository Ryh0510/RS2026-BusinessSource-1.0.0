cmake_minimum_required(VERSION 3.20)

set(APP_TARGET_NAME RobotQtViewer)

if(NOT TARGET Qt5::Widgets OR NOT TARGET Qt5::OpenGL)
    message(FATAL_ERROR "${APP_TARGET_NAME} requires Qt5::Widgets and Qt5::OpenGL.")
endif()

if(NOT TARGET SMRobotCore::RobotSDK)
    message(FATAL_ERROR "${APP_TARGET_NAME} requires SMRobotCore::RobotSDK.")
endif()

set(${APP_TARGET_NAME}_REQUIRED_TARGETS
    ${${PACKAGE_NAME}${APP_TARGET_NAME}_RequiredLibsPublic}
)

set(${APP_TARGET_NAME}_MISSING_TARGETS)
foreach(required_target_ IN LISTS ${APP_TARGET_NAME}_REQUIRED_TARGETS)
    if(NOT TARGET ${required_target_})
        list(APPEND ${APP_TARGET_NAME}_MISSING_TARGETS ${required_target_})
    endif()
endforeach()

if(${APP_TARGET_NAME}_MISSING_TARGETS)
    message(STATUS
        "Skip ${APP_TARGET_NAME} because required targets are missing: "
        "${${APP_TARGET_NAME}_MISSING_TARGETS}"
    )
    return()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(ROBOT_QT_VIEWER_BUILTIN_WORKBENCH_TARGETS
    SMRobotWorkbenchProjectAssembly::ProjectAssemblyWorkbench
    SMRobotWorkbenchCollisionConfig::CollisionConfigWorkbench
    SMRobotWorkbenchRobotRun::RobotRunWorkbench
    SMRobotWorkbenchMotionPlanning::MotionPlanningWorkbench
    SMRobotWorkbenchPaintingAnalysis::PaintingAnalysisWorkbench
    SMRobotWorkbenchSprayProcess::SprayProcessWorkbench
    SMRobotWorkbenchDigitalTwin::DigitalTwinWorkbench
)
set(SMROBOT_ROBOT_QT_VIEWER_EXTRA_WORKBENCH_TARGETS "" CACHE STRING
    "Additional static Workbench targets contributing to RobotQtViewer.")
list(APPEND ROBOT_QT_VIEWER_BUILTIN_WORKBENCH_TARGETS
    ${SMROBOT_ROBOT_QT_VIEWER_EXTRA_WORKBENCH_TARGETS})

set(ROBOT_QT_VIEWER_WORKBENCH_REGISTRAR_INCLUDES)
set(ROBOT_QT_VIEWER_WORKBENCH_REGISTRAR_CALLS)
foreach(workbench_target_ IN LISTS ROBOT_QT_VIEWER_BUILTIN_WORKBENCH_TARGETS)
    if(NOT TARGET ${workbench_target_})
        message(FATAL_ERROR
            "RobotQtViewer Workbench contribution target is missing: ${workbench_target_}")
    endif()
    get_target_property(workbench_registrar_header_
        ${workbench_target_} SMROBOT_WORKBENCH_REGISTRAR_HEADER)
    get_target_property(workbench_registrar_function_
        ${workbench_target_} SMROBOT_WORKBENCH_REGISTRAR_FUNCTION)
    if(NOT workbench_registrar_header_ OR NOT workbench_registrar_function_)
        message(FATAL_ERROR
            "Workbench target ${workbench_target_} does not expose registrar metadata.")
    endif()
    get_target_property(workbench_target_imported_ ${workbench_target_} IMPORTED)
    if(workbench_target_imported_)
        set(workbench_source_kind_
            "RobotQtViewerWorkbenchPackageSource::Prebuilt")
    else()
        set(workbench_source_kind_
            "RobotQtViewerWorkbenchPackageSource::BuiltInSource")
    endif()
    string(APPEND ROBOT_QT_VIEWER_WORKBENCH_REGISTRAR_INCLUDES
        "#include \"${workbench_registrar_header_}\"\n")
    string(APPEND ROBOT_QT_VIEWER_WORKBENCH_REGISTRAR_CALLS
        "        if(!${workbench_registrar_function_}(catalog, ${workbench_source_kind_})) {\n"
        "            if(errorMessage != nullptr) {\n"
        "                *errorMessage = QStringLiteral(\"Workbench contribution registration failed: ${workbench_target_}\");\n"
        "            }\n"
        "            return {};\n"
        "        }\n")
endforeach()

set(ROBOT_QT_VIEWER_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${ROBOT_QT_VIEWER_GENERATED_DIR})
configure_file(
    RobotQtViewerBuiltWorkbenchCatalog.cpp.in
    ${ROBOT_QT_VIEWER_GENERATED_DIR}/RobotQtViewerBuiltWorkbenchCatalog.cpp
    @ONLY)

set(${APP_TARGET_NAME}_SHELL_SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    RobotQtViewerBuiltWorkbenchCatalog.h
    ${ROBOT_QT_VIEWER_GENERATED_DIR}/RobotQtViewerBuiltWorkbenchCatalog.cpp
    RobotQtViewerPlatformConfigurationDialog.cpp
    RobotQtViewerPlatformConfigurationDialog.h
    RobotQtViewerProductProfileConfigurationDialog.cpp
    RobotQtViewerProductProfileConfigurationDialog.h
    RobotQtViewerRibbonModel.cpp
    RobotQtViewerRibbonModel.h
    RobotQtViewerResources.qrc
    RobotQtViewerSceneExplorerActionRouter.cpp
    RobotQtViewerSceneExplorerActionRouter.h
    RobotQtViewerToolbarController.cpp
    RobotQtViewerToolbarController.h
    RobotQtViewerCollisionWorkbenchServicesAdapter.cpp
    RobotQtViewerCollisionWorkbenchServicesAdapter.h
    RobotQtViewerToolSetupAppServicesAdapter.cpp
    RobotQtViewerToolSetupAppServicesAdapter.h
    RobotQtViewerViewportEventController.cpp
    RobotQtViewerViewportEventController.h
    RobotQtViewerViewportPresentationController.cpp
    RobotQtViewerViewportPresentationController.h
    RobotQtViewerViewportServicesAdapter.cpp
    RobotQtViewerViewportServicesAdapter.h
    RobotQtViewerTheme.cpp
    RobotQtViewerTheme.h
)

if(WIN32)
    list(APPEND ${APP_TARGET_NAME}_SHELL_SOURCES
        RobotQtViewer.rc
        RobotQtViewerResource.h
    )
endif()

add_executable(${APP_TARGET_NAME}
    ${${APP_TARGET_NAME}_SHELL_SOURCES}
)

target_include_directories(${APP_TARGET_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ROBOT_QT_VIEWER_GENERATED_DIR}
)

add_executable(SMRobotApps::${APP_TARGET_NAME} ALIAS ${APP_TARGET_NAME})
add_executable(Applications::${APP_TARGET_NAME} ALIAS ${APP_TARGET_NAME})

target_link_libraries(${APP_TARGET_NAME}
    PRIVATE
        SMRobotWorkbenchCommon::RobotQtModulesCoreWidgets
        SMRobotWorkbenchCommon::RobotQtModulesShared
        RobotQtModulesStatus
        ${${APP_TARGET_NAME}_REQUIRED_TARGETS}
)

if(WIN32)
    target_link_libraries(${APP_TARGET_NAME}
        PRIVATE
            dwmapi
    )
endif()

target_compile_features(${APP_TARGET_NAME}
    PRIVATE
        cxx_std_17
)

target_compile_definitions(${APP_TARGET_NAME}
    PRIVATE
        ROBOT_QT_VIEWER_DEFAULT_PROJECT_PATH="projects/ABB4600-burnner.sys.json"
)

set_target_properties(${APP_TARGET_NAME}
    PROPERTIES
        FOLDER SMRobotApps
        DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
        RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX}
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set_property(TARGET ${APP_TARGET_NAME} PROPERTY
        VS_DEBUGGER_ENVIRONMENT $<$<CONFIG:Debug>:${VSDebugEnvPath}>)
endif()

#############################################################
#	Copy the runtime files to release directory for current target
#############################################################
copy_runtime_dlls( ${APP_TARGET_NAME} )

if(TARGET CoACD::coacd AND COMMAND CoACD_CopyRuntimeDependencies)
    CoACD_CopyRuntimeDependencies(${APP_TARGET_NAME})
endif()

deploy_qt( ${APP_TARGET_NAME} )

add_custom_command(TARGET ${APP_TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:${APP_TARGET_NAME}>/config"
    COMMENT "Deploy RobotQtViewer runtime configuration"
)

rs_example_category_enabled(_build_regression Regression ${PACKAGE_NAME})
if(_build_regression)
    add_subdirectory(regression)
endif()

if(ExportPacakge${PACKAGE_NAME})
    set(${APP_TARGET_NAME}_INSTALL_HEADER_STAGING_DIR ${CMAKE_CURRENT_BINARY_DIR}/install_headers)
    file(MAKE_DIRECTORY ${${APP_TARGET_NAME}_INSTALL_HEADER_STAGING_DIR})

    function_InstallTarget(
        TARGET_NAME ${APP_TARGET_NAME}
        PACKAGE_NAME ${PACKAGE_NAME}
        PACKAGE_INTERFACE_HEADERS ${${APP_TARGET_NAME}_INSTALL_HEADER_STAGING_DIR}
    )

    install(FILES
        $<TARGET_RUNTIME_DLLS:${APP_TARGET_NAME}>
        DESTINATION ${PACKAGE_NAME}/bin
        CONFIGURATIONS Release
    )

    install(DIRECTORY
        $<TARGET_FILE_DIR:${APP_TARGET_NAME}>/
        DESTINATION ${PACKAGE_NAME}/bin
        CONFIGURATIONS Release
        FILES_MATCHING
            PATTERN "*.dll"
            PATTERN "iconengines/*"
            PATTERN "imageformats/*"
            PATTERN "platforms/*"
            PATTERN "styles/*"
    )

    install(DIRECTORY
        ${PROJECT_SOURCE_DIR}/config/platforms/
        DESTINATION ${PACKAGE_NAME}/bin/config/platforms
        FILES_MATCHING PATTERN "*.platform.json"
    )
endif()

