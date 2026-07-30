##########################################################################
# CMake requirement
##########################################################################
cmake_minimum_required( VERSION 3.20 )

set( ${TARGET_NAME}_RequiredLibsPublic
)

set( ${TARGET_NAME}_RequiredLibsPrivate
    LicenseSystem::LicenseSDK
)

set( ${TARGET_NAME}_INSTALL_PUBLIC_DEPENDENCIES
    Verification::LicenseSDK
)
