#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace cdf
{
    using JointVector = std::vector<double>;
    using Path = std::vector<JointVector>;

    struct JointBounds
    {
        double lower = -3.14159265358979323846;
        double upper = 3.14159265358979323846;
    };

    struct CircularObstacle
    {
        double x = 0.0;
        double y = 0.0;
        double radius = 0.25;
        std::string name;
    };

    enum class CdfSceneKind
    {
        JointSpace2D,
        ThreeDofRobot3R
    };

    enum class InitialPathStrategy
    {
        Linear,
        OmplSeedPath,
        OmplStyleSampledTree
    };

    struct PlanningRequest
    {
        JointVector start;
        JointVector goal;
        std::vector<JointBounds> bounds;
        std::vector<CircularObstacle> obstacles;
        Path omplSeedPath;
        CdfSceneKind sceneKind = CdfSceneKind::ThreeDofRobot3R;
        InitialPathStrategy initialPathStrategy = InitialPathStrategy::OmplStyleSampledTree;
        double safetyMargin = 0.04;
        double targetClearance = 0.035;
        double finiteDifferenceStep = 1.0e-3;
        double interpolationStep = 0.045;
        double sampledTreeStep = 0.18;
        std::size_t sampledTreeMaxNodes = 900;
        std::size_t maxRepairIterations = 90;
        double repairGain = 0.75;
        double smoothGain = 0.12;
        std::uint32_t randomSeed = 4204600u;
    };

    struct ClearanceSample
    {
        double signedDistance = 0.0;
        JointVector gradient;
        bool inCollision = false;
        std::size_t nearestObstacle = std::numeric_limits<std::size_t>::max();
    };

    struct CdfRepairIteration
    {
        std::size_t iteration = 0;
        double minClearance = 0.0;
        double maxCorrection = 0.0;
    };

    struct PlanStatistics
    {
        double minClearanceBeforeRepair = 0.0;
        double minClearanceAfterRepair = 0.0;
        std::size_t waypointCountBeforeRepair = 0;
        std::size_t waypointCountAfterRepair = 0;
        std::size_t repairIterations = 0;
        bool collisionFree = false;
        bool usedProvidedOmplSeed = false;
        bool usedSampledTreeSeed = false;
    };

    struct PlanningResult
    {
        Path initialPath;
        Path repairedPath;
        std::vector<double> initialClearances;
        std::vector<double> repairedClearances;
        std::vector<CdfRepairIteration> repairTrace;
        PlanStatistics statistics;
        std::string message;
    };
}
