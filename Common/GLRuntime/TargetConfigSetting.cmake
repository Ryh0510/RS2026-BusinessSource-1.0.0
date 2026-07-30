##########################################################################
#	CMake requirement
##########################################################################
cmake_minimum_required( VERSION 3.20 )

#   Set the requirement for current target
set( ${TARGET_NAME}_RequiredLibsPublic

)
# set( ${TARGET_NAME}_RequiredLibsPublic ${${TARGET_NAME}_RequiredLibsPublic_Local} PARENT_SCOPE)


set( ${TARGET_NAME}_RequiredLibsPrivate
    OpenGL::GL
    Common::CustomLog
)
# set( ${TARGET_NAME}_RequiredLibsPrivate ${${TARGET_NAME}_RequiredLibsPrivate_Local} PARENT_SCOPE)

