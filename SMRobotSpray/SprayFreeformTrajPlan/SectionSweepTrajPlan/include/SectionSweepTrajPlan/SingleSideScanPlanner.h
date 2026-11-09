#pragma once

#include <SlicingTrajPlan/BurTrajPlan_global.h>
#include <Eigen/Geometry>
#include <array>
#include <string>
#include <vector>

namespace section_sweep
{
    using Triangle = std::array<Eigen::Vector3d, 3>;

    // All geometry, distances and speeds at this boundary are SI units.
    struct Parameters
    {
        int scanAxis = 4; // +X,-X,+Y,-Y,+Z,-Z
        int sideAxis = 2;
        double passSpacing = .005;
        double sampleSpacing = .001;
        double sprayDistance = .100;
        double spraySpeed = .020;
        double transferSpeed = .200;
        double endExtension = .010;
        double retractClearance = .002; // Additional clearance beyond footprint/ramp/valve extension.
        double maximumTiltDegrees = 88.;
        bool alternateDirection = true;
    };

    struct SectionSegment
    {
        Eigen::Vector3d a = Eigen::Vector3d::Zero();
        Eigen::Vector3d b = Eigen::Vector3d::Zero();
        Eigen::Vector3d normal = Eigen::Vector3d::UnitY();
    };

    struct Point
    {
        Eigen::Isometry3d pose = Eigen::Isometry3d::Identity();
        double time = 0.;
        double speed = 0.;
        int pass = -1;
        // Describes the move ending at this point; transfers never spray.
        bool spray = false;
    };

    struct Result
    {
        bool success = false;
        std::string message;
        std::vector<SectionSegment> section;
        std::vector<Point> points;
        std::size_t passCount = 0;
        std::size_t hiddenCandidates = 0;
        std::size_t exteriorSamples = 0;
        std::size_t coveredSamples = 0;
        double maximumUsedTiltDegrees = 0.;
        double scanLength = 0.;
        double sprayDistance = 0.;
    };

    class BurTrajPlan_SHAREDLIB SingleSideScanPlanner final
    {
    public:
        static Eigen::Vector3d axis(int signedAxis);
        static Result generate(const std::vector<Triangle>& mesh, const Parameters& parameters);
    };
}
