#############################################################
#	CMake requirement (From Modern CMake)
#############################################################
cmake_minimum_required(VERSION 3.20)

rs_example_category_enabled( _build_example FeatureProbes ${PACKAGE_NAME} )
if( NOT _build_example )
    return()
endif()

#	Set project name
message( STATUS "Current directory : ${CMAKE_CURRENT_SOURCE_DIR}" )
message( STATUS "\n-------------  ${PACKAGE_NAME} - ${TARGET_NAME} - Example -------------" )

set( EXAMPLE_TARGET_NAME ${TARGET_NAME}-Example )

#############################################################
#	Add the head and source files to the current project
#############################################################
file(GLOB EXAMPLE_SOURCES *.cpp     *.c     )
file(GLOB EXAMPLE_HEADERS *.h       *.hpp   )
# file(GLOB INTERFACE_HEADERS include/*.h include/*.hpp )

#############################################################
#	Create target and Set the target prorperties
#############################################################
#	Create target
add_executable( ${EXAMPLE_TARGET_NAME} ${EXAMPLE_SOURCES} ${EXAMPLE_HEADERS} )

#   Set Link Libraries
target_link_libraries ( ${EXAMPLE_TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${PACKAGE_NAME}::${TARGET_NAME}>        
)

#   Set the properties of the target
set_target_properties( ${EXAMPLE_TARGET_NAME} 
    PROPERTIES 
        FOLDER                  "FeatureProbes/Common/LogSystem"
        DEBUG_POSTFIX           ${CMAKE_DEBUG_POSTFIX}
        RELEASE_POSTFIX         ${CMAKE_RELEASE_POSTFIX}
)

message( STATUS "-------------  ${PACKAGE_NAME} - ${TARGET_NAME} - Example  -------------\n" )

