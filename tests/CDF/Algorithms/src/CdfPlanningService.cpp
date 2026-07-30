#include "CDFAlgorithms/CdfPlanningService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

namespace cdf
{
    namespace
    {
        constexpr double epsilon = 1.0e-9;

        std::size_t checkedDof(const PlanningRequest& request)
        {
            const std::size_t dof = request.bounds.size();
            if (dof == 0)
                throw std::invalid_argument("CDF planning request needs joint bounds.");
            if (request.start.size() != dof || request.goal.size() != dof)
                throw std::invalid_argument("CDF planning request start/goal size must match bounds.");
            return dof;
        }

        double squaredDistance(const JointVector& a, const JointVector& b)
        {
            double total = 0.0;
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                const double delta = a[i] - b[i];
                total += delta * delta;
            }
            return total;
        }

        double distance(const JointVector& a, const JointVector& b)
        {
            return std::sqrt(squaredDistance(a, b));
        }

        double norm(const JointVector& value)
        {
            return std::sqrt(std::inner_product(value.begin(), value.end(), value.begin(), 0.0));
        }

        JointVector lerp(const JointVector& a, const JointVector& b, double t)
        {
            JointVector result(a.size(), 0.0);
            for (std::size_t i = 0; i < a.size(); ++i)
                result[i] = a[i] + (b[i] - a[i]) * t;
            return result;
        }

        JointVector clampToBounds(const JointVector& q, const std::vector<JointBounds>& bounds)
        {
            JointVector result = q;
            for (std::size_t i = 0; i < result.size(); ++i)
                result[i] = std::max(bounds[i].lower, std::min(bounds[i].upper, result[i]));
            return result;
        }

        Path resamplePath(const Path& path, double maxStep)
        {
            if (path.size() < 2)
                return path;

            const double step = std::max(maxStep, 1.0e-4);
            Path result;
            result.push_back(path.front());
            for (std::size_t segment = 1; segment < path.size(); ++segment)
            {
                const JointVector& from = path[segment - 1];
                const JointVector& to = path[segment];
                const std::size_t count = std::max<std::size_t>(
                    1,
                    static_cast<std::size_t>(std::ceil(distance(from, to) / step)));
                for (std::size_t i = 1; i <= count; ++i)
                    result.push_back(lerp(from, to, static_cast<double>(i) / static_cast<double>(count)));
            }
            return result;
        }

        double minValue(const std::vector<double>& values)
        {
            if (values.empty())
                return 0.0;
            return *std::min_element(values.begin(), values.end());
        }

        bool sameState(const JointVector& a, const JointVector& b)
        {
            return distance(a, b) <= 1.0e-6;
        }

        bool stateSafe(const ICdfDistanceOracle& oracle, const JointVector& q)
        {
            return oracle.signedDistance(q) >= 0.0;
        }

        bool stateHasClearance(const ICdfDistanceOracle& oracle, const JointVector& q, double minimumClearance)
        {
            return oracle.signedDistance(q) >= minimumClearance;
        }

        bool segmentSafe(
            const ICdfDistanceOracle& oracle,
            const JointVector& a,
            const JointVector& b,
            double interpolationStep)
        {
            const std::size_t count = std::max<std::size_t>(
                1,
                static_cast<std::size_t>(std::ceil(distance(a, b) / std::max(interpolationStep, 1.0e-4))));
            for (std::size_t i = 0; i <= count; ++i)
            {
                const JointVector q = lerp(a, b, static_cast<double>(i) / static_cast<double>(count));
                if (!stateSafe(oracle, q))
                    return false;
            }
            return true;
        }

        void copyEndpoint(const PlanningRequest& request, Path& path);

