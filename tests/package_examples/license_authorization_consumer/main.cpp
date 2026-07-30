#include <RobotCoreAuthorization/RobotCoreAuthorization.h>
#include <RobotPlatformAuthorization/RobotPlatformAuthorization.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace
{

int reportResult(
    const std::string& name,
    const common::license::AuthorizationResult& result)
{
    std::cout << name << ": " << common::license::toString(result.status) << '\n';
    std::cout << "  message: " << result.message << '\n';
    if (!result.licenseFile.empty()) {
        std::cout << "  license: " << result.licenseFile.string() << '\n';
    }
    if (!result.enabledFeatures.empty()) {
        std::cout << "  features:";
        for (const auto& feature : result.enabledFeatures) {
            std::cout << ' ' << feature;
        }
        std::cout << '\n';
    }
    std::cout << "  searched paths: " << result.searchedPaths.size() << '\n';

    return result.isAuthorized() ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[])
{
    common::license::LicenseSearchOptions options;
    if (argc > 1) {
        options.extraSearchDirectories.push_back(std::filesystem::path(argv[1]));
    }

    const auto coreResult = smrobotcore::authorization::verifyRobotCoreAuthorization(options);
    const auto platformResult = smrobotplatform::authorization::verifyRobotPlatformAuthorization(options);

    int failed = 0;
    failed += reportResult("SMRobot.RobotCore", coreResult);
    failed += reportResult("SMRobot.RobotPlatform", platformResult);

    if (failed != 0) {
        return 1;
    }

    std::cout << "License authorization consumer passed.\n";
    return 0;
}
