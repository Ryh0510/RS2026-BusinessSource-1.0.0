#include <CalibrationInstructionTranslation/TransformUtils.h>
#include <algorithm>
#include <cmath>
namespace smrobot::spray::translation {
    double millimetersToMeters(double millimeters) noexcept
    {
        return millimeters * metersPerMillimeter;
    }
    double metersToMillimeters(double meters) noexcept
    {
        return meters * millimetersPerMeter;
    }
    double degreesToRadians(double degrees) noexcept
    {
        return degrees * pi / 180.0;
    }
    double radiansToDegrees(double radians) noexcept
    {
        return radians * 180.0 / pi;
    }
    bool isFiniteTransform(const Eigen::Isometry3d& transform) noexcept
    {
        if (!transform.matrix().allFinite())
        {
            return false;
        }
        const Eigen::Matrix3d rotation = transform.linear();
        return std::abs(rotation.determinant() - 1.0) <= 1.0e-8 &&
            (rotation.transpose() * rotation - Eigen::Matrix3d::Identity()).norm() <= 1.0e-8;
    }
    Eigen::Matrix3d rotationFromRollPitchYaw(const Eigen::Vector3d& rollPitchYawRadians) noexcept
    {
        const Eigen::AngleAxisd roll(rollPitchYawRadians.x(), Eigen::Vector3d::UnitX());
        const Eigen::AngleAxisd pitch(rollPitchYawRadians.y(), Eigen::Vector3d::UnitY());
        const Eigen::AngleAxisd yaw(rollPitchYawRadians.z(), Eigen::Vector3d::UnitZ());
        return (yaw * pitch * roll).toRotationMatrix();
    }
    Eigen::Isometry3d makeTransform(const TransformComponents& components) noexcept
    {
        Eigen::Isometry3d result = Eigen::Isometry3d::Identity();
        result.linear() = rotationFromRollPitchYaw(components.rollPitchYawRadians);
        result.translation() = components.translationMeters;
        return result;
    }
    PlanningResult<TransformComponents> transformComponents(const Eigen::Isometry3d& transform)
    {
        if (!isFiniteTransform(transform))
        {
            return PlanningResult<TransformComponents>::failure(
                PlanningErrorCode::NonFiniteGeometry,
                "Transform must contain a finite rigid rotation and translation.");
        }

        TransformComponents result;
        result.translationMeters = transform.translation();
        const Eigen::Matrix3d& rotation = transform.linear();
        const double sinPitch = std::clamp(-rotation(2, 0), -1.0, 1.0);
        const double pitch = std::asin(sinPitch);
        const double cosPitch = std::cos(pitch);
        double roll = 0.0;
        double yaw = 0.0;
        if (std::abs(cosPitch) > 1.0e-10)
        {
            roll = std::atan2(rotation(2, 1), rotation(2, 2));
            yaw = std::atan2(rotation(1, 0), rotation(0, 0));
        }
        else
        {
            roll = 0.0;
            yaw = std::atan2(-rotation(0, 1), rotation(1, 1));
        }
        result.rollPitchYawRadians = { roll, pitch, yaw };
        return PlanningResult<TransformComponents>::success(result);
    }
}