        JointVector solveCdfQpCorrection(
            const PlanningRequest& request,
            const Path& path,
            std::size_t waypointIndex,
            const ClearanceSample& sample)
        {
            const JointVector& q = path[waypointIndex];
            JointVector delta(q.size(), 0.0);

            for (std::size_t j = 0; j < delta.size(); ++j)
            {
                const double midpoint = 0.5 * (path[waypointIndex - 1][j] + path[waypointIndex + 1][j]);
                delta[j] = request.smoothGain * (midpoint - q[j]);
            }

            const double requiredIncrease = request.repairGain * (request.targetClearance - sample.signedDistance);
            const double gradientNormSquared = std::inner_product(
                sample.gradient.begin(),
                sample.gradient.end(),
                sample.gradient.begin(),
                0.0);

            if (requiredIncrease > 0.0 && gradientNormSquared > 1.0e-12)
            {
                const double predictedIncrease = std::inner_product(
                    sample.gradient.begin(),
                    sample.gradient.end(),
                    delta.begin(),
                    0.0);

                if (predictedIncrease < requiredIncrease)
                {
                    const double lambda = (requiredIncrease - predictedIncrease) / gradientNormSquared;
                    for (std::size_t j = 0; j < delta.size(); ++j)
                        delta[j] += lambda * sample.gradient[j];
                }
            }

            const double trustRegion = std::max(0.015, std::min(request.interpolationStep, request.sampledTreeStep * 0.35));
            const double deltaNorm = norm(delta);
            if (deltaNorm > trustRegion)
            {
                const double scale = trustRegion / deltaNorm;
                for (double& value : delta)
                    value *= scale;
            }

            return delta;
        }

        bool segmentHasClearance(
            const ICdfDistanceOracle& oracle,
            const JointVector& a,
            const JointVector& b,
            double interpolationStep,
            double minimumClearance)
        {
            const std::size_t count = std::max<std::size_t>(
                1,
                static_cast<std::size_t>(std::ceil(distance(a, b) / std::max(interpolationStep, 1.0e-4))));
            for (std::size_t i = 0; i <= count; ++i)
            {
                const JointVector q = lerp(a, b, static_cast<double>(i) / static_cast<double>(count));
                if (!stateHasClearance(oracle, q, minimumClearance))
                    return false;
            }
            return true;
        }

        Path smoothCollisionFreePath(
            const PlanningRequest& request,
            const ICdfDistanceOracle& oracle,
            const Path& path)
        {
            if (path.size() < 3)
                return path;

            Path smoothed = resamplePath(path, std::max(0.012, request.interpolationStep * 0.45));
            copyEndpoint(request, smoothed);

            const double acceptedClearance = 0.0;
            const double smoothingStep = std::max(0.10, request.smoothGain * 1.5);
            const std::size_t passCount = 90;
            for (std::size_t pass = 0; pass < passCount; ++pass)
            {
                bool changed = false;
                Path next = smoothed;
                for (std::size_t i = 1; i + 1 < smoothed.size(); ++i)
                {
                    JointVector candidate = smoothed[i];
                    for (std::size_t j = 0; j < candidate.size(); ++j)
                    {
                        const double midpoint = 0.5 * (smoothed[i - 1][j] + smoothed[i + 1][j]);
                        candidate[j] += smoothingStep * (midpoint - smoothed[i][j]);
                    }
                    candidate = clampToBounds(candidate, request.bounds);

                    if (!stateHasClearance(oracle, candidate, acceptedClearance))
                        continue;
                    if (!segmentHasClearance(
                            oracle,
                            smoothed[i - 1],
                            candidate,
                            request.interpolationStep,
                            acceptedClearance))
                        continue;
                    if (!segmentHasClearance(
                            oracle,
                            candidate,
                            smoothed[i + 1],
                            request.interpolationStep,
                            acceptedClearance))
                        continue;

                    const double oldCost =
                        squaredDistance(smoothed[i - 1], smoothed[i]) + squaredDistance(smoothed[i], smoothed[i + 1]);
                    const double newCost =
                        squaredDistance(smoothed[i - 1], candidate) + squaredDistance(candidate, smoothed[i + 1]);
                    if (newCost <= oldCost)
                    {
                        next[i] = std::move(candidate);
                        changed = true;
                    }
                }

                copyEndpoint(request, next);
                smoothed = std::move(next);
                if (!changed)
                    break;
            }

            return resamplePath(smoothed, std::max(0.010, request.interpolationStep * 0.35));
        }

        std::vector<CircularObstacle> defaultObstacles()
        {
            return {
                { 0.170, 0.110, 0.035, "obs_01_elbow_cylinder" },
                { 0.310, -0.090, 0.047, "obs_02_front_box" },
                { 0.430, 0.075, 0.028, "obs_03_outer_cylinder" },
                { 0.120, -0.185, 0.076, "obs_04_left_wall" },
                { 0.370, 0.185, 0.046, "obs_05_tool_goal_block" }
            };
        }

