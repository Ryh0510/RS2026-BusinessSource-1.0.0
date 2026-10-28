include("${CMAKE_CURRENT_LIST_DIR}/DependencyProviders.cmake")
set(SAVE_MODULE_PATH "${CMAKE_MODULE_PATH}")
include(CMakePrintHelpers)
set(_rs_prebuild_default "${RS2026_PROFILE_PREBUILD_DEFAULT}")
if(DEFINED ENV{RS2026_EXTERNAL_PREBUILD_ROOT} AND NOT "$ENV{RS2026_EXTERNAL_PREBUILD_ROOT}" STREQUAL "")
    set(_rs_prebuild_default "$ENV{RS2026_EXTERNAL_PREBUILD_ROOT}")
endif()
set(PREBUILD_DIR "${_rs_prebuild_default}" CACHE PATH "External Windows dependency root for the selected machine")
get_filename_component(BUILDINLIB_DIR "${SMROBOT_THIRDPARTY_ROOT}" ABSOLUTE)
get_filename_component(WIN_VSIndep_DIR "${PREBUILD_DIR}/vs_indep/cmake" ABSOLUTE)
get_filename_component(WIN_VS2019_DIR "${PREBUILD_DIR}/vs2019/cmake" ABSOLUTE)
message(STATUS "[Dependency ${RS2026_SELECTED_PROFILE}] PREBUILD_DIR=${PREBUILD_DIR}")
macro(_rsprovider_find_qt5_exact)
    if(EXISTS "${RS2026_DEPENDENCY_PROVIDER_OVERRIDE_DIR}/CMake_FindQt5155.cmake")
        rs_include_dependency_provider(CMake_FindQt5155)
    else()
        rs_project_required_components(Qt5 _rsprovider_qt5_components)
        list(REMOVE_DUPLICATES _rsprovider_qt5_components)
        if(NOT _rsprovider_qt5_components)
            message(FATAL_ERROR
                "[Dependency ${RS2026_SELECTED_PROFILE}] Qt5 was selected without required components.")
        endif()

        set(_rsprovider_qt5_root "${PREBUILD_DIR}/vs_indep/Qt5155_x64")
        set(_rsprovider_qt5_cmake_dir "${_rsprovider_qt5_root}/lib/cmake/Qt5")
        if(NOT EXISTS "${_rsprovider_qt5_cmake_dir}/Qt5Config.cmake")
            message(FATAL_ERROR
                "Qt 5.15.5 config package is missing: "
                "${_rsprovider_qt5_cmake_dir}/Qt5Config.cmake")
        endif()

        message(STATUS
            "[Dependency ${RS2026_SELECTED_PROFILE}] Qt5 exact components: ${_rsprovider_qt5_components}")
        find_package(Qt5 5.15.5 CONFIG REQUIRED
            COMPONENTS ${_rsprovider_qt5_components}
            PATHS "${_rsprovider_qt5_cmake_dir}"
            NO_DEFAULT_PATH
        )

        set(Qt5_RUNTIME_DIR "${_rsprovider_qt5_root}/bin")
        set(Qt5_PLATFORM_DIR "${_rsprovider_qt5_root}/plugins")
        if(NOT TARGET Qt5::qmake)
            message(FATAL_ERROR
                "Qt5 exact provider did not provide Qt5::qmake for runtime deployment.")
        endif()
        if(NOT EXISTS "${Qt5_PLATFORM_DIR}/platforms/qwindows.dll")
            message(FATAL_ERROR
                "Qt5 platform plugin is missing: "
                "${Qt5_PLATFORM_DIR}/platforms/qwindows.dll")
        endif()

        foreach(_component IN LISTS _rsprovider_qt5_components)
            rs_assert_required_targets(Qt5 "Qt5::${_component}")
        endforeach()
    endif()
    rs_assert_requested_provider_targets(Qt5)
endmacro()

if(RS2026_DEPENDENCY_REPORT)
    message(STATUS
        "[Dependency ${RS2026_SELECTED_PROFILE}] demand loading enabled: "
        "${RS2026_ENABLE_DEMAND_LOADING}")
    message(STATUS
        "[Dependency ${RS2026_SELECTED_PROFILE}] snapshot complete: "
        "${RS2026_DEPENDENCY_SNAPSHOT_COMPLETE}")
    message(STATUS
        "[Dependency ${RS2026_SELECTED_PROFILE}] required packages: ${RS2026_REQUIRED_PACKAGES}")
