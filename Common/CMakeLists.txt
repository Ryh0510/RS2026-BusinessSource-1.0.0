##########################################################################
#	CMake requirement
##########################################################################
cmake_minimum_required( VERSION 3.20 )

##########################################################################
#	Package ${PACKAGE_NAME} 
##########################################################################
get_filename_component( PACKAGE_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME )
set( PKG_MSG_HEADER "[PKG-${PACKAGE_NAME}]:" )

#   Display all the components of this package
message( STATUS "${PKG_MSG_HEADER} Package ${PACKAGE_NAME}'s Components include: ${${PACKAGE_NAME}_Components}" )
foreach( comp_ ${${PACKAGE_NAME}_Components} )
    message( STATUS "  ----  ${comp_}" )
endforeach()
message( STATUS " " )

#   Display all the required libs for this package
message( STATUS "${PKG_MSG_HEADER} ${PACKAGE_NAME}_RequiredLibsPublic: ${${PACKAGE_NAME}_RequiredLibsPublic}" )
message( STATUS "${PKG_MSG_HEADER} ${PACKAGE_NAME}_RequiredLibsPrivate: ${${PACKAGE_NAME}_RequiredLibsPrivate}" )

#   Display all the required libs for each components of this package
foreach( comp_ ${${PACKAGE_NAME}_Components} )
    message( STATUS "${PKG_MSG_HEADER} ${PACKAGE_NAME}${comp_}_RequiredLibsPublic: ${${PACKAGE_NAME}${comp_}_RequiredLibsPublic}" )
    message( STATUS "${PKG_MSG_HEADER} ${PACKAGE_NAME}${comp_}_RequiredLibsPrivate: ${${PACKAGE_NAME}${comp_}_RequiredLibsPrivate}" )
endforeach()

##########################################################################
#	Set all export option for each components of package ${PACKAGE_NAME}
##########################################################################
#   Set the export option for each target
option( ExportPacakge${PACKAGE_NAME} "Export package -- ${PACKAGE_NAME}" ON )
foreach( export_comp_ ${${PACKAGE_NAME}_ExportComponents} )
    cmake_dependent_option( ExportPacakge${PACKAGE_NAME}_${export_comp_} "${PACKAGE_NAME} : export ${export_comp_}" ON "ExportPacakge${PACKAGE_NAME}" OFF )
endforeach()


##########################################################################
#	Build ${PACKAGE_NAME}
##########################################################################
message( STATUS "BuildPackage_${PACKAGE_NAME} is ${BuildPackage_${PACKAGE_NAME}} in ${PACKAGE_NAME} directory." )

if( UsingPrebuilt_${PACKAGE_NAME} )
    message( STATUS "Use prebuilt package ${PACKAGE_NAME}; source components are not added." )
    rs_any_example_category_enabled( _build_examples ${PACKAGE_NAME} )
    if( _build_examples )
        foreach( each_comp_ ${${PACKAGE_NAME}_Components} )
            rs_add_prebuilt_component_example_roots( ${PACKAGE_NAME} ${each_comp_} )
        endforeach()
    endif()
else()
    #   Build from source
    message( STATUS "Build package ${PACKAGE_NAME} on site" )
    foreach( each_comp_ ${${PACKAGE_NAME}_Components} )
        cmake_dependent_option( Build${PACKAGE_NAME}_${each_comp_} "Build ${PACKAGE_NAME}::${each_comp_}" ON "BuildPackage_${PACKAGE_NAME}" OFF )
        if( Build${PACKAGE_NAME}_${each_comp_} )
            add_subdirectory( ${each_comp_} )
        else()
            message( STATUS "Build${PACKAGE_NAME}_${each_comp_} is ${Build${PACKAGE_NAME}_${each_comp_}} in ${PACKAGE_NAME} directory." )
        endif()
    endforeach()
endif()



# #############################################################
# #	Build ${PACKAGE_NAME} Example
# #############################################################
# if( BuildExample_${PACKAGE_NAME} )
#     foreach( each_comp_ ${${PACKAGE_NAME}_Components} )
#         set( TARGET_NAME ${each_comp_} )
#         add_subdirectory( ${each_comp_}/example )
#     endforeach()
# endif()



##########################################################################
#   Generate package
##########################################################################
if( ${ExportPacakge${PACKAGE_NAME}} AND NOT UsingPrebuilt_${PACKAGE_NAME} )
    include(CMakePackageConfigHelpers)
    set(SUPPORTED_COMPONENTS ${${PACKAGE_NAME}_PublicComponents} ${${PACKAGE_NAME}_CompatibilityComponents})
    set(SDK_PACKAGE_INSTALL_COMPONENT "smrobot_sdk_${PACKAGE_NAME}_package")

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}Config.cmake
        INSTALL_DESTINATION ${PACKAGE_NAME}/lib/cmake
    )

    write_basic_package_version_file(
        ${PACKAGE_NAME}ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}ConfigVersion.cmake
        DESTINATION ${PACKAGE_NAME}/lib/cmake
        COMPONENT ${SDK_PACKAGE_INSTALL_COMPONENT}
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}ConfigVersion.cmake
        DESTINATION ${PACKAGE_NAME}/lib/cmake/${PACKAGE_NAME}
        COMPONENT ${SDK_PACKAGE_INSTALL_COMPONENT}
    )
endif()



