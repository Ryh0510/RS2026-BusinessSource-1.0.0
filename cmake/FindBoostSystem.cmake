# Prefer Boost's own Config package; do not depend on removed FindBoost policies.
if(NOT Boost_Components)
    set(Boost_Components headers)
endif()
find_package(Boost CONFIG REQUIRED COMPONENTS ${Boost_Components})
message(STATUS "System Boost ${Boost_VERSION}: ${Boost_Components}")
