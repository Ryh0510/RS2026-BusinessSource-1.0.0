#include <SprayTrajectoryCore/SprayTrajectory.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace spraytrajectory
{
    namespace
    {
        double clamp01(double value)
        {
            if (value < 0.0)
                return 0.0;
            if (value > 1.0)
                return 1.0;
            return value;
        }

        Eigen::Isometry3d interpolatePose(
            const Eigen::Isometry3d& a,
            const Eigen::Isometry3d& b,
            double t)
        {
            Eigen::Quaterniond qa(a.rotation());
            Eigen::Quaterniond qb(b.rotation());

            Eigen::Isometry3d result = Eigen::Isometry3d::Identity();
            result.linear() = qa.slerp(t, qb).normalized().toRotationMatrix();
            result.translation() = a.translation() * (1.0 - t) + b.translation() * t;
            return result;
        }

        std::vector<double> interpolateJoints(
            const std::vector<double>& a,
            const std::vector<double>& b,
            double t)
        {
            if (a.size() != b.size())
                return t < 0.5 ? a : b;

            std::vector<double> value(a.size(), 0.0);
            for (size_t i = 0; i < a.size(); ++i)
                value[i] = a[i] * (1.0 - t) + b[i] * t;
            return value;
        }
        SprayTrajectorySample makeSample(const SprayPathPoint& point)
        {
            SprayTrajectorySample sample;
            sample.time = point.time;
            sample.tcpPose = point.tcpPose;
            sample.jointValues = point.jointValues;
            sample.sprayEnabled = point.sprayEnabled;
            sample.processId = point.processId;
            sample.targetDistance = point.targetDistance;
            sample.targetNormal = point.targetNormal;
            sample.workpieceRegionId = point.workpieceRegionId;
            sample.velocity = point.velocity;
            sample.acceleration = point.acceleration;
            sample.pointType = point.pointType;
            return sample;
        }
    }

    bool SprayTrajectory::empty() const
    {
        return flattenedPoints().empty();
    }

    double SprayTrajectory::duration() const
    {
        const auto points = flattenedPoints();
        if (points.empty())
            return 0.0;
        return points.back().time - points.front().time;
    }

    std::vector<SprayPathPoint> SprayTrajectory::flattenedPoints() const
    {
        std::vector<SprayPathPoint> points;
        for (const auto& segment : segments)
        {
            for (auto point : segment.points)
            {
                if (point.processId.empty())
                    point.processId = segment.processId;
                point.sprayEnabled = point.sprayEnabled && segment.sprayEnabled;
                points.push_back(point);
            }
        }

        // Equal-time process transitions must retain their producer order.
        std::stable_sort(points.begin(), points.end(),
            [](const SprayPathPoint& a, const SprayPathPoint& b)
            {
                return a.time < b.time;
            });
        return points;
    }

    SprayTrajectorySample SprayTrajectorySampler::evaluate(
        const SprayTrajectory& trajectory,
        double time)
    {
        const auto points = trajectory.flattenedPoints();
        if (points.empty())
            return {};

        if (time <= points.front().time)
        {
            return makeSample(points.front());
        }

        if (time >= points.back().time)
        {
            return makeSample(points.back());
        }

        auto next = std::lower_bound(
            points.begin(),
            points.end(),
            time,
            [](const SprayPathPoint& point, double value)
            {
                return point.time < value;
            });

        // Preserve source points exactly, in particular at process on/off and
        // process-id boundaries.  Interpolating their discrete state would
        // otherwise change the state at the boundary itself.
        if (next != points.end() && next->time == time)
        {
            return makeSample(*next);
        }

        if (next == points.begin())
        {
            return makeSample(*next);
        }

        const auto prev = next - 1;
        const double span = next->time - prev->time;
        const double alpha = span > 0.0 ? clamp01((time - prev->time) / span) : 0.0;

        SprayTrajectorySample sample;
        sample.time = time;
        sample.tcpPose = interpolatePose(prev->tcpPose, next->tcpPose, alpha);
        sample.jointValues = interpolateJoints(prev->jointValues, next->jointValues, alpha);
        // Process state is piecewise constant over [prev.time, next.time).
        // Position-like values are interpolated, while this state belongs to
        // the interval's left endpoint and must not be blended away.
        sample.sprayEnabled = prev->sprayEnabled;
        sample.processId = prev->processId;
        sample.targetDistance = prev->targetDistance * (1.0 - alpha) + next->targetDistance * alpha;
        Eigen::Vector3d targetNormal = prev->targetNormal * (1.0 - alpha) + next->targetNormal * alpha;
        sample.targetNormal = targetNormal.norm() > 1.0e-12 ? targetNormal.normalized() : Eigen::Vector3d::UnitZ();
        sample.workpieceRegionId = prev->workpieceRegionId;
        sample.velocity = prev->velocity * (1.0 - alpha) + next->velocity * alpha;
        sample.acceleration = prev->acceleration * (1.0 - alpha) + next->acceleration * alpha;
        sample.pointType = alpha < 0.5 ? prev->pointType : next->pointType;
        return sample;
    }

    std::vector<SprayTrajectorySample> SprayTrajectorySampler::sample(
        const SprayTrajectory& trajectory,
        double timeStep)
    {
        std::vector<SprayTrajectorySample> samples;
        const auto points = trajectory.flattenedPoints();
        if (points.empty() || timeStep <= 0.0)
            return samples;

        const double begin = points.front().time;
        const double end = points.back().time;
        if (end <= begin)
        {
            samples.push_back(makeSample(points.front()));
            return samples;
        }

        // Derive every time from the integer sample index.  Repeated floating
        // point addition drifts over long paths and can skip a requested grid
        // boundary or add an almost-duplicate final sample.
        for (std::size_t index = 0;; ++index)
        {
            const double time = begin + static_cast<double>(index) * timeStep;
            if (time >= end)
            {
                break;
            }
            samples.push_back(evaluate(trajectory, time));
        }

        samples.push_back(evaluate(trajectory, end));
        return samples;
    }

    std::vector<SprayTrajectorySample> SprayTrajectorySampler::originalSamples(
        const SprayTrajectory& trajectory)
    {
        const auto points = trajectory.flattenedPoints();
        std::vector<SprayTrajectorySample> samples;
        samples.reserve(points.size());
        for(const auto& point : points)
            samples.push_back(makeSample(point));
        return samples;
    }
}
