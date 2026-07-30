#include <LicenseVerification/LicenseVerifier.h>

#include <LicenseSystem/LicenseClient.h>
#include <LicenseSystem/LicenseTypes.h>

#include <algorithm>
#include <cctype>
#include <set>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#if defined(__has_include)
#if __has_include(<data_path.h>)
#include <data_path.h>
#endif
#endif

namespace
{

std::filesystem::path weakAbsolutePath(const std::filesystem::path& path)
{
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    if (ec) {
        return path;
    }
    return absolute.lexically_normal();
}

std::filesystem::path weakCanonicalPath(const std::filesystem::path& path)
{
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return weakAbsolutePath(path);
    }
    return canonical.lexically_normal();
}

bool directoryExists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

bool fileExists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

void appendUniquePath(
    std::vector<std::filesystem::path>& paths,
    std::set<std::filesystem::path>& seen,
    const std::filesystem::path& path)
{
    if (path.empty()) {
        return;
    }

    const auto normalized = weakCanonicalPath(path);
    if (seen.insert(normalized).second) {
        paths.push_back(normalized);
    }
}

std::filesystem::path executableDirectory()
{
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD copied = 0;
    for (;;) {
        copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) {
            break;
        }
        if (copied < buffer.size() - 1) {
            buffer.resize(copied);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#endif

    std::error_code ec;
    return std::filesystem::current_path(ec);
}

std::filesystem::path sourceDirectory()
{
#ifdef PROJECT_SOURCE_PATH
    return std::filesystem::path(PROJECT_SOURCE_PATH);
#else
    return {};
#endif
}

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isLicenseDirectoryName(const std::filesystem::path& path)
{
    const auto name = lowercase(path.filename().string());
    return name == "license" || name == "licenses";
}

void appendRecursiveLicenseDirectories(
    std::vector<std::filesystem::path>& directories,
    std::set<std::filesystem::path>& seen,
    const std::filesystem::path& root)
{
    if (!directoryExists(root) || !isLicenseDirectoryName(root)) {
        return;
    }

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end;
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it->is_directory(ec)) {
            appendUniquePath(directories, seen, it->path());
        }
    }
}

std::vector<std::filesystem::path> buildSearchDirectories(const common::license::LicenseSearchOptions& options)
{
    std::vector<std::filesystem::path> directories;
    std::set<std::filesystem::path> seen;

    auto appendDirectory = [&](const std::filesystem::path& path) {
        if (directoryExists(path)) {
            appendUniquePath(directories, seen, path);
        }
    };

    if (options.searchExecutableDirectory) {
        const auto exeDir = executableDirectory();
        appendDirectory(exeDir);
        appendDirectory(exeDir / "license");
        appendDirectory(exeDir / "licenses");
    }

    if (options.searchSourceLicenseDirectory) {
        const auto sourceDir = sourceDirectory();
        if (!sourceDir.empty()) {
            appendDirectory(sourceDir / "license");
            appendDirectory(sourceDir / "licenses");
        }
    }

    if (options.searchWorkingDirectory) {
        std::error_code ec;
        const auto workingDir = std::filesystem::current_path(ec);
        if (!ec) {
            appendDirectory(workingDir);
            appendDirectory(workingDir / "license");
            appendDirectory(workingDir / "licenses");
        }
    }

    for (const auto& directory : options.extraSearchDirectories) {
        appendDirectory(directory);
    }

    if (options.recursiveLicenseDirectories) {
        const auto snapshot = directories;
        for (const auto& directory : snapshot) {
            appendRecursiveLicenseDirectories(directories, seen, directory);
        }
    }

    return directories;
}

std::vector<std::string> licenseFileNames(const common::license::LicenseSearchOptions& options)
{
    if (!options.fileNames.empty()) {
        return options.fileNames;
    }

    return {"license.LIC", "license.lic", "RS2026.LIC", "rs2026.LIC"};
}

bool hasLicenseExtension(const std::filesystem::path& path)
{
    const auto ext = lowercase(path.extension().string());
    return ext == ".lic";
}

std::vector<std::filesystem::path> buildLicenseCandidates(const common::license::LicenseSearchOptions& options)
{
    const auto directories = buildSearchDirectories(options);
    const auto preferredNames = licenseFileNames(options);

    std::vector<std::filesystem::path> candidates;
    std::set<std::filesystem::path> seen;

    for (const auto& directory : directories) {
        for (const auto& fileName : preferredNames) {
            const auto candidate = directory / fileName;
            if (fileExists(candidate)) {
                appendUniquePath(candidates, seen, candidate);
            }
        }
    }

    for (const auto& directory : directories) {
        std::error_code ec;
        for (std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end;
             it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (it->is_regular_file(ec) && hasLicenseExtension(it->path())) {
                appendUniquePath(candidates, seen, it->path());
            }
        }
    }

    return candidates;
}

