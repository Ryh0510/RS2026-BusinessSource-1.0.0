cmake_minimum_required(VERSION 3.20)

project(SMRobotLicenseAuthorizationConsumer LANGUAGES CXX)

include(CTest)

set(SMROBOTGEN2_PACKAGE_LICENSE_DIR "" CACHE PATH
    "Optional license search directory for the package authorization consumer test")

find_package(Common CONFIG REQUIRED COMPONENTS LicenseVerification)
find_package(SMRobotCore CONFIG REQUIRED COMPONENTS RobotCoreAuthorization)
find_package(SMRobotPlatform CONFIG REQUIRED COMPONENTS RobotPlatformAuthorization)

add_executable(LicenseAuthorizationConsumer main.cpp)
target_compile_features(LicenseAuthorizationConsumer PRIVATE cxx_std_17)
target_link_libraries(LicenseAuthorizationConsumer
    PRIVATE
        Common::LicenseVerification
        SMRobotCore::RobotCoreAuthorization
        SMRobotPlatform::RobotPlatformAuthorization
)

if(BUILD_TESTING)
    set(_license_authorization_consumer_args)
    if(SMROBOTGEN2_PACKAGE_LICENSE_DIR)
        list(APPEND _license_authorization_consumer_args "${SMROBOTGEN2_PACKAGE_LICENSE_DIR}")
    endif()

    add_test(
        NAME SMRobotLicenseAuthorizationPackageConsumer
        COMMAND LicenseAuthorizationConsumer ${_license_authorization_consumer_args}
    )
endif()
