#include "CDFAlgorithms/CdfCollisionOracle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cdf
{
    namespace
    {
        constexpr std::size_t invalidIndex()
        {
            return std::numeric_limits<std::size_t>::max();
        }

        void validateStateSize(const ICdfDistanceOracle& oracle, const JointVector& q)
        {
            if (q.size() != oracle.dof())
                throw std::invalid_argument("CDF query dimension does not match the oracle.");
        }

        double distanceToObstacle(const JointVector& q, const CircularObstacle& obstacle)
        {
            const double dx = q[0] - obstacle.x;
            const double dy = q[1] - obstacle.y;
            return std::sqrt(dx * dx + dy * dy) - obstacle.radius;
        }

        double pointSegmentDistance(Point2 point, Point2 a, Point2 b)
        {
            const double vx = b.x - a.x;
            const double vy = b.y - a.y;
            const double wx = point.x - a.x;
            const double wy = point.y - a.y;
            const double lengthSquared = vx * vx + vy * vy;
            double t = 0.0;
            if (lengthSquared > 1.0e-12)
                t = std::max(0.0, std::min(1.0, (wx * vx + wy * vy) / lengthSquared));
            const double px = a.x + t * vx;
            const double py = a.y + t * vy;
            const double dx = point.x - px;
            const double dy = point.y - py;
            return std::sqrt(dx * dx + dy * dy);
        }

        double distanceLinkToObstacle(
            Point2 a,
            Point2 b,
            const CircularObstacle& obstacle,
            double linkRadius,
            double safetyMargin)
        {
            const Point2 center{ obstacle.x, obstacle.y };
            return pointSegmentDistance(center, a, b) - obstacle.radius - linkRadius - safetyMargin;
        }
    }

    std::size_t ICdfDistanceOracle::nearestObstacle(const JointVector&) const
    {
        return invalidIndex();
    }

    CircularObstacleCdfOracle::CircularObstacleCdfOracle(
        std::vector<JointBounds> bounds,
        std::vector<CircularObstacle> obstacles,
        double safetyMargin)
        : m_bounds(std::move(bounds))
        , m_obstacles(std::move(obstacles))
        , m_safetyMargin(safetyMargin)
    {
        if (m_bounds.size() < 2)
            throw std::invalid_argument("The demo CDF oracle needs at least two joint bounds.");
        for (const JointBounds& bound : m_bounds)
        {
            if (!(bound.lower < bound.upper))
                throw std::invalid_argument("Invalid CDF joint bounds.");
        }
    }

    std::size_t CircularObstacleCdfOracle::dof() const
    {
        return m_bounds.size();
    }

    const std::vector<JointBounds>& CircularObstacleCdfOracle::bounds() const
    {
        return m_bounds;
    }

    double CircularObstacleCdfOracle::signedDistance(const JointVector& q) const
    {
        if (q.size() != m_bounds.size())
            throw std::invalid_argument("CDF query dimension does not match the obstacle oracle.");

        double best = std::numeric_limits<double>::max();
        for (const CircularObstacle& obstacle : m_obstacles)
            best = std::min(best, distanceToObstacle(q, obstacle) - m_safetyMargin);

        for (std::size_t i = 0; i < q.size(); ++i)
        {
            best = std::min(best, q[i] - m_bounds[i].lower);
            best = std::min(best, m_bounds[i].upper - q[i]);
        }

        return best == std::numeric_limits<double>::max() ? 1.0e6 : best;
    }

    std::size_t CircularObstacleCdfOracle::nearestObstacle(const JointVector& q) const
    {
        if (q.size() != m_bounds.size())
            throw std::invalid_argument("CDF query dimension does not match the obstacle oracle.");
        if (m_obstacles.empty())
            return invalidIndex();

        double best = std::numeric_limits<double>::max();
        std::size_t bestIndex = invalidIndex();
        for (std::size_t i = 0; i < m_obstacles.size(); ++i)
        {
            const double distance = distanceToObstacle(q, m_obstacles[i]);
            if (distance < best)
            {
                best = distance;
                bestIndex = i;
            }
        }
        return bestIndex;
    }

    const std::vector<CircularObstacle>& CircularObstacleCdfOracle::obstacles() const
    {
        return m_obstacles;
    }

    double CircularObstacleCdfOracle::safetyMargin() const
    {
        return m_safetyMargin;
    }

    ThreeDofRobotPose ThreeDofRobot3RModel::forwardKinematics(const JointVector& q) const
    {
        if (q.size() != 3)
            throw std::invalid_argument("ThreeDofRobot3RModel needs exactly three joint values.");

        ThreeDofRobotPose pose;
        pose.joints.resize(4);
        pose.joints[0] = { 0.0, 0.0 };

        double angle = q[0];
        pose.joints[1] = {
            pose.joints[0].x + link1 * std::cos(angle),
            pose.joints[0].y + link1 * std::sin(angle)
        };

        angle += q[1];
        pose.joints[2] = {
            pose.joints[1].x + link2 * std::cos(angle),
            pose.joints[1].y + link2 * std::sin(angle)
        };

        angle += q[2];
        pose.joints[3] = {
            pose.joints[2].x + link3 * std::cos(angle),
            pose.joints[2].y + link3 * std::sin(angle)
        };
        pose.tool = pose.joints[3];
        return pose;
    }

    ThreeDofRobotCdfOracle::ThreeDofRobotCdfOracle(
        std::vector<JointBounds> bounds,
        std::vector<CircularObstacle> obstacles,
        double safetyMargin,
        double linkRadius)
        : m_bounds(std::move(bounds))
        , m_obstacles(std::move(obstacles))
        , m_safetyMargin(safetyMargin)
        , m_linkRadius(std::max(0.0, linkRadius))
    {
        if (m_bounds.size() != 3)
            throw std::invalid_argument("The 3R robot CDF oracle needs exactly three joint bounds.");
        for (const JointBounds& bound : m_bounds)
        {
            if (!(bound.lower < bound.upper))
                throw std::invalid_argument("Invalid 3R robot joint bounds.");
        }
    }

    std::size_t ThreeDofRobotCdfOracle::dof() const
    {
        return m_bounds.size();
    }

    const std::vector<JointBounds>& ThreeDofRobotCdfOracle::bounds() const
    {
        return m_bounds;
    }

    double ThreeDofRobotCdfOracle::signedDistance(const JointVector& q) const
    {
        if (q.size() != m_bounds.size())
            throw std::invalid_argument("CDF query dimension does not match the 3R robot oracle.");

        double best = std::numeric_limits<double>::max();
        const ThreeDofRobotPose pose = forwardKinematics(q);

        for (const CircularObstacle& obstacle : m_obstacles)
        {
            for (std::size_t link = 1; link < pose.joints.size(); ++link)
            {
                best = std::min(best, distanceLinkToObstacle(
                    pose.joints[link - 1],
                    pose.joints[link],
                    obstacle,
                    m_linkRadius,
                    m_safetyMargin));
            }
        }

        for (std::size_t i = 0; i < q.size(); ++i)
        {
            best = std::min(best, q[i] - m_bounds[i].lower);
            best = std::min(best, m_bounds[i].upper - q[i]);
        }

        return best == std::numeric_limits<double>::max() ? 1.0e6 : best;
    }

    std::size_t ThreeDofRobotCdfOracle::nearestObstacle(const JointVector& q) const
    {
        if (q.size() != m_bounds.size())
            throw std::invalid_argument("CDF query dimension does not match the 3R robot oracle.");
        if (m_obstacles.empty())
            return invalidIndex();

        double best = std::numeric_limits<double>::max();
        std::size_t bestIndex = invalidIndex();
        const ThreeDofRobotPose pose = forwardKinematics(q);
        for (std::size_t obstacleIndex = 0; obstacleIndex < m_obstacles.size(); ++obstacleIndex)
        {
            for (std::size_t link = 1; link < pose.joints.size(); ++link)
            {
                const double distance = distanceLinkToObstacle(
                    pose.joints[link - 1],
                    pose.joints[link],
                    m_obstacles[obstacleIndex],
                    m_linkRadius,
                    m_safetyMargin);
                if (distance < best)
                {
                    best = distance;
                    bestIndex = obstacleIndex;
                }
            }
        }
        return bestIndex;
    }

    ThreeDofRobotPose ThreeDofRobotCdfOracle::forwardKinematics(const JointVector& q) const
    {
        return m_model.forwardKinematics(q);
    }

    const std::vector<CircularObstacle>& ThreeDofRobotCdfOracle::obstacles() const
    {
        return m_obstacles;
    }

    double ThreeDofRobotCdfOracle::linkRadius() const
    {
        return m_linkRadius;
    }

    double ThreeDofRobotCdfOracle::safetyMargin() const
    {
        return m_safetyMargin;
    }

    FiniteDifferenceCdfOracle::FiniteDifferenceCdfOracle(
        const ICdfDistanceOracle& oracle,
        double step)
        : m_oracle(oracle)
        , m_step(std::max(step, 1.0e-6))
    {
    }

    ClearanceSample FiniteDifferenceCdfOracle::evaluate(const JointVector& q) const
    {
        validateStateSize(m_oracle, q);

        ClearanceSample sample;
        sample.signedDistance = m_oracle.signedDistance(q);
        sample.gradient = gradientAt(q);
        sample.inCollision = sample.signedDistance < 0.0;
        sample.nearestObstacle = m_oracle.nearestObstacle(q);
        return sample;
    }

    JointVector FiniteDifferenceCdfOracle::gradientAt(const JointVector& q) const
    {
        validateStateSize(m_oracle, q);

        JointVector gradient(q.size(), 0.0);
        const std::vector<JointBounds>& bounds = m_oracle.bounds();
        for (std::size_t i = 0; i < q.size(); ++i)
        {
            JointVector forward = q;
            JointVector backward = q;
            forward[i] = std::min(bounds[i].upper, q[i] + m_step);
            backward[i] = std::max(bounds[i].lower, q[i] - m_step);
            const double denominator = forward[i] - backward[i];
            if (std::abs(denominator) <= 1.0e-12)
                continue;
            const double distanceForward = m_oracle.signedDistance(forward);
            const double distanceBackward = m_oracle.signedDistance(backward);
            gradient[i] = (distanceForward - distanceBackward) / denominator;
        }
        return gradient;
    }
}
