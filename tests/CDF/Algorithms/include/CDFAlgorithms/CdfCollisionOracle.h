#pragma once

#include "CDFAlgorithms/CdfTypes.h"

#include <cstddef>
#include <vector>

namespace cdf
{
    class ICdfDistanceOracle
    {
    public:
        virtual ~ICdfDistanceOracle() = default;

        virtual std::size_t dof() const = 0;
        virtual const std::vector<JointBounds>& bounds() const = 0;
        virtual double signedDistance(const JointVector& q) const = 0;
        virtual std::size_t nearestObstacle(const JointVector& q) const;
    };

    class CircularObstacleCdfOracle final : public ICdfDistanceOracle
    {
    public:
        CircularObstacleCdfOracle(
            std::vector<JointBounds> bounds,
            std::vector<CircularObstacle> obstacles,
            double safetyMargin);

        std::size_t dof() const override;
        const std::vector<JointBounds>& bounds() const override;
        double signedDistance(const JointVector& q) const override;
        std::size_t nearestObstacle(const JointVector& q) const override;

        const std::vector<CircularObstacle>& obstacles() const;
        double safetyMargin() const;

    private:
        std::vector<JointBounds> m_bounds;
        std::vector<CircularObstacle> m_obstacles;
        double m_safetyMargin = 0.0;
    };

    class FiniteDifferenceCdfOracle final
    {
    public:
        FiniteDifferenceCdfOracle(const ICdfDistanceOracle& oracle, double step);

        ClearanceSample evaluate(const JointVector& q) const;
        JointVector gradientAt(const JointVector& q) const;

    private:
        const ICdfDistanceOracle& m_oracle;
        double m_step = 1.0e-3;
    };

    struct Point2
    {
        double x = 0.0;
        double y = 0.0;
    };

    struct ThreeDofRobotPose
    {
        std::vector<Point2> joints;
        Point2 tool;
    };

    class ThreeDofRobot3RModel final
    {
    public:
        static constexpr double link1 = 0.22;
        static constexpr double link2 = 0.18;
        static constexpr double link3 = 0.13;

        ThreeDofRobotPose forwardKinematics(const JointVector& q) const;
    };

    class ThreeDofRobotCdfOracle final : public ICdfDistanceOracle
    {
    public:
        ThreeDofRobotCdfOracle(
            std::vector<JointBounds> bounds,
            std::vector<CircularObstacle> obstacles,
            double safetyMargin,
            double linkRadius = 0.018);

        std::size_t dof() const override;
        const std::vector<JointBounds>& bounds() const override;
        double signedDistance(const JointVector& q) const override;
        std::size_t nearestObstacle(const JointVector& q) const override;

        ThreeDofRobotPose forwardKinematics(const JointVector& q) const;
        const std::vector<CircularObstacle>& obstacles() const;
        double linkRadius() const;
        double safetyMargin() const;

    private:
        std::vector<JointBounds> m_bounds;
        std::vector<CircularObstacle> m_obstacles;
        double m_safetyMargin = 0.0;
        double m_linkRadius = 0.018;
        ThreeDofRobot3RModel m_model;
    };
}
