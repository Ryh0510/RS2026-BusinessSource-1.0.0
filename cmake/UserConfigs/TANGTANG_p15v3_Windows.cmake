#################################################################################
#   Find dependant packages for user tangtang 4090 Win
#   1. Head only pakcages
#   --  glm
#   --  Eigen
#
#   2. Third-Party Packages:
#   --  glfw
#   --  freeglut
#   --  glew
#
#   3. Prebuild Packages:
#   --  Boost   1.78.0
#   --  Assimp  static
#   --  FCL
#
#   --  Qt      5.15.5
#   --  OpenSSL 
#   Customized Packages (Using prebuilt):
#   --  
#   Written by Tang Qing in March. 2025.
#################################################################################


#   Information
#   Current file output
get_filename_component( this_cmake_file ${CMAKE_CURRENT_LIST_FILE} ABSOLUTE )
message( STATUS "\n------------------ Find packages in ${this_cmake_file} ------------------" )


set( SAVE_MODULE_PATH ${CMAKE_MODULE_PATH} )
include( CMakePrintHelpers )

#   Define prebuild directory
set( PREBUILD_DIR D:/PreBuild CACHE PATH "The prebuild directory in Windows" FORCE )
get_filename_component( BUILDINLIB_DIR ${SMROBOT_THIRDPARTY_ROOT} ABSOLUTE )
get_filename_component( WIN_VSIndep_DIR ${PREBUILD_DIR}/vs_indep/cmake ABSOLUTE )
get_filename_component( WIN_VS2019_DIR ${PREBUILD_DIR}/vs2019/cmake ABSOLUTE )
message( STATUS "   --  Build in library Directory: ${BUILDINLIB_DIR}" )
message( STATUS "   --  Prebuild VSIndep Directory: ${WIN_VSIndep_DIR}" )
message( STATUS "   --  Prebuild VS2019 Directory: ${WIN_VS2019_DIR}" )



#   1. Head only pakcages
#   --  glm
#   --  Eigen
set( CMAKE_MODULE_PATH ${BUILDINLIB_DIR} )
message( STATUS "CMAKE_MODULE_PATH for header only package is ${CMAKE_MODULE_PATH}" )

#   1.1 Find glm
include( Findglm_3rdParty )
#   1.2. Find Eigen
include( FindEigen_3rdParty )
#   1.3. Find json
include( Findnlohmann_json_3rdParty )


#   2. Third-Party Packages
set( CMAKE_MODULE_PATH ${BUILDINLIB_DIR}/Windows )
include( Findglfw_3rdParty )
include( Findfreeglut_3rdParty )
include( Findpinocchio_3rdParty )
include( FindCoACD_3rdParty )

#   3. Prebuild Packages:
set( CMAKE_MODULE_PATH ${WIN_VS2019_DIR} ) 
#   3.1. Find boost
# set( Boost_USE_STATIC_LIBS           	OFF )    # only find static libs
# # If you need to build a debug program, the following options should be checked (ON).
# set( Boost_USE_DEBUG_LIBS            	ON )	# ignore debug libs and
# set( Boost_USE_RELEASE_LIBS          	ON )    # only find release libs
# set( Boost_USE_MULTITHREADED        	ON )
# set( Boost_USE_STATIC_RUNTIME        	OFF )

set( Boost_Components 
    atomic locale date_time filesystem timer regex thread serialization system program_options 
    python random exception log log_setup unit_test_framework json iostreams )
include( CMake_FindBoost1.78.0 )



# #   FCL static - needed by SMRobotGen1::ModelTangQing
# include( CMake_FindFCL )
# include( CMake_Findcoal )

include( CMake_FindCCD )
include( CMake_Findoctomap1.10.0)
include( CMake_FindFCL0.7.0 )
include( CMake_FindOMPL )
include( CMake_FindKtx )


#   Assimp static - needed by SMRobotGen1::ModelTangQing
# set(ON TRUE)				#	For assimp : True - SHARED; False - STATIC
include( CMake_FindAssimp6.0.2 )




set( CMAKE_MODULE_PATH ${WIN_VSIndep_DIR} ) 
include( CMake_FindQt5155 )

if( ${BuildPackage_Verification} )
    include( CMake_FindOpenSSL3.6.1 )
endif()


# #   Add simple package
# add_subdirectory( ${PROJECT_SOURCE_DIR}/thirdparty/tinyxml2 )


set( CMAKE_MODULE_PATH ${SAVE_MODULE_PATH} )
message( STATUS "\n------------------ End of finding packages in ${this_cmake_file} ------------------" )


