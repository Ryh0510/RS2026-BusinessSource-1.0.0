#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace common::license
{

struct LicenseRequirement
{
    std::string productId;
    std::string moduleId;
    std::string featureId;
    std::string canonicalFeatureName;
    std::vector<std::string> acceptedFeatureAliases;
};

struct LicenseSearchOptions
{
    std::vector<std::filesystem::path> extraSearchDirectories;
    std::vector<std::string> fileNames;
    bool searchExecutableDirectory = true;
    bool searchSourceLicenseDirectory = true;
    bool searchWorkingDirectory = true;
    bool recursiveLicenseDirectories = true;
};

enum class AuthorizationStatus
{
    Authorized,
    LicenseFileNotFound,
    InvalidLicense,
    ProductMismatch,
    FeatureNotAllowed,
    HardwareMismatch,
    Expired,
    InternalError
};

struct AuthorizationResult
{
    AuthorizationStatus status = AuthorizationStatus::InternalError;
    std::string message;
    std::filesystem::path licenseFile;
    std::vector<std::filesystem::path> searchedPaths;
    std::vector<std::string> enabledFeatures;
    std::string diagnosticsJson;

    bool isAuthorized() const
    {
        return status == AuthorizationStatus::Authorized;
    }
};

class LicenseVerifier
{
public:
    AuthorizationResult verify(
        const LicenseRequirement& requirement,
        const LicenseSearchOptions& options = LicenseSearchOptions{}) const;
};

std::string toString(AuthorizationStatus status);

} // namespace common::license