common::license::AuthorizationStatus convertStatus(LicenseSystem::LicenseValidationStatus status)
{
    using LicenseSystem::LicenseValidationStatus;
    using common::license::AuthorizationStatus;

    switch (status) {
    case LicenseValidationStatus::Valid:
        return AuthorizationStatus::Authorized;
    case LicenseValidationStatus::FileNotFound:
        return AuthorizationStatus::LicenseFileNotFound;
    case LicenseValidationStatus::ProductMismatch:
        return AuthorizationStatus::ProductMismatch;
    case LicenseValidationStatus::FeatureNotAllowed:
        return AuthorizationStatus::FeatureNotAllowed;
    case LicenseValidationStatus::HardwareMismatch:
    case LicenseValidationStatus::HardwareBindingInvalid:
    case LicenseValidationStatus::HardwareBindingUnsupported:
        return AuthorizationStatus::HardwareMismatch;
    case LicenseValidationStatus::Expired:
        return AuthorizationStatus::Expired;
    case LicenseValidationStatus::InvalidFormat:
    case LicenseValidationStatus::InvalidSignature:
    case LicenseValidationStatus::UnsupportedLicenseFormat:
    case LicenseValidationStatus::DecryptionFailed:
    case LicenseValidationStatus::MissingDecryptionKey:
    case LicenseValidationStatus::InvalidEnvelopeSignature:
        return AuthorizationStatus::InvalidLicense;
    case LicenseValidationStatus::InternalError:
        return AuthorizationStatus::InternalError;
    }

    return AuthorizationStatus::InternalError;
}

int statusPriority(common::license::AuthorizationStatus status)
{
    using common::license::AuthorizationStatus;
    switch (status) {
    case AuthorizationStatus::Expired:
        return 80;
    case AuthorizationStatus::HardwareMismatch:
        return 70;
    case AuthorizationStatus::FeatureNotAllowed:
        return 60;
    case AuthorizationStatus::ProductMismatch:
        return 50;
    case AuthorizationStatus::InvalidLicense:
        return 40;
    case AuthorizationStatus::InternalError:
        return 30;
    case AuthorizationStatus::LicenseFileNotFound:
        return 10;
    case AuthorizationStatus::Authorized:
        return 100;
    }

    return 0;
}

bool featureEnabled(
    const LicenseSystem::LicenseClient& client,
    const common::license::LicenseRequirement& requirement)
{
    if (!requirement.canonicalFeatureName.empty()
        && client.isFeatureEnabled(requirement.canonicalFeatureName)) {
        return true;
    }

    for (const auto& alias : requirement.acceptedFeatureAliases) {
        if (!alias.empty() && client.isFeatureEnabled(alias)) {
            return true;
        }
    }

    return false;
}

std::vector<std::filesystem::path> mergeSearchedPaths(
    const std::vector<std::filesystem::path>& candidates,
    const LicenseSystem::LicenseResult& sdkResult)
{
    std::vector<std::filesystem::path> searched;
    std::set<std::filesystem::path> seen;

    for (const auto& candidate : candidates) {
        appendUniquePath(searched, seen, candidate);
    }
    for (const auto& path : sdkResult.searchedPaths) {
        appendUniquePath(searched, seen, path);
    }

    return searched;
}

} // namespace

namespace common::license
{

AuthorizationResult LicenseVerifier::verify(
    const LicenseRequirement& requirement,
    const LicenseSearchOptions& options) const
{
    const auto candidates = buildLicenseCandidates(options);
    if (candidates.empty()) {
        AuthorizationResult result;
        result.status = AuthorizationStatus::LicenseFileNotFound;
        result.message = "No license file was found in configured search paths.";
        result.searchedPaths = buildSearchDirectories(options);
        return result;
    }

    AuthorizationResult bestFailure;
    bestFailure.status = AuthorizationStatus::LicenseFileNotFound;
    bestFailure.message = "No valid license file was found.";
    bestFailure.searchedPaths = candidates;

    for (const auto& candidate : candidates) {
        LicenseSystem::LicenseClient client(requirement.productId);
        const auto sdkResult = client.authenticateLicenseFile(candidate);

        AuthorizationResult current;
        current.status = convertStatus(sdkResult.status);
        current.message = sdkResult.message;
        current.licenseFile = sdkResult.licenseFile.empty() ? candidate : sdkResult.licenseFile;
        current.enabledFeatures = sdkResult.enabledFeatures;
        current.searchedPaths = mergeSearchedPaths(candidates, sdkResult);
        current.diagnosticsJson = sdkResult.diagnosticsJson;

        if (sdkResult.isValid()) {
            if (featureEnabled(client, requirement)) {
                current.status = AuthorizationStatus::Authorized;
                if (current.message.empty()) {
                    current.message = "License authorization passed.";
                }
                return current;
            }

            current.status = AuthorizationStatus::FeatureNotAllowed;
            current.message = "License is valid but required feature is not enabled: "
                + requirement.canonicalFeatureName;
        }

        if (statusPriority(current.status) > statusPriority(bestFailure.status)) {
            bestFailure = current;
        }
    }

    return bestFailure;
}

std::string toString(AuthorizationStatus status)
{
    switch (status) {
    case AuthorizationStatus::Authorized:
        return "Authorized";
    case AuthorizationStatus::LicenseFileNotFound:
        return "LicenseFileNotFound";
    case AuthorizationStatus::InvalidLicense:
        return "InvalidLicense";
    case AuthorizationStatus::ProductMismatch:
        return "ProductMismatch";
    case AuthorizationStatus::FeatureNotAllowed:
        return "FeatureNotAllowed";
    case AuthorizationStatus::HardwareMismatch:
        return "HardwareMismatch";
    case AuthorizationStatus::Expired:
        return "Expired";
    case AuthorizationStatus::InternalError:
        return "InternalError";
    }

    return "InternalError";
}

} // namespace common::license
