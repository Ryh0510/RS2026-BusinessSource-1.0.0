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
set( ${PACKAGE_NAME}_ExportComponents
    RobotViewerCore
    RobotQtViewer
    RobotSDKVisualQuickStart
)

##########################################################################
#	Package ${PACKAGE_NAME} components definition
##########################################################################
#   1. Get all targets' names

set( ExcludeTargets 
    RobotViewerCore
    RobotGlfwViewer
    diagnostics
    docs
    examples
    external_validation
    feature_probes
    regression
    tutorials
)
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


##########################################################################
#	Package ${PACKAGE_NAME} required targets
##########################################################################

#   2. Get all targets' required libs
rs_begin_package_requirements(${PACKAGE_NAME})
set(_apps_metadata_components ${${PACKAGE_NAME}_ExportComponents} RobotQtModules)
foreach( TARGET_NAME ${_apps_metadata_components} )
    include( ${PACKAGE_NAME}/${TARGET_NAME}/TargetConfigSetting )
    rs_collect_component_requirements(${PACKAGE_NAME} ${TARGET_NAME}) 
    set( ${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPublic ${${TARGET_NAME}_RequiredLibsPublic} )
    set( ${PACKAGE_NAME}${TARGET_NAME}_RequiredLibsPrivate ${${TARGET_NAME}_RequiredLibsPrivate} )
endforeach()



#   3. Remove duplicated libs and catolog the targets
list( REMOVE_DUPLICATES ${PACKAGE_NAME}_RequiredLibsPublic )
list( REMOVE_DUPLICATES ${PACKAGE_NAME}_RequiredLibsPrivate )
list( APPEND ${PACKAGE_NAME}_RequiredLibs ${${PACKAGE_NAME}_RequiredLibsPublic} ${${PACKAGE_NAME}_RequiredLibsPrivate} )


list( REMOVE_DUPLICATES ${PACKAGE_NAME}_RequiredLibs )
resolve_dependencies(${PACKAGE_NAME}_RequiredLibs )

#   4. Display the required libs for this package
message( STATUS "AllPackagesRequired is ${AllPackagesRequired}" )
foreach( pkg ${AllPackagesRequired} )
    message( STATUS "Finding ${pkg} with components: ${${pkg}_Components}" )
endforeach()



