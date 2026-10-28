#include <SprayTrajectoryCore/SprayTrajectoryIo.h>

#include <SprayTrajectoryCore/LegacySprayTrajectoryIo.h>

#include <Eigen/Geometry>

#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

namespace spraytrajectory
{
    namespace
    {
        constexpr std::size_t kPointColumnCount = 12;

        std::vector<std::string> splitTokens(const std::string& line)
        {
            std::vector<std::string> tokens;
            std::istringstream stream(line);
            std::string token;
            while(stream >> token) {
                tokens.push_back(token);
            }
            return tokens;
        }

        bool parseFiniteDouble(const std::string& token, double& value)
        {
            try {
                std::size_t parsedLength = 0;
                value = std::stod(token, &parsedLength);
                return parsedLength == token.size() && std::isfinite(value);
            } catch(const std::exception&) {
                return false;
            }
        }

        SprayTrajectoryLoadResult failure(std::size_t lineNumber, const std::string& message)
        {
            SprayTrajectoryLoadResult result;
            result.warnings.push_back(
                "Line " + std::to_string(lineNumber) + ": " + message);
            return result;
        }
    }

    SprayTrajectoryLoadResult SprayTrajectoryIo::loadText(
        const std::filesystem::path& path,
        const SprayTrajectoryLoadOptions& options)
    {
        std::ifstream input(path);
        if(!input) {
            SprayTrajectoryLoadResult result;
            result.warnings.push_back("Failed to open the spray trajectory file.");
            return result;
        }

        std::string line;
        while(std::getline(input, line)) {
            const std::vector<std::string> tokens = splitTokens(line);
            if(tokens.empty()) {
                continue;
            }
            if(tokens.size() == kPointColumnCount) {
                return loadPointText(path, options);
            }
            break;
        }

        LegacySprayTrajectoryLoadOptions legacyOptions;
        legacyOptions.lengthScaleToMeters = options.lengthScaleToMeters;
        legacyOptions.processId = options.processId;
        const LegacySprayTrajectoryLoadResult legacy =
            LegacySprayTrajectoryIo::loadMatrixText(path, legacyOptions);

        SprayTrajectoryLoadResult result;
        result.trajectory = legacy.trajectory;
        result.warnings = legacy.warnings;
        result.success = legacy.success;
        return result;
    }

    SprayTrajectoryLoadResult SprayTrajectoryIo::loadPointText(
        const std::filesystem::path& path,
        const SprayTrajectoryLoadOptions& options)
    {
        std::ifstream input(path);
        if(!input) {
            SprayTrajectoryLoadResult result;
            result.warnings.push_back("Failed to open the spray trajectory file.");
            return result;
        }

        SpraySegment segment;
        segment.processId = options.processId;
        segment.sprayEnabled = true;

        bool hasPreviousTime = false;
        double previousTime = 0.0;
        std::size_t lineNumber = 0;
        std::string line;
        while(std::getline(input, line)) {
            ++lineNumber;
            const std::vector<std::string> tokens = splitTokens(line);
            if(tokens.empty()) {
                continue;
            }
            if(tokens.size() != kPointColumnCount) {
                return failure(lineNumber,
                    "expected 12 columns: X Y Z qw qx qy qz velocity acceleration "
                    "timestamp pointType sprayDistance.");
            }

            std::array<double, kPointColumnCount> values{};
            for(std::size_t column = 0; column < values.size(); ++column) {
                if(!parseFiniteDouble(tokens[column], values[column])) {
                    return failure(lineNumber,
                        "column " + std::to_string(column + 1)
                        + " is not a finite number.");
                }
            }

            const double timestamp = values[9];
            if(hasPreviousTime && timestamp < previousTime) {
                return failure(lineNumber, "timestamp must not decrease.");
            }

            Eigen::Quaterniond orientation(values[3], values[4], values[5], values[6]);
            if(!std::isfinite(orientation.norm()) || orientation.norm() <= 1.0e-12) {
                return failure(lineNumber, "quaternion must have a non-zero finite norm.");
            }
            orientation.normalize();

            SprayPathPoint point;
            point.time = timestamp;
            point.tcpPose = Eigen::Isometry3d::Identity();
            point.tcpPose.translation() = Eigen::Vector3d(values[0], values[1], values[2])
                * options.lengthScaleToMeters;
            point.tcpPose.linear() = orientation.toRotationMatrix();
            point.sprayEnabled = true;
            point.processId = options.processId;
            point.targetDistance = values[11] * options.lengthScaleToMeters;
            point.velocity = values[7] * options.velocityScaleToMetersPerSecond;
            point.acceleration = values[8]
                * options.accelerationScaleToMetersPerSecondSquared;
            point.pointType = SprayPathPointType::Regular;
            segment.points.push_back(std::move(point));

            previousTime = timestamp;
            hasPreviousTime = true;
        }

        if(segment.points.empty()) {
            SprayTrajectoryLoadResult result;
            result.warnings.push_back("The spray trajectory file contained no points.");
            return result;
        }

        SprayTrajectoryLoadResult result;
        result.trajectory.name = path.stem().string();
        result.trajectory.segments.push_back(std::move(segment));
        result.success = true;
        return result;
    }
}
