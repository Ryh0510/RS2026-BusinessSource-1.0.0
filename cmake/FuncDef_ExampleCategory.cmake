############################################################
# Example category helpers
############################################################

set( RS_EXAMPLE_CATEGORIES
    Diagnostics
    FeatureProbes
    Regression
    ExternalValidation
    Tutorials
)

set( RS_EXAMPLE_CATEGORY_DIR_Diagnostics diagnostics )
set( RS_EXAMPLE_CATEGORY_DIR_FeatureProbes feature_probes )
set( RS_EXAMPLE_CATEGORY_DIR_Regression regression )
set( RS_EXAMPLE_CATEGORY_DIR_ExternalValidation external_validation )
set( RS_EXAMPLE_CATEGORY_DIR_Tutorials tutorials )

function( rs_example_category_enabled OUT_VAR CATEGORY PACKAGE_NAME )
    set( _enabled OFF )

    if(CATEGORY STREQUAL "Regression")
        rs2026_test_subtree_selected(_enabled "${CMAKE_CURRENT_SOURCE_DIR}")
        set(${OUT_VAR} ${_enabled} PARENT_SCOPE)
        return()
    endif()

    if(CATEGORY STREQUAL "Diagnostics")
        set(_option DevTools_Diagnostics)
    elseif(CATEGORY STREQUAL "FeatureProbes")
        set(_option DevTools_FeatureProbes)
    elseif(CATEGORY STREQUAL "ExternalValidation")
        set(_option DevTools_PackageConsumers)
    elseif(CATEGORY STREQUAL "Tutorials")
        set(_option BuildTutorials)
        if(BuildExample_${PACKAGE_NAME})
            set(_enabled ON)
        endif()
    else()
        message(FATAL_ERROR "Unknown example category: ${CATEGORY}")
    endif()
    if(${_option} OR ${_option}_${PACKAGE_NAME})
        set(_enabled ON)
    endif()

    set( ${OUT_VAR} ${_enabled} PARENT_SCOPE )
endfunction()

function( rs_any_example_category_enabled OUT_VAR PACKAGE_NAME )
    set( _enabled OFF )

    if( DEFINED BuildExample_${PACKAGE_NAME} AND BuildExample_${PACKAGE_NAME} )
        set( _enabled ON )
    endif()

    foreach( _category ${RS_EXAMPLE_CATEGORIES} )
        rs_example_category_enabled( _category_enabled ${_category} ${PACKAGE_NAME} )
        if( _category_enabled )
            set( _enabled ON )
        endif()
    endforeach()

    set( ${OUT_VAR} ${_enabled} PARENT_SCOPE )
endfunction()

function( rs_add_example_subdirectory CATEGORY DIR_NAME )
    rs_example_category_enabled( _enabled ${CATEGORY} ${PACKAGE_NAME} )
    if( _enabled )
        add_subdirectory( ${DIR_NAME} )
    endif()
endfunction()