endif()

rs_prebuilt_component_required(
    _rsprovider_prebuilt_core_robot_io SMRobotCore RobotIO)
rs_prebuilt_component_required(
    _rsprovider_prebuilt_platform_asset_core SMRobotPlatform AssetCore)
rs_prebuilt_component_required(
    _rsprovider_prebuilt_platform_render_core SMRobotPlatform RenderCore)
rs_prebuilt_component_required(
    _rsprovider_prebuilt_platform_scene_core SMRobotPlatform SceneCore)

set(_rsprovider_prebuilt_platform_asset_stack OFF)
if(_rsprovider_prebuilt_platform_asset_core OR
   _rsprovider_prebuilt_platform_render_core OR
   _rsprovider_prebuilt_platform_scene_core)
    set(_rsprovider_prebuilt_platform_asset_stack ON)
endif()



#   1. Head only pakcages
#   --  glm
#   --  Eigen
set( CMAKE_MODULE_PATH ${BUILDINLIB_DIR} )

#   1.1 Find glm
rs_dependency_provider_required(_rsprovider_load_glm glm glm)
if(_rsprovider_prebuilt_platform_asset_stack)
    rs_force_dependency_provider(_rsprovider_load_glm glm
        "requested prebuilt SMRobotPlatform asset/render/scene component")
endif()
if(_rsprovider_load_glm)
    rs_include_dependency_provider(Findglm_3rdParty)
    rs_assert_dependency_provider_targets(glm glm::glm)
endif()
#   1.2. Find Eigen
rs_dependency_provider_required(_rsprovider_load_eigen Eigen3
    Eigen3 ompl fcl ccd octomap pinocchio urdfdom urdfdom_headers console_bridge)
if((DEFINED UsingPrebuilt_SMRobotCore AND UsingPrebuilt_SMRobotCore) OR
   (DEFINED UsingPrebuilt_SMRobotPlatform AND UsingPrebuilt_SMRobotPlatform))
    rs_force_dependency_provider(_rsprovider_load_eigen Eigen3
        "requested prebuilt Core/Platform components expose Eigen3 interfaces")
endif()
if(_rsprovider_load_eigen)
    rs_include_dependency_provider(FindEigen_3rdParty)
    rs_assert_dependency_provider_targets(Eigen3 Eigen3::Eigen)
endif()
#   1.3. Find json
rs_dependency_provider_required(_rsprovider_load_nlohmann_json nlohmann_json nlohmann_json)
if(_rsprovider_load_nlohmann_json)
    rs_include_dependency_provider(Findnlohmann_json_3rdParty)
    rs_assert_dependency_provider_targets(nlohmann_json nlohmann_json::nlohmann_json)
endif()


#   2. Third-Party Packages
set( CMAKE_MODULE_PATH ${BUILDINLIB_DIR}/Windows )
if(BUILD_TESTING OR BuildExample_SMRobotCore)
    rs_include_dependency_provider(FindGTest_3rdParty)
endif()

rs_optional_glfw_required(_rsprovider_optional_glfw_required)
rs_dependency_provider_required(_rsprovider_load_glfw glfw glfw)
if(_rsprovider_optional_glfw_required)
    set(_rsprovider_load_glfw ON)
    if(RS2026_DEPENDENCY_REPORT)
        message(STATUS
            "[Dependency ${RS2026_SELECTED_PROFILE}] glfw: force actual=LOAD because an optional example "
            "category is enabled")
    endif()
endif()
if(_rsprovider_load_glfw)
    rs_include_dependency_provider(Findglfw_3rdParty)
    rs_assert_dependency_provider_targets(glfw glfw::glfw)
endif()

rs_dependency_provider_required(_rsprovider_load_freeglut FreeGLUT FreeGLUT)
rs_optional_freeglut_required(_rsprovider_optional_freeglut_required)
if(_rsprovider_optional_freeglut_required)
    rs_force_dependency_provider(_rsprovider_load_freeglut FreeGLUT
        "enabled diagnostic or feature probe requires FreeGLUT")
endif()
if(_rsprovider_load_freeglut)
    rs_include_dependency_provider(Findfreeglut_3rdParty)
    rs_assert_dependency_provider_targets(FreeGLUT FreeGLUT::freeglut)
