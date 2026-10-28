#pragma once

#include <SprayTrajectoryCore/SprayTrajectory.h>

#include <Eigen/Core>

#include <string>
#include <vector>

namespace calibrationtranslation
{
    struct RapidExportSettings
    {
        std::string moduleName{ "FreeformSpray" };
        std::string toolDataName{ "sprayTool" };
        Eigen::Vector3d safetyPositionBaseMeters = Eigen::Vector3d::Zero();
        double safetySpeedMetersPerSecond{ 0.2 };
    };

    struct RapidProgram
    {
        std::string text;
        std::vector<std::string> warnings;
    };

    class TranslationProgramGenerator
    {
    public:
        static RapidProgram generate(
            const spraytrajectory::SprayTrajectory& trajectory,
            const RapidExportSettings& settings = {});
    };
}
