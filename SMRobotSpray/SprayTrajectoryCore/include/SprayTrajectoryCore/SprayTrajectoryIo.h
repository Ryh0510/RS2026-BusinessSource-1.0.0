#pragma once

#include <SprayTrajectoryCore/SprayTrajectory.h>

#include <filesystem>
#include <string>
#include <vector>

namespace spraytrajectory
{
    struct SprayTrajectoryLoadOptions
    {
        double lengthScaleToMeters{ 1.0e-3 };
        double velocityScaleToMetersPerSecond{ 1.0e-3 };
        double accelerationScaleToMetersPerSecondSquared{ 1.0e-3 };
        std::string processId{ "paper_gaussian" };
    };

    struct SprayTrajectoryLoadResult
    {
        SprayTrajectory trajectory;
        std::vector<std::string> warnings;
        bool success{ false };
    };

    class SprayTrajectoryIo
    {
    public:
        static SprayTrajectoryLoadResult loadText(
            const std::filesystem::path& path,
            const SprayTrajectoryLoadOptions& options = {});

        static SprayTrajectoryLoadResult loadPointText(
            const std::filesystem::path& path,
            const SprayTrajectoryLoadOptions& options = {});
    };
}