        PlanningRequest makeBaseDemoRequest()
        {
            PlanningRequest request;
            request.sceneKind = CdfSceneKind::ThreeDofRobot3R;
            request.bounds = {
                { -3.141593, 3.141593 },
                { -2.617994, 2.617994 },
                { -2.617994, 2.617994 }
            };
            request.start = { 1.634, -1.209, 0.376 };
            request.goal = { 1.382, 1.130, 0.731 };
            request.obstacles = defaultObstacles();
            request.safetyMargin = 0.012;
            request.targetClearance = 0.008;
            request.finiteDifferenceStep = 5.0e-4;
            request.interpolationStep = 0.055;
            request.sampledTreeStep = 0.26;
            request.sampledTreeMaxNodes = 2400;
            request.maxRepairIterations = 120;
            request.repairGain = 0.65;
            request.smoothGain = 0.08;
            request.randomSeed = 4204600u;
            return request;
        }

        struct TreeNode
        {
            JointVector q;
            int parent = -1;
        };

        int nearestNode(const std::vector<TreeNode>& tree, const JointVector& q)
        {
            double best = std::numeric_limits<double>::max();
            int bestIndex = 0;
            for (std::size_t i = 0; i < tree.size(); ++i)
            {
                const double candidate = squaredDistance(tree[i].q, q);
                if (candidate < best)
                {
                    best = candidate;
                    bestIndex = static_cast<int>(i);
                }
            }
            return bestIndex;
        }

        JointVector steerToward(const JointVector& from, const JointVector& to, double step)
        {
            const double length = distance(from, to);
            if (length <= step || length <= epsilon)
                return to;
            return lerp(from, to, step / length);
        }

        int extendTree(
            std::vector<TreeNode>& tree,
            const JointVector& target,
            const ICdfDistanceOracle& oracle,
            const PlanningRequest& request)
        {
            const int parent = nearestNode(tree, target);
            const JointVector candidate = clampToBounds(
                steerToward(tree[static_cast<std::size_t>(parent)].q, target, request.sampledTreeStep),
                request.bounds);

            if (sameState(tree[static_cast<std::size_t>(parent)].q, candidate))
                return -1;
            if (!segmentSafe(oracle, tree[static_cast<std::size_t>(parent)].q, candidate, request.interpolationStep))
                return -1;

            tree.push_back({ candidate, parent });
            return static_cast<int>(tree.size() - 1);
        }

        int tryConnectTree(
            std::vector<TreeNode>& tree,
            const JointVector& target,
            const ICdfDistanceOracle& oracle,
            const PlanningRequest& request)
        {
            int newest = -1;
            const std::size_t maxConnectSteps = 64;
            for (std::size_t i = 0; i < maxConnectSteps; ++i)
            {
                newest = extendTree(tree, target, oracle, request);
                if (newest < 0)
                    return -1;
                if (distance(tree[static_cast<std::size_t>(newest)].q, target) <= request.sampledTreeStep)
                {
                    if (segmentSafe(oracle, tree[static_cast<std::size_t>(newest)].q, target, request.interpolationStep))
                    {
                        tree.push_back({ target, newest });
                        return static_cast<int>(tree.size() - 1);
                    }
                }
            }
            return -1;
        }

