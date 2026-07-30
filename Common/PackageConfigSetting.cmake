##########################################################################
#	CMake requirement
##########################################################################
cmake_minimum_required( VERSION 3.20 )

##########################################################################
#	${PACKAGE_NAME} Configuration
##########################################################################
#   Get directory name as package name
message( STATUS "!!!!!!Package ${PACKAGE_NAME} configuration is set here!!!!!!" )


##########################################################################
#	Package ${PACKAGE_NAME} export components definition
##########################################################################
set( ${PACKAGE_NAME}_PublicComponents
    FileSystem
    CustomLog
    LogSystem
    # GLAdvanced        
    GLRuntime
    LicenseVerification
    StringToFile
    Utility


    IDGenerator
    stb_image
)

set( ${PACKAGE_NAME}_CompatibilityComponents )
set( ${PACKAGE_NAME}_TutorialDirs )
set( ${PACKAGE_NAME}_InstallDocs )

set( ${PACKAGE_NAME}_ExportComponents
    ${${PACKAGE_NAME}_PublicComponents}
    ${${PACKAGE_NAME}_CompatibilityComponents}
)

##########################################################################
#	Package ${PACKAGE_NAME} components definition
##########################################################################
#   1. Get all targets' names

set( ExcludeTargets )
# message( STATUS "ExcludeTargets is : ${ExcludeTargets}" )
collect_subdirs(
    ${CMAKE_CURRENT_LIST_DIR}
    AllTargetNames
    EXCLUDE ${ExcludeTargets}
)

#   Package components setting
set( ${PACKAGE_NAME}_Components ${AllTargetNames} )
message( STATUS "==================================================================")
message( STATUS "All Targets in Package ${PACKAGE_NAME}: ${${PACKAGE_NAME}_Components}" )
message( STATUS "==================================================================\n")

set( ${PACKAGE_NAME}_InternalComponents ${${PACKAGE_NAME}_Components} )
foreach( public_comp_ ${${PACKAGE_NAME}_ExportComponents} )
    list( REMOVE_ITEM ${PACKAGE_NAME}_InternalComponents ${public_comp_} )
endforeach()

foreach( comp_ ${${PACKAGE_NAME}_Components} )
    list( FIND ${PACKAGE_NAME}_ExportComponents ${comp_} export_index_ )
    if( export_index_ EQUAL -1 )
        set( ExportPacakge${PACKAGE_NAME}_${comp_} OFF CACHE BOOL "Do not export internal ${PACKAGE_NAME}::${comp_}" FORCE )
    else()
        set( ExportPacakge${PACKAGE_NAME}_${comp_} ON CACHE BOOL "Export public ${PACKAGE_NAME}::${comp_}" FORCE )
    endif()
endforeach()


##########################################################################
#	Package ${PACKAGE_NAME} required targets
##########################################################################
#   2. Get all targets' required libs
foreach( TARGET_NAME ${${PACKAGE_NAME}_Components} )
    include( ${PACKAGE_NAME}/${TARGET_NAME}/TargetConfigSetting )

    list( APPEND ${PACKAGE_NAME}_RequiredLibsPublic ${${TARGET_NAME}_RequiredLibsPublic} )
    list( APPEND ${PACKAGE_NAME}_RequiredLibsPrivate ${${TARGET_NAME}_RequiredLibsPrivate} ) 

    set( ${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPublic ${${TARGET_NAME}_RequiredLibsPublic} )
    set( ${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPrivate ${${TARGET_NAME}_RequiredLibsPrivate} )
endforeach()



#   3. Remove duplicated libs and catolog the targets
list( REMOVE_DUPLICATES ${PACKAGE_NAME}_RequiredLibsPublic )
list( REMOVE_DUPLICATES ${PACKAGE_NAME}_RequiredLibsPrivate )
list( APPEND ${PACKAGE_NAME}_RequiredLibs ${${PACKAGE_NAME}_RequiredLibsPublic} ${${PACKAGE_NAME}_RequiredLibsPrivate} )

# message( STATUS "[PackageConfigSetting1] ${PACKAGE_NAME}_RequiredLibsPublic: ${${PACKAGE_NAME}_RequiredLibsPublic}" )
# message( STATUS "[PackageConfigSetting1] ${PACKAGE_NAME}_RequiredLibsPrivate: ${${PACKAGE_NAME}_RequiredLibsPrivate}" )


list( REMOVE_DUPLICATES ${PACKAGE_NAME}_RequiredLibs )
resolve_dependencies( ${PACKAGE_NAME}_RequiredLibs )

# message( STATUS "[PackageConfigSetting2] ${PACKAGE_NAME}_RequiredLibsPublic: ${${PACKAGE_NAME}_RequiredLibsPublic}" )
# message( STATUS "[PackageConfigSetting2] ${PACKAGE_NAME}_RequiredLibsPrivate: ${${PACKAGE_NAME}_RequiredLibsPrivate}" )



#   4. Display the required libs for this package
message( STATUS "AllPackagesRequired is ${AllPackagesRequired}" )
foreach( pkg ${AllPackagesRequired} )
    message( STATUS "Finding ${pkg} with components: ${${pkg}_Components}" )
endforeach()


# #   5. Handle the collected targets's required libs
# set( ${PACKAGE_NAME}_RequiredLibsPublic

# )

# set( ${PACKAGE_NAME}_RequiredLibsPrivate

# )

# #   3. Catalog the required libs
# parse_required_libs( ${PACKAGE_NAME}_RequiredLibs )

# message(STATUS "Boost components: ${Boost_Components}")
# message(STATUS "OpenCV components: ${OpenCV_Components}")

# message( STATUS "[PackageConfig] Package Common's Components include: ${Common_Components}" )



