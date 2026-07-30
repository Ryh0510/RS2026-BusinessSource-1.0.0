##########################################################################
#	CMake requirement
##########################################################################
cmake_minimum_required( VERSION 3.20 )

#   Set the requirement for current target
set( ${TARGET_NAME}_RequiredLibsPublic
    Boost::headers
    Boost::log
    Boost::log_setup
    Boost::date_time
    Boost::filesystem
)
# set( ${TARGET_NAME}_RequiredLibsPublic ${${TARGET_NAME}_RequiredLibsPublic_Local} PARENT_SCOPE)


set( ${TARGET_NAME}_RequiredLibsPrivate
)
# set( ${TARGET_NAME}_RequiredLibsPrivate ${${TARGET_NAME}_RequiredLibsPrivate_Local} PARENT_SCOPE)

