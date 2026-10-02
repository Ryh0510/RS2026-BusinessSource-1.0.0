cmake_minimum_required(VERSION 3.20)

set(_smrobot_quickstart_standalone OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(CustomMeshViewerQuickStart LANGUAGES CXX)
    set(_smrobot_quickstart_standalone ON)
    find_package(SMRobotApps CONFIG REQUIRED COMPONENTS RobotViewerCore)
endif()

add_executable(CustomMeshViewerQuickStart main.cpp)
target_compile_features(CustomMeshViewerQuickStart PRIVATE cxx_std_17)
target_link_libraries(CustomMeshViewerQuickStart PRIVATE SMRobotApps::RobotViewerCore)

if(_smrobot_quickstart_standalone)
    add_custom_command(TARGET CustomMeshViewerQuickStart POST_BUILD
        COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:CustomMeshViewerQuickStart>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:CustomMeshViewerQuickStart>;$<TARGET_FILE_DIR:CustomMeshViewerQuickStart>>"
        COMMAND_EXPAND_LISTS
    )
endif()

if(_smrobot_quickstart_standalone AND WIN32 AND TARGET Qt5::qmake)
    get_target_property(_qmake Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET CustomMeshViewerQuickStart POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env
                "PATH=$<TARGET_FILE_DIR:CustomMeshViewerQuickStart>\;${_qt_bin_dir}"
                "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-compiler-runtime
                "$<TARGET_FILE:CustomMeshViewerQuickStart>"
        )
    endif()
endif()

if(NOT _smrobot_quickstart_standalone)
    set_target_properties(CustomMeshViewerQuickStart PROPERTIES
        FOLDER "Examples/SMRobotApps")
    copy_runtime_dlls(CustomMeshViewerQuickStart)
    deploy_qt(CustomMeshViewerQuickStart)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME CustomMeshViewerQuickStart COMMAND CustomMeshViewerQuickStart)
    set_tests_properties(CustomMeshViewerQuickStart PROPERTIES
        WORKING_DIRECTORY $<TARGET_FILE_DIR:CustomMeshViewerQuickStart>
        TIMEOUT 30
    )
endif()
