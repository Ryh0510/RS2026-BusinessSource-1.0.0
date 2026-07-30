#############################################################
#	User TANGTANG 4090
#   Configuration on Ubuntu 22.04
#   Configurations includes:
#   1. ---- Boost
#   2. ---- Qt
#   3. ---- glfw
#   4. ---- glut
#   5. ---- glew
#   6. ---- openssl
#   7. ---- assimp
#   8. ---- fcl
#   9. ---- pinocchio
#############################################################


set( message_header "[Find thirdparty libraries for USER_TANGTANG_4090_Ubuntu2204] :" )

set( SAVE_MODULE_PATH ${CMAKE_MODULE_PATH} )

#######################################################
#   Find library in thirdparty directory  
#######################################################
get_filename_component( THIRDPARTY_LIB_DIR ${SMROBOT_THIRDPARTY_ROOT} ABSOLUTE )
message( STATUS "   --  Build in library Directory: ${THIRDPARTY_LIB_DIR}" )


set( CMAKE_MODULE_PATH ${THIRDPARTY_LIB_DIR}/Ubuntu )
message( STATUS "CMAKE_MODULE_PATH is: ${CMAKE_MODULE_PATH}" )

# #   1. Find glfw3
# include( Findglfw_3rdParty )

# #   2. Find glut: System glut, Custom freeglut
# include( Findfreeglut_3rdParty )

#   3. Find glew: System glew, Custom glew
# include( Findglew_3rdParty )


#######################################################
#   Find library in system directory  
#######################################################
# set( CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/Ubuntu2204 )
# message( STATUS "CMAKE_MODULE_PATH is: ${CMAKE_MODULE_PATH}" )

#   3. Find glfw3
# set( PackDIR_GLFW3 "/usr/lib/x86_64-linux-gnu/cmake/glfw3" )
# include( CMake_FindGLFW3_System )
include( Findglfw_3rdParty )

#   4. Find glut: System glut, Custom freeglut
# include( CMake_Findfreeglut_System )
include( Findfreeglut_3rdParty )

#   4. Find Boost 1.78.0
include( CMake_FindBoost178 )

#   5. Find Qt 5.15.8
set( PackDIR_Qt5 "/opt/Qt5.15.8/lib/cmake/Qt5" )
include( CMake_FindQt515x )

#   6. Find OpenSSL: System OpenSSL, Custom OpenSSL
include( CMake_FindOpenSSL_System )
# include( CMake_FindOpenSSL )

#   7. Find assimp: System assimp, Custom assimp
# include( CMake_Findassimp_System )
#   Could be used but not compulsory
include( Findassimp_3rdParty )

#   8. Find fcl: System fcl, Custom fcl
include( CMake_Findfcl_System )

#   9. Find fcl: System fcl, Custom fcl
include( CMake_Findpinocchio_System )

#   Define the third party directory for finding package Eigen, glm
set( THIRDPARTY_HEADONLY_DIR ${SMROBOT_THIRDPARTY_ROOT} CACHE PATH "The third party head only directory in Both Windows/Ubuntu System" FORCE )

set( CMAKE_MODULE_PATH ${THIRDPARTY_HEADONLY_DIR} )
include( Findglm_3rdParty )
include( FindEigen_3rdParty )


#   Reset the CMAKE_MODULE_PATH and CMAKE_PREFIX_PATH back
set( CMAKE_MODULE_PATH ${SAVE_MODULE_PATH} )
set( CMAKE_PREFIX_PATH ${SAVE_PREFIX_PATH} )