endif()

rs_dependency_provider_required(_rsprovider_load_pinocchio pinocchio/urdfdom
    pinocchio urdfdom urdfdom_headers console_bridge)
if(_rsprovider_prebuilt_core_robot_io)
    rs_force_dependency_provider(_rsprovider_load_pinocchio pinocchio/urdfdom
        "prebuilt SMRobotCore::RobotIO runtime dependencies")
endif()
if(_rsprovider_load_pinocchio)
    rs_include_dependency_provider(Findpinocchio_3rdParty)
endif()

rs_dependency_provider_required(_rsprovider_load_coacd CoACD CoACD)
if(_rsprovider_load_coacd)
    rs_include_dependency_provider(FindCoACD_3rdParty)
    rs_assert_dependency_provider_targets(CoACD CoACD::coacd)
endif()

#   3. Prebuild Packages:
set( CMAKE_MODULE_PATH ${WIN_VS2019_DIR} ) 

#   Find boost
set( Boost_USE_STATIC_LIBS           	OFF )    # only find static libs
# If you need to build a debug program, the following options should be checked (ON).
set( Boost_USE_DEBUG_LIBS            	ON )	# ignore debug libs and
set( Boost_USE_RELEASE_LIBS          	ON )    # only find release libs
set( Boost_USE_MULTITHREADED        	ON )
set( Boost_USE_STATIC_RUNTIME        	OFF )       # VS default to MD
rs_project_required_components(Boost _rsprovider_boost_direct_components)
set(_rsprovider_boost_provider_components)
rs_project_requires_package(ompl _rsprovider_ompl_required)
if(_rsprovider_ompl_required)
    list(APPEND _rsprovider_boost_provider_components serialization)
endif()
if(_rsprovider_load_pinocchio)
    list(APPEND _rsprovider_boost_provider_components filesystem system serialization)
endif()
rs_optional_boost_program_options_required(_rsprovider_optional_program_options)
if(_rsprovider_optional_program_options)
    list(APPEND _rsprovider_boost_provider_components program_options)
endif()
set(_rsprovider_boost_effective_components
    ${_rsprovider_boost_direct_components}
    ${_rsprovider_boost_provider_components})
if(_rsprovider_boost_effective_components)
    # The current Boost finder queries Boost::headers unconditionally.
    list(APPEND _rsprovider_boost_effective_components headers)
    list(REMOVE_DUPLICATES _rsprovider_boost_effective_components)
endif()

rs_dependency_provider_required(_rsprovider_load_boost Boost
    Boost ompl pinocchio urdfdom urdfdom_headers console_bridge)
if(_rsprovider_optional_program_options)
    rs_force_dependency_provider(_rsprovider_load_boost Boost
        "enabled Platform feature probe requires Boost program_options")
endif()
if(RS2026_DEPENDENCY_REPORT)
    message(STATUS
        "[Dependency ${RS2026_SELECTED_PROFILE}] Boost direct components: "
        "${_rsprovider_boost_direct_components}")
    message(STATUS
        "[Dependency ${RS2026_SELECTED_PROFILE}] Boost provider components: "
        "${_rsprovider_boost_provider_components}")
    message(STATUS
        "[Dependency ${RS2026_SELECTED_PROFILE}] Boost effective components: "
        "${_rsprovider_boost_effective_components}")
endif()
if(_rsprovider_load_boost)
    if(RS2026_ENABLE_DEMAND_LOADING)
        set(Boost_Components ${_rsprovider_boost_effective_components})
    endif()
    rs_include_dependency_provider(${RS2026_PROFILE_BOOST_FINDER})
    if(RS2026_ENABLE_DEMAND_LOADING)
        foreach(_component IN LISTS _rsprovider_boost_effective_components)
            rs_assert_required_targets(Boost "Boost::${_component}")
        endforeach()
    endif()
endif()


rs_dependency_provider_required(_rsprovider_load_assimp assimp assimp)
if(_rsprovider_prebuilt_platform_asset_stack)
    rs_force_dependency_provider(_rsprovider_load_assimp assimp
        "prebuilt SMRobotPlatform asset/render/scene runtime dependency")
endif()
if(_rsprovider_load_assimp)
    rs_include_dependency_provider(${RS2026_PROFILE_ASSIMP_FINDER})
    rs_assert_dependency_provider_targets(assimp assimp::assimp)
