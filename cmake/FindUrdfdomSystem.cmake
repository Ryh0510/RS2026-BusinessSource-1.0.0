# Keep URDF parsing independent from optional Pinocchio capabilities.
if(PINOCCHIO_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${PINOCCHIO_DIR}")
endif()
find_package(console_bridge CONFIG REQUIRED)
find_package(urdfdom_headers CONFIG REQUIRED)
find_package(urdfdom CONFIG REQUIRED)
