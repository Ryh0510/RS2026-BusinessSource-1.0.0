#############################################################
#	User TANGTANG p15v3 Laptop
#   Configuration on Ubuntu 22.04
#   Configurations includes:

#   1. Head only pakcages
#   --  glm
#   --  Eigen

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


set( message_header "[Find thirdparty libraries for USER_TANGTANG_p15v3_Ubuntu2204] :" )

set( SAVE_MODULE_PATH ${CMAKE_MODULE_PATH} )


set( CMAKE_MODULE_PATH ${SMROBOT_THIRDPARTY_ROOT} )
#   1.1. Find glm
include( Findglm_3rdParty )

#   1.2. Find Eigen
include( FindEigen_3rdParty )

#   1.3. Find json
include( Findnlohmann_json_3rdParty )


set( CMAKE_MODULE_PATH ${SMROBOT_THIRDPARTY_ROOT}/Ubuntu )

message( STATUS "CMAKE_MODULE_PATH is: ${CMAKE_MODULE_PATH}" )
#   1. Find Boost 1.78.0
# set( Boost_DIR )
# include( CMake_FindBoost178 )
include( CMake_FindBoost176 )

#   2. Find Qt 5.15.8
set( PackDIR_Qt5 "/opt/Qt5.15.8/lib/cmake/Qt5" )
include( CMake_FindQt515x )

#   3. Find glfw3
# set( PackDIR_GLFW3 "/usr/lib/x86_64-linux-gnu/cmake/glfw3" )
include( CMake_FindGLFW3_System )

#   4. Find glut: System glut, Custom freeglut
include( CMake_Findfreeglut_System )

#   6. Find OpenSSL: System OpenSSL, Custom OpenSSL
include( CMake_FindOpenSSL_System )

#   7. Find assimp: System assimp, Custom assimp
include( CMake_Findassimp_System )

#   8. Find fcl: System fcl, Custom fcl
include( CMake_Findfcl_System )

#   9. Find fcl: System fcl, Custom fcl
include( CMake_Findpinocchio_System )

include( CMake_FindKTX )

#   Define the third party directory for finding package Eigen, glm
set( THIRDPARTY_HEADONLY_DIR ${SMROBOT_THIRDPARTY_ROOT} CACHE PATH "The third party head only directory in Both Windows/Ubuntu System" FORCE )

#   Reset the CMAKE_MODULE_PATH and CMAKE_PREFIX_PATH back
set( CMAKE_MODULE_PATH ${SAVE_MODULE_PATH} )
set( CMAKE_PREFIX_PATH ${SAVE_PREFIX_PATH} )

