#############################################################
#	CMake requirement (From Modern CMake)
#############################################################
cmake_minimum_required( VERSION 3.20 )

#	Set target name
set( TARGET_NAME "IdGenerator" )
message( STATUS "\n-------------  ${GROUP_NAME} - ${TARGET_NAME}  -------------" )

#############################################################
#	Add the head and source files to the current project
#############################################################
#	Build this project
file( GLOB SOURCES *.cpp *.c )
file( GLOB_RECURSE INTERFACE_HEADERS 
    ${PROJECT_SOURCE_DIR}/include/${GROUP_NAME}/${TARGET_NAME}/*.h
    )


#############################################################
#	Create target and Set the target prorperties
#############################################################
#	Create target
add_library( ${TARGET_NAME} STATIC ${SOURCES} ${INTERFACE_HEADERS} )
add_library( ${GROUP_NAME}::${TARGET_NAME} ALIAS ${TARGET_NAME})


#   Set Include Directories
target_include_directories( ${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/${GROUP_NAME}>
    PRIVATE
        $<INSTALL_INTERFACE:include>
    )

# #   Set Link Libraries
# target_link_libraries( ${TARGET_NAME}
#     PRIVATE
#         $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:OpenSSL::SSL>
#         $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:OpenSSL::Crypto>
        
#         $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>:OpenSSL::OpenSSLx64Static>
#         $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>:Crypt32>
#         $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>:Ws2_32>
# )

#  Set project compile definitions
target_compile_definitions( ${TARGET_NAME}
    PRIVATE
        # $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:LINUX>
        # $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>:WINDOWS>
)

#   Set the properties of the target
set_target_properties( ${TARGET_NAME}
    PROPERTIES 
        FOLDER                                  ${GROUP_NAME}  
        DEBUG_POSTFIX                           _static_${CMAKE_DEBUG_POSTFIX}
        RELEASE_POSTFIX                         _static_${CMAKE_RELEASE_POSTFIX}
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:COMPILE_FLAGS -fPIC>
)

#############################################################
#	The Install Setting
#############################################################
if( ${EXPORT_${GROUP_NAME}_${TARGET_NAME}} )
    InstallTarget( ${TARGET_NAME} ${GROUP_NAME} ${INTERFACE_HEADERS} )
endif()

#   The End of CMakeLists.txt
message( STATUS "-------------  ${GROUP_NAME} - ${TARGET_NAME}  -------------\n" )