function( rs_add_example_category_roots )
    foreach( _category ${RS_EXAMPLE_CATEGORIES} )
        rs_example_category_enabled( _category_enabled ${_category} ${PACKAGE_NAME} )
        set( _category_dir ${RS_EXAMPLE_CATEGORY_DIR_${_category}} )

        if( _category_enabled AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_category_dir}/CMakeLists.txt" )
            add_subdirectory( ${_category_dir} )
        endif()
    endforeach()

    if( DEFINED BuildExample_${PACKAGE_NAME}
        AND BuildExample_${PACKAGE_NAME}
        AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/example/CMakeLists.txt" )
        add_subdirectory( example )
    endif()
endfunction()

function( rs_add_installed_sdk_examples )
    if(NOT RS2026_TEST_BUILD_SDK_EXAMPLES)
        return()
    endif()
    set( _enabled OFF )
    if( DEFINED BuildSdkExamples AND BuildSdkExamples )
        set( _enabled ON )
    endif()
    if( DEFINED BuildSdkExamples_${PACKAGE_NAME}
        AND BuildSdkExamples_${PACKAGE_NAME} )
        set( _enabled ON )
    endif()

    if( _enabled AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt" )
        add_subdirectory( examples )
    endif()
endfunction()

function( rs_add_subdirectory_if_targets_available DIR_NAME )
    set( _missing_targets )
    foreach( _required_target ${ARGN} )
        if( NOT TARGET ${_required_target} )
            list( APPEND _missing_targets ${_required_target} )
        endif()
    endforeach()

    if( _missing_targets )
        message( STATUS
            "Skip ${DIR_NAME} because required targets are missing: ${_missing_targets}"
        )
        return()
    endif()

    add_subdirectory( ${DIR_NAME} )
endfunction()

function( rs_add_prebuilt_component_example_roots PACKAGE_NAME COMPONENT_NAME )
    if( NOT TARGET ${PACKAGE_NAME}::${COMPONENT_NAME} )
        message( STATUS
            "Skip examples for prebuilt ${PACKAGE_NAME}::${COMPONENT_NAME}; target was not found."
        )
        return()
    endif()

    set( _component_dir "${CMAKE_CURRENT_SOURCE_DIR}/${COMPONENT_NAME}" )
    if( NOT EXISTS "${_component_dir}" )
        return()
    endif()

    set( TARGET_NAME ${COMPONENT_NAME} )
    foreach( _category ${RS_EXAMPLE_CATEGORIES} )
        rs_example_category_enabled( _category_enabled ${_category} ${PACKAGE_NAME} )
        set( _category_dir ${RS_EXAMPLE_CATEGORY_DIR_${_category}} )

        if( _category_enabled AND EXISTS "${_component_dir}/${_category_dir}/CMakeLists.txt" )
            add_subdirectory(
                "${_component_dir}/${_category_dir}"
                "${COMPONENT_NAME}/${_category_dir}"
            )
        endif()
    endforeach()

    if( DEFINED BuildExample_${PACKAGE_NAME}
        AND BuildExample_${PACKAGE_NAME}
        AND EXISTS "${_component_dir}/example/CMakeLists.txt" )
        add_subdirectory(
            "${_component_dir}/example"
            "${COMPONENT_NAME}/example"
        )
    endif()
endfunction()

function( rs_set_example_target_folder TARGET_NAME CATEGORY MODULE_NAME )
    if( TARGET ${TARGET_NAME} )
        set_target_properties( ${TARGET_NAME} PROPERTIES
            FOLDER "${CATEGORY}/${PACKAGE_NAME}/${MODULE_NAME}"
        )
    endif()
endfunction()

# Optional diagnostic, probe, and external-validation programs are not
# catalog CTest units. Keep them separate from governed Tests in Visual Studio.
function( rs_group_optional_program_targets SOURCE_DIR )
    get_property(_targets DIRECTORY "${SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_target IN LISTS _targets)
        get_target_property(_folder ${_target} FOLDER)
        if(_folder MATCHES "^(Diagnostics|FeatureProbes|ExternalValidation)(/|$)")
            if(NOT _folder MATCHES "^(Diagnostics|FeatureProbes|ExternalValidation)/([^/]+)/([^/]+)$")
                message(FATAL_ERROR
                    "Optional program ${_target} needs a Category/Package/Component FOLDER: ${_folder}")
            endif()
            set(_category "${CMAKE_MATCH_1}")
            if(_category STREQUAL "ExternalValidation")
                set(_category PackageConsumers)
            endif()
            set_target_properties(${_target} PROPERTIES
                FOLDER "DeveloperTools/${CMAKE_MATCH_2}/${CMAKE_MATCH_3}/${_category}")
        endif()
    endforeach()
    get_property(_children DIRECTORY "${SOURCE_DIR}" PROPERTY SUBDIRECTORIES)
    foreach(_child IN LISTS _children)
        rs_group_optional_program_targets("${_child}")
    endforeach()
endfunction()
