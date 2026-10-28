# Machine defaults only; command-line cache paths take precedence.
set(RS2026_SELECTED_PROFILE TANGTANG_4090_Windows)
set(RS2026_PROFILE_PREBUILD_DEFAULT "D:/PreBuild")
set(RS2026_PROFILE_BOOST_FINDER CMake_FindBoost1.78.0)
set(RS2026_PROFILE_ASSIMP_FINDER CMake_FindAssimp6.0.2)
set(RS2026_PROFILE_FCL_FINDER CMake_FindFCL)
set(RS2026_PROFILE_OPENSSL_FINDER CMake_FindOpenSSL3.6.1)
set(RS2026_PROFILE_FCL_PREREQUISITE_FINDERS "")
include("${CMAKE_CURRENT_LIST_DIR}/../WindowsDependencyProviders.cmake")
