#############################################################
#	CMake requirement (From Modern CMake)
#############################################################
cmake_minimum_required(VERSION 3.20)

rs_example_category_enabled( _build_example Diagnostics ${PACKAGE_NAME} )
if( NOT _build_example )
    return()
endif()

#	Set example name
get_filename_component( EXAMPLE_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME )

message( STATUS "Current directory : ${CMAKE_CURRENT_SOURCE_DIR}" )
message( STATUS "\n-------------  ${PACKAGE_NAME} - ${TARGET_NAME} - Example -------------" )

set( EXAMPLE_TARGET_NAME ${TARGET_NAME}-${EXAMPLE_NAME} )

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

#############################################################
#	Add the head and source files to the current project
#############################################################
file(GLOB EXAMPLE_SOURCES *.cpp     *.c     )
file(GLOB EXAMPLE_HEADERS *.h       *.hpp   )

#############################################################
#	Create target and Set the target prorperties
#############################################################
#	Create target
add_executable( ${EXAMPLE_TARGET_NAME} ${EXAMPLE_SOURCES} ${EXAMPLE_HEADERS} )


#   Set Link Libraries
target_link_libraries ( ${EXAMPLE_TARGET_NAME}
    PRIVATE
        $<BUILD_INTERFACE:${PACKAGE_NAME}::${TARGET_NAME}>
        $<BUILD_INTERFACE:Qt5::Widgets Qt5::OpenGL>        
)

#   Set the properties of the target
set_target_properties( ${EXAMPLE_TARGET_NAME} 
    PROPERTIES 
        FOLDER                  "Diagnostics/Common/GLRuntime"
        DEBUG_POSTFIX           ${CMAKE_DEBUG_POSTFIX}
        RELEASE_POSTFIX         ${CMAKE_RELEASE_POSTFIX}
)

#############################################################
#	Runtime environment setting
#############################################################
#   Set runtime directory for VS project

if( CMAKE_SYSTEM_NAME MATCHES "Windows" )
    set_property( TARGET ${EXAMPLE_TARGET_NAME} PROPERTY 
        VS_DEBUGGER_ENVIRONMENT $<$<CONFIG:Debug>:${VSDebugEnvPath}> )     
endif()

message( STATUS "-------------  ${PACKAGE_NAME} - ${TARGET_NAME} - Example  -------------\n" )

