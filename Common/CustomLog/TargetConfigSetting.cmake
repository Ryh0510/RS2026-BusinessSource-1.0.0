##########################################################################
#	CMake requirement
##########################################################################
cmake_minimum_required( VERSION 3.20 )

#   Set the requirement for current target
set( ${TARGET_NAME}_RequiredLibsPublic
)

set( ${TARGET_NAME}_RequiredLibsPrivate
    Boost::log
    Boost::log_setup
    Boost::headers
    Boost::timer
    Boost::filesystem
    Boost::locale
)

