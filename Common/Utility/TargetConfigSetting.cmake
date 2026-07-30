##########################################################################
#	CMake requirement
##########################################################################
cmake_minimum_required( VERSION 3.20 )

#   Set the requirement for current target
set( ${TARGET_NAME}_RequiredLibsPublic
    glm::glm
    Eigen3::Eigen
)

set( ${TARGET_NAME}_RequiredLibsPrivate
    # Boost::headers
    # Boost::timer
    # Boost::filesystem
    # Boost::locale

    # Boost::log
    # Boost::log_setup
)

