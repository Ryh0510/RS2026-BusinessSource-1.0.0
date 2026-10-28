#pragma once
#include <Eigen/Geometry>
#include <string>
#include <vector>
#include <utility>
#include <optional>
namespace smrobot::spray::translation {
    enum class PlanningErrorCode
    {
        None,
        InvalidArgument,
        EmptyMesh,
        NonFiniteGeometry,
        InvalidTriangleIndex,
        DegenerateGeometry,
        InvalidAxisSelection,
        AlignmentFailed,
        SectionFailed,
        NoContour,
        InsufficientRegionData,
        InsufficientToothTopData,
        SingularFit
    };

    struct PlanningError
    {
        PlanningErrorCode code{ PlanningErrorCode::None };
        std::string message;

        explicit operator bool() const noexcept
        {
            return code != PlanningErrorCode::None;
        }
    };

    template<typename T>
    struct PlanningResult
    {
        T value{};
        PlanningError error;
        std::vector<std::string> diagnostics;

        bool ok() const noexcept
        {
            return !static_cast<bool>(error);
        }

        explicit operator bool() const noexcept
        {
            return ok();
        }

        static PlanningResult success(T result)
        {
            PlanningResult output;
            output.value = std::move(result);
            return output;
        }

        static PlanningResult failure(PlanningErrorCode code, std::string message)
        {
            PlanningResult output;
            output.error = { code, std::move(message) };
            return output;
        }
    };

    template<>
    struct PlanningResult<void>
    {
        PlanningError error;
        std::vector<std::string> diagnostics;

        bool ok() const noexcept
        {
            return !static_cast<bool>(error);
        }

        explicit operator bool() const noexcept
        {
            return ok();
        }

        static PlanningResult success()
        {
            return {};
        }

        static PlanningResult failure(PlanningErrorCode code, std::string message)
        {
            PlanningResult output;
            output.error = { code, std::move(message) };
            return output;
        }
    };

    struct TransformComponents
    {
        Eigen::Vector3d translationMeters = Eigen::Vector3d::Zero();
        Eigen::Vector3d rollPitchYawRadians = Eigen::Vector3d::Zero();
    };

    enum class RapidSequenceEntryKind { SafetyPoint, Trajectory };
    struct RapidSequenceEntry { RapidSequenceEntryKind kind{RapidSequenceEntryKind::SafetyPoint}; std::string trajectoryPassId; };
    struct TrajectoryPosePoint {
        double timeSeconds{0.0};
        Eigen::Isometry3d planningFromTool = Eigen::Isometry3d::Identity();
        bool sprayEnabled{false};
        double speedMetersPerSecond{0.0};
    };
    struct TrajectoryGenerationParameters { double speedMetersPerSecond{0.006}; };
    struct PlannedTrajectory {
        TrajectoryGenerationParameters parameters;
        std::vector<TrajectoryPosePoint> linearPoints;
        bool hasValidPoints() const noexcept { return !linearPoints.empty(); }
    };
    struct TrajectoryPass { std::string id; int order{0}; PlannedTrajectory trajectory; };
    struct TrajectoryGroup { std::vector<TrajectoryPass> passes; };
    struct TrajectoryWorkspace { TrajectoryGroup group; };
    struct PublishedTrajectoryPlan {
        std::string objectId;
        Eigen::Isometry3d baseFromPlanning = Eigen::Isometry3d::Identity();
        TrajectoryGroup group;
        std::vector<RapidSequenceEntry> executionSequence;
    };
}