        Path nodePath(const std::vector<TreeNode>& tree, int index)
        {
            Path path;
            while (index >= 0)
            {
                path.push_back(tree[static_cast<std::size_t>(index)].q);
                index = tree[static_cast<std::size_t>(index)].parent;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        void appendReverseWithoutDuplicate(Path& destination, const Path& source)
        {
            if (source.empty())
                return;
            for (auto iterator = source.rbegin() + 1; iterator != source.rend(); ++iterator)
                destination.push_back(*iterator);
        }

        Path mergeTreePaths(
            const std::vector<TreeNode>& startTree,
            int startIndex,
            const std::vector<TreeNode>& goalTree,
            int goalIndex)
        {
            Path startPath = nodePath(startTree, startIndex);
            Path goalPath = nodePath(goalTree, goalIndex);
            appendReverseWithoutDuplicate(startPath, goalPath);
            return startPath;
        }

        bool pathHasSafeEndpoints(const ICdfDistanceOracle& oracle, const PlanningRequest& request)
        {
            return stateSafe(oracle, request.start) && stateSafe(oracle, request.goal);
        }

        void copyEndpoint(const PlanningRequest& request, Path& path)
        {
            if (path.empty())
                return;
            path.front() = request.start;
            path.back() = request.goal;
        }
    }

    PlanningResult CdfPlanningService::plan(const PlanningRequest& request) const
    {
        checkedDof(request);
        if (request.sceneKind == CdfSceneKind::ThreeDofRobot3R)
        {
            ThreeDofRobotCdfOracle oracle(request.bounds, request.obstacles, request.safetyMargin);
            return plan(request, oracle);
        }

        CircularObstacleCdfOracle oracle(request.bounds, request.obstacles, request.safetyMargin);
        return plan(request, oracle);
    }

    PlanningResult CdfPlanningService::plan(
        const PlanningRequest& request,
        const ICdfDistanceOracle& oracle) const
    {
        checkedDof(request);
        if (oracle.dof() != request.bounds.size())
            throw std::invalid_argument("CDF planning request and oracle dimensions do not match.");

        PlanningResult result;
        result.initialPath = resamplePath(makeInitialPath(request, oracle), request.interpolationStep);
        result.initialClearances = sampleClearances(request, oracle, result.initialPath);
        result.statistics.minClearanceBeforeRepair = minValue(result.initialClearances);
        result.statistics.waypointCountBeforeRepair = result.initialPath.size();
        result.statistics.usedProvidedOmplSeed =
            request.initialPathStrategy == InitialPathStrategy::OmplSeedPath && !request.omplSeedPath.empty();
        result.statistics.usedSampledTreeSeed =
            request.initialPathStrategy == InitialPathStrategy::OmplStyleSampledTree;

        result.repairedPath = repairPath(request, oracle, result.initialPath, &result.repairTrace);
        result.repairedClearances = sampleClearances(request, oracle, result.repairedPath);
        result.statistics.minClearanceAfterRepair = minValue(result.repairedClearances);
        result.statistics.waypointCountAfterRepair = result.repairedPath.size();
        result.statistics.repairIterations = result.repairTrace.size();
        result.statistics.collisionFree = isPathCollisionFree(request, oracle, result.repairedPath);
        result.message = result.statistics.collisionFree
            ? "CDF path is collision-free at the requested interpolation resolution."
            : "CDF repair did not fully clear the path; try an OMPL seed or more iterations.";
        return result;
    }

    ClearanceSample CdfPlanningService::clearanceAndGradient(
        const PlanningRequest& request,
        const JointVector& q) const
    {
        checkedDof(request);
        if (request.sceneKind == CdfSceneKind::ThreeDofRobot3R)
        {
            ThreeDofRobotCdfOracle oracle(request.bounds, request.obstacles, request.safetyMargin);
            return clearanceAndGradient(oracle, request.finiteDifferenceStep, q);
        }

        CircularObstacleCdfOracle oracle(request.bounds, request.obstacles, request.safetyMargin);
        return clearanceAndGradient(oracle, request.finiteDifferenceStep, q);
    }

    ClearanceSample CdfPlanningService::clearanceAndGradient(
        const ICdfDistanceOracle& oracle,
        double finiteDifferenceStep,
        const JointVector& q) const
    {
        FiniteDifferenceCdfOracle finiteDifference(oracle, finiteDifferenceStep);
        return finiteDifference.evaluate(q);
    }

    Path CdfPlanningService::makeInitialPath(
        const PlanningRequest& request,
        const ICdfDistanceOracle& oracle) const
    {
        checkedDof(request);

        if (request.initialPathStrategy == InitialPathStrategy::OmplSeedPath && !request.omplSeedPath.empty())
            return request.omplSeedPath;

        if (request.initialPathStrategy == InitialPathStrategy::OmplStyleSampledTree)
        {
            Path sampledPath = makeSampledTreePath(request, oracle);
            if (sampledPath.size() >= 2)
                return sampledPath;
        }

        return makeLinearPath(request);
    }

    Path CdfPlanningService::repairPath(
        const PlanningRequest& request,
        const ICdfDistanceOracle& oracle,
        const Path& seedPath,
        std::vector<CdfRepairIteration>* trace) const
    {
        checkedDof(request);
        if (seedPath.size() < 2)
            return makeLinearPath(request);

        Path path = resamplePath(seedPath, request.interpolationStep);
        copyEndpoint(request, path);

        FiniteDifferenceCdfOracle finiteDifference(oracle, request.finiteDifferenceStep);
        for (std::size_t iteration = 0; iteration < request.maxRepairIterations; ++iteration)
        {
            Path next = path;
            double maxCorrection = 0.0;

            for (std::size_t i = 1; i + 1 < path.size(); ++i)
            {
                const ClearanceSample sample = finiteDifference.evaluate(path[i]);
                const JointVector correction = solveCdfQpCorrection(request, path, i, sample);

                for (std::size_t j = 0; j < correction.size(); ++j)
                    next[i][j] = path[i][j] + correction[j];
                next[i] = clampToBounds(next[i], request.bounds);
                maxCorrection = std::max(maxCorrection, norm(correction));
            }

            copyEndpoint(request, next);
            path = std::move(next);

            const std::vector<double> clearances = sampleClearances(request, oracle, path);
            const double currentMin = minValue(clearances);
            if (trace != nullptr)
                trace->push_back({ iteration + 1, currentMin, maxCorrection });

            if (currentMin >= request.targetClearance && maxCorrection < 1.0e-4)
                break;
        }

        return smoothCollisionFreePath(request, oracle, path);
    }

    bool CdfPlanningService::isPathCollisionFree(
        const PlanningRequest& request,
        const ICdfDistanceOracle& oracle,
        const Path& path) const
    {
        if (path.size() < 2)
            return false;

        for (std::size_t i = 1; i < path.size(); ++i)
        {
            if (!segmentSafe(oracle, path[i - 1], path[i], request.interpolationStep))
                return false;
        }
        return true;
    }

    std::vector<double> CdfPlanningService::sampleClearances(
        const PlanningRequest& request,
        const ICdfDistanceOracle& oracle,
        const Path& path) const
    {
        std::vector<double> clearances;
        const Path sampled = resamplePath(path, request.interpolationStep);
        clearances.reserve(sampled.size());
        for (const JointVector& q : sampled)
            clearances.push_back(oracle.signedDistance(q));
        return clearances;
    }

    Path CdfPlanningService::makeLinearPath(const PlanningRequest& request) const
    {
        return { request.start, request.goal };
    }

    Path CdfPlanningService::makeSampledTreePath(
        const PlanningRequest& request,
        const ICdfDistanceOracle& oracle) const
    {
        if (!pathHasSafeEndpoints(oracle, request))
            return {};

        std::mt19937 random(request.randomSeed);
        std::vector<std::uniform_real_distribution<double>> distributions;
        distributions.reserve(request.bounds.size());
        for (const JointBounds& bound : request.bounds)
            distributions.emplace_back(bound.lower, bound.upper);

        std::vector<TreeNode> startTree = { { request.start, -1 } };
        std::vector<TreeNode> goalTree = { { request.goal, -1 } };

        const auto randomState = [&]() {
            JointVector q(request.bounds.size(), 0.0);
            for (std::size_t i = 0; i < q.size(); ++i)
                q[i] = distributions[i](random);
            return q;
        };

        for (std::size_t iteration = 0; iteration < request.sampledTreeMaxNodes; ++iteration)
        {
            const bool growStartTree = (iteration % 2) == 0;
            std::vector<TreeNode>& activeTree = growStartTree ? startTree : goalTree;
            std::vector<TreeNode>& passiveTree = growStartTree ? goalTree : startTree;

            JointVector target = randomState();
            if (iteration % 8 == 0)
                target = growStartTree ? request.goal : request.start;

            const int activeIndex = extendTree(activeTree, target, oracle, request);
            if (activeIndex < 0)
                continue;

            const int passiveIndex = tryConnectTree(
                passiveTree,
                activeTree[static_cast<std::size_t>(activeIndex)].q,
                oracle,
                request);
            if (passiveIndex < 0)
                continue;

            if (growStartTree)
                return mergeTreePaths(startTree, activeIndex, goalTree, passiveIndex);
            return mergeTreePaths(startTree, passiveIndex, goalTree, activeIndex);
        }

        return {};
    }

    PlanningRequest makeDistanceFieldDemoRequest()
    {
        PlanningRequest request = makeBaseDemoRequest();
        request.initialPathStrategy = InitialPathStrategy::Linear;
        request.maxRepairIterations = 0;
        return request;
    }

    PlanningRequest makeOmplSeedDemoRequest()
    {
        PlanningRequest request = makeBaseDemoRequest();
        request.initialPathStrategy = InitialPathStrategy::OmplStyleSampledTree;
        request.maxRepairIterations = 0;
        request.randomSeed = 20260726u;
        return request;
    }

    PlanningRequest makeRepairDemoRequest()
    {
        PlanningRequest request = makeBaseDemoRequest();
        request.initialPathStrategy = InitialPathStrategy::Linear;
        request.maxRepairIterations = 110;
        request.randomSeed = 314159u;
        return request;
    }
}
