#pragma once
#include <CalibrationInstructionTranslation/TranslationTypes.h>
namespace smrobot::spray::translation {
constexpr double pi = 3.14159265358979323846;
constexpr double metersPerMillimeter = 0.001;
constexpr double millimetersPerMeter = 1000.0;
double millimetersToMeters(double millimeters) noexcept;
double metersToMillimeters(double meters) noexcept;
double degreesToRadians(double degrees) noexcept;
double radiansToDegrees(double radians) noexcept;
bool isFiniteTransform(const Eigen::Isometry3d& transform) noexcept;
Eigen::Matrix3d rotationFromRollPitchYaw(const Eigen::Vector3d& rollPitchYawRadians) noexcept;
Eigen::Isometry3d makeTransform(const TransformComponents& components) noexcept;
PlanningResult<TransformComponents> transformComponents(const Eigen::Isometry3d& transform);
}