endif()

rs_dependency_provider_required(_rsprovider_load_fcl FCL fcl ccd octomap)
if(_rsprovider_load_fcl)
    foreach(_finder IN LISTS RS2026_PROFILE_FCL_PREREQUISITE_FINDERS)
        rs_include_dependency_provider(${_finder})
    endforeach()
    rs_include_dependency_provider(${RS2026_PROFILE_FCL_FINDER})
    rs_assert_dependency_provider_targets(FCL fcl::fcl)
endif()

rs_dependency_provider_required(_rsprovider_load_ompl OMPL ompl)
if(_rsprovider_load_ompl)
    rs_include_dependency_provider(CMake_FindOMPL)
    rs_assert_dependency_provider_targets(OMPL ompl::ompl)
endif()

rs_dependency_provider_required(_rsprovider_load_ktx KTX KTX)
if(_rsprovider_load_ktx
   AND _rsprovider_prebuilt_platform_asset_stack
   AND RS2026_DEPENDENCY_SNAPSHOT_COMPLETE)
    rs_project_requires_package(KTX _rsprovider_source_requires_ktx)
    if(NOT _rsprovider_source_requires_ktx)
        set(_rsprovider_load_ktx OFF)
        if(RS2026_DEPENDENCY_REPORT)
            message(STATUS
                "[Dependency ${RS2026_SELECTED_PROFILE}] KTX: actual=SKIP, reason=private static "
                "dependency is already contained in prebuilt SMRobotPlatform::RenderCore")
        endif()
    endif()
    unset(_rsprovider_source_requires_ktx)
endif()
if(_rsprovider_load_ktx)
    rs_include_dependency_provider(CMake_FindKtx)
    rs_assert_dependency_provider_targets(KTX KTX::ktx)
endif()

# if( NOT ${UsingPrebuild_ALL} )
#     #   Assimp static - needed by SMRobotGen1::ModelTangQing
#     rs_include_dependency_provider(${RS2026_PROFILE_ASSIMP_FINDER})

#     #   FCL static - needed by SMRobotGen1::ModelTangQing
#     rs_include_dependency_provider(${RS2026_PROFILE_FCL_FINDER})
# endif()


set( CMAKE_MODULE_PATH ${WIN_VSIndep_DIR} ) 
rs_dependency_provider_required(_rsprovider_load_qt5 Qt5 Qt5)
if(_rsprovider_load_qt5)
    if(RS2026_ENABLE_DEMAND_LOADING AND RS2026_DEPENDENCY_SNAPSHOT_COMPLETE)
        _rsprovider_find_qt5_exact()
    else()
        rs_include_dependency_provider(CMake_FindQt5155)
    endif()
endif()


rs_dependency_provider_required(_rsprovider_load_openssl OpenSSL OpenSSL)
if(_rsprovider_load_openssl)
    rs_include_dependency_provider(${RS2026_PROFILE_OPENSSL_FINDER})
endif()
# if( NOT ${UsingPrebuild_ALL} )
#     if( ${BuildPackage_Verification} )
#         rs_include_dependency_provider(CMake_FindOpenSSL)
#     endif()
# endif()


rs_dependency_provider_required(_rsprovider_load_opengl OpenGL OpenGL)
if(_rsprovider_load_opengl)
    if(EXISTS "${RS2026_DEPENDENCY_PROVIDER_OVERRIDE_DIR}/FindOpenGL.cmake")
        rs_include_dependency_provider(FindOpenGL)
    else()
        find_package( OpenGL REQUIRED )
    endif()
endif()
#    find_package( OpenGL REQUIRED COMPONENTS OpenGL EGL )
if(_rsprovider_load_opengl AND OpenGL_FOUND )
    message( STATUS "OpenGL is FOUND!" )
    if( TARGET OpenGL::GL )
        message( STATUS "Target OpenGL::GL is FOUND!" )
    else()
        message( STATUS "Target OpenGL::GL is NOT found!" )
    endif()
elseif(_rsprovider_load_opengl)
    message( WARNING "OpenGL is NOT found!" )
endif()





set( CMAKE_MODULE_PATH ${SAVE_MODULE_PATH} )
message( STATUS "\n------------------ End of finding packages in ${RS2026_SELECTED_PROFILE} ------------------" )
