#include <ProjectMotionPlanning/CdfQpTrajectoryRepair.h>

#include <ProjectMotionPlanning/ProjectMotionPlanning.h>
#include <SimulationProject/ProjectDocument.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace motion_planning
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kTwoPi = 2.0 * kPi;
        constexpr double kTiny = 1.0e-12;

        void addDiagnostic(
            std::vector<MotionPlanningDiagnostic>* diagnostics,
            const std::string& code,
            const std::string& message)
        {
            if(diagnostics != nullptr) {
                diagnostics->push_back({ code, message });
            }
        }

        bool containsCaseSensitive(
            const std::string& text,
            const std::string& token)
        {
            return text.find(token) != std::string::npos;
        }

        const simulation_project::RobotDesc* findRobot(
            const simulation_project::ProjectDocument& document,
            const std::string& robotId)
        {
            const auto it = std::find_if(
                document.robots.begin(),
                document.robots.end(),
                [&](const simulation_project::RobotDesc& robot) {
                    return robot.id == robotId;
                });
            return it == document.robots.end() ? nullptr : &(*it);
        }

        const simulation_project::RobotDesc* findBurnnerRobot(
            const simulation_project::ProjectDocument& document,
            const ProjectCdfQpRepairOptions& options,
            const std::string& planningRobotId)
        {
            const auto exactIt = std::find_if(
                document.robots.begin(),
                document.robots.end(),
                [&](const simulation_project::RobotDesc& robot) {
                    return robot.id == options.obstacleId && robot.id != planningRobotId;
                });
            if(exactIt != document.robots.end()) {
                return &(*exactIt);
            }

            const auto sourceIt = std::find_if(
                document.robots.begin(),
                document.robots.end(),
                [&](const simulation_project::RobotDesc& robot) {
                    return robot.id != planningRobotId &&
                        (containsCaseSensitive(robot.id, "burnner") ||
                         containsCaseSensitive(robot.name, "burnner") ||
                         containsCaseSensitive(robot.sourcePath, "burnner"));
                });
            return sourceIt == document.robots.end() ? nullptr : &(*sourceIt);
        }

        const simulation_project::SceneObjectDesc* findBurnnerObject(
            const simulation_project::ProjectDocument& document,
            const ProjectCdfQpRepairOptions& options)
        {
            const auto exactIt = std::find_if(
                document.objects.begin(),
                document.objects.end(),
                [&](const simulation_project::SceneObjectDesc& object) {
                    return object.id == options.obstacleId;
                });
            if(exactIt != document.objects.end()) {
                return &(*exactIt);
            }

            const auto sourceIt = std::find_if(
                document.objects.begin(),
                document.objects.end(),
                [](const simulation_project::SceneObjectDesc& object) {
                    return containsCaseSensitive(object.id, "burnner") ||
                        containsCaseSensitive(object.name, "burnner") ||
                        containsCaseSensitive(object.sourcePath, "burnner");
                });
            return sourceIt == document.objects.end() ? nullptr : &(*sourceIt);
        }

        bool hasDetector(
            const simulation_project::ProjectDocument& document,
            const std::string& detectorId)
        {
            return std::any_of(
                document.collision.detectors.begin(),
                document.collision.detectors.end(),
                [&](const simulation_project::CollisionDetectorDesc& detector) {
                    return detector.id == detectorId;
                });
        }

        simulation_project::CollisionDetectorDesc makeRobotRobotDetector(
            const std::string& detectorId,
            const std::string& planningRobotId,
            const std::string& obstacleRobotId,
            const ProjectCdfQpRepairOptions& options)
        {
            simulation_project::CollisionDetectorDesc detector;
            detector.id = detectorId;
            detector.name = "CDF ABB4600-burnner detector";
            detector.type = "RobotRobot";
            detector.enabled = true;
            detector.geometryRole = "Exact";
            detector.contacts = true;
            detector.nearestPoints = true;
            detector.distance = true;
            detector.maxContacts = 64;
            detector.distanceThreshold = options.distanceThreshold;

            simulation_project::CollisionDetectorTargetDesc robotTarget;
            robotTarget.robotId = planningRobotId;
            detector.targets.push_back(std::move(robotTarget));

            simulation_project::CollisionDetectorTargetDesc obstacleTarget;
            obstacleTarget.robotId = obstacleRobotId;
            detector.targets.push_back(std::move(obstacleTarget));
            return detector;
        }

        simulation_project::CollisionDetectorDesc makeRobotObjectDetector(
            const std::string& detectorId,
            const std::string& planningRobotId,
            const std::string& obstacleObjectId,
            const ProjectCdfQpRepairOptions& options)
        {
            simulation_project::CollisionDetectorDesc detector;
            detector.id = detectorId;
            detector.name = "CDF ABB4600-burnner detector";
            detector.type = "RobotObject";
            detector.enabled = true;
            detector.geometryRole = "Exact";
            detector.contacts = true;
            detector.nearestPoints = true;
            detector.distance = true;
            detector.maxContacts = 64;
            detector.distanceThreshold = options.distanceThreshold;

            simulation_project::CollisionDetectorTargetDesc robotTarget;
            robotTarget.robotId = planningRobotId;
            detector.targets.push_back(std::move(robotTarget));

            simulation_project::CollisionDetectorTargetDesc obstacleTarget;
            obstacleTarget.objectId = obstacleObjectId;
            detector.targets.push_back(std::move(obstacleTarget));
            return detector;
        }

        double wrapContinuousAngle(double value)
        {
            if(!std::isfinite(value)) {
                return value;
            }

            double wrapped = std::remainder(value, kTwoPi);
            if(wrapped <= -kPi) {
                wrapped += kTwoPi;
            } else if(wrapped > kPi) {
                wrapped -= kTwoPi;
            }
            return wrapped;
        }

        std::vector<double> clampToBounds(
            const std::vector<double>& q,
            const std::vector<JointBound>& bounds,
            int* clampedCount)
        {
            std::vector<double> clamped = q;
            const std::size_t count = std::min(clamped.size(), bounds.size());
            for(std::size_t index = 0; index < count; ++index) {
                const double before = clamped[index];
                clamped[index] = bounds[index].continuous
                    ? wrapContinuousAngle(clamped[index])
                    : std::max(bounds[index].lower, std::min(bounds[index].upper, clamped[index]));
                if(before != clamped[index] && clampedCount != nullptr) {
                    ++(*clampedCount);
                }
            }
            return clamped;
        }

        double vectorNorm(const std::vector<double>& values)
        {
            double sum = 0.0;
            for(const double value : values) {
                sum += value * value;
            }
            return std::sqrt(sum);
        }

        double dot(
            const std::vector<double>& lhs,
            const std::vector<double>& rhs)
        {
            double value = 0.0;
            const std::size_t count = std::min(lhs.size(), rhs.size());
            for(std::size_t index = 0; index < count; ++index) {
                value += lhs[index] * rhs[index];
            }
            return value;
        }

        struct SignedDistanceSample
        {
            bool valid = false;
            bool inCollision = false;
            double phi = -std::numeric_limits<double>::max();
            double rawDistance = std::numeric_limits<double>::max();
            std::string message;
        };

        SignedDistanceSample evaluateSignedPhi(
            ProjectPlanningSceneSnapshot& scene,
            const std::vector<double>& q,
            double safetyMargin,
            double distanceThreshold,
            ProjectCdfQpRepairStatistics& statistics)
        {
            SignedDistanceSample sample;

            std::string stateError;
            if(!scene.setState(q, &stateError)) {
                sample.message = stateError.empty() ? "Failed to set planning scene state." : stateError;
                return sample;
            }

            bool sawFiniteDistance = false;
            bool checkedAnyDetector = false;
            double minimumPhi = std::numeric_limits<double>::max();
            double minimumDistance = std::numeric_limits<double>::max();
            bool inCollision = false;

            for(const std::string& detectorId : scene.collisionDetectorIds()) {
                checkedAnyDetector = true;
                ++statistics.collisionQueries;
                const simulation_runtime::Result checkResult =
                    scene.collisionRuntime().checkDetector(detectorId);
                if(!checkResult.success) {
                    sample.message = checkResult.message;
                    return sample;
                }

                const collision::CollisionResult* collisionResult =
                    scene.collisionRuntime().resultOf(detectorId);
                if(collisionResult == nullptr) {
                    sample.message = "Collision detector produced no result: " + detectorId;
                    return sample;
                }

                if(collisionResult->inCollision()) {
                    inCollision = true;
                    double penetrationDepth = 0.0;
                    for(const collision::Contact& contact : collisionResult->contacts) {
                        if(std::isfinite(contact.penetrationDepth)) {
                            penetrationDepth = std::max(penetrationDepth, contact.penetrationDepth);
                        }
                    }
                    const double signedDistance = -(penetrationDepth > 0.0 ? penetrationDepth : 1.0e-5);
                    minimumPhi = std::min(minimumPhi, signedDistance - safetyMargin);
                    minimumDistance = std::min(minimumDistance, signedDistance);
                    sawFiniteDistance = true;
                } else if(std::isfinite(collisionResult->minDistance) &&
                    collisionResult->minDistance < std::numeric_limits<double>::max() * 0.25) {
                    sawFiniteDistance = true;
                    minimumDistance = std::min(minimumDistance, collisionResult->minDistance);
                    minimumPhi = std::min(minimumPhi, collisionResult->minDistance - safetyMargin);
                }
            }

            if(!inCollision && !sawFiniteDistance && checkedAnyDetector) {
                const double saturatedDistance =
                    distanceThreshold > 0.0 && std::isfinite(distanceThreshold)
                        ? distanceThreshold
                        : safetyMargin;
                sample.valid = true;
                sample.inCollision = false;
                sample.rawDistance = saturatedDistance;
                sample.phi = saturatedDistance - safetyMargin;
                return sample;
            }

            if(!sawFiniteDistance) {
                sample.message = "Collision detector did not report a usable distance or collision result.";
                return sample;
            }

            sample.valid = true;
            sample.inCollision = inCollision;
            sample.rawDistance = minimumDistance;
            sample.phi = minimumPhi;
            return sample;
        }

        struct CdfLinearization
        {
            SignedDistanceSample sample;
            std::vector<double> gradient;
        };

        CdfLinearization linearizeSignedPhi(
            ProjectPlanningSceneSnapshot& scene,
            const std::vector<double>& q,
            const std::vector<JointBound>& bounds,
            double safetyMargin,
            double distanceThreshold,
            double finiteDifferenceStep,
            ProjectCdfQpRepairStatistics& statistics)
        {
            CdfLinearization linearization;
            linearization.sample = evaluateSignedPhi(scene, q, safetyMargin, distanceThreshold, statistics);
            linearization.gradient.assign(q.size(), 0.0);
            if(!linearization.sample.valid) {
                return linearization;
            }

            const double step = finiteDifferenceStep > 0.0 && std::isfinite(finiteDifferenceStep)
                ? finiteDifferenceStep
                : 5.0e-4;

            for(std::size_t index = 0; index < q.size(); ++index) {
                std::vector<double> plus = q;
                std::vector<double> minus = q;
                plus[index] += step;
                minus[index] -= step;
                if(index < bounds.size() && !bounds[index].continuous) {
                    plus[index] = std::max(bounds[index].lower, std::min(bounds[index].upper, plus[index]));
                    minus[index] = std::max(bounds[index].lower, std::min(bounds[index].upper, minus[index]));
                }

                const double denominator = plus[index] - minus[index];
                if(std::abs(denominator) <= kTiny) {
                    continue;
                }

                const SignedDistanceSample plusSample =
                    evaluateSignedPhi(scene, plus, safetyMargin, distanceThreshold, statistics);
                const SignedDistanceSample minusSample =
                    evaluateSignedPhi(scene, minus, safetyMargin, distanceThreshold, statistics);

                if(plusSample.valid && minusSample.valid) {
                    linearization.gradient[index] =
                        (plusSample.phi - minusSample.phi) / denominator;
                } else if(plusSample.valid && std::abs(plus[index] - q[index]) > kTiny) {
                    linearization.gradient[index] =
                        (plusSample.phi - linearization.sample.phi) / (plus[index] - q[index]);
                } else if(minusSample.valid && std::abs(q[index] - minus[index]) > kTiny) {
                    linearization.gradient[index] =
                        (linearization.sample.phi - minusSample.phi) / (q[index] - minus[index]);
                }
            }

            return linearization;
        }

        std::filesystem::path projectBaseOrParent(
            const std::filesystem::path& projectBasePath)
        {
            return projectBasePath;
        }

        std::string segmentFailureMessage(
            std::size_t index,
            const StateValidationResult& validation)
        {
            std::ostringstream stream;
            stream << "Repaired trajectory segment " << (index + 1)
                   << " -> " << (index + 2) << " is still invalid";
            if(!validation.message.empty()) {
                stream << ": " << validation.message;
            }
            return stream.str();
        }
    }

    bool ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(
        simulation_project::ProjectDocument& document,
        const std::string& robotId,
        const ProjectCdfQpRepairOptions& options,
        std::string* detectorId,
        std::vector<MotionPlanningDiagnostic>* diagnostics)
    {
        if(robotId.empty()) {
            addDiagnostic(diagnostics, "cdf_missing_robot_id", "Planning robot id is empty.");
            return false;
        }
        if(findRobot(document, robotId) == nullptr) {
            addDiagnostic(diagnostics, "cdf_robot_missing", "Planning robot is not present in the project: " + robotId);
            return false;
        }

        document.collision.query.enabled = true;

        const simulation_project::RobotDesc* obstacleRobot =
            findBurnnerRobot(document, options, robotId);
        const simulation_project::SceneObjectDesc* obstacleObject =
            obstacleRobot == nullptr ? findBurnnerObject(document, options) : nullptr;

        if(obstacleRobot == nullptr && obstacleObject == nullptr) {
            simulation_project::RobotDesc burnner;
            burnner.id = options.obstacleId;
            burnner.name = options.obstacleName.empty() ? options.obstacleId : options.obstacleName;
            burnner.sourceType = "urdf";
            burnner.sourcePath = options.obstacleRobotSourcePath;
            burnner.collisionEnabled = true;
            burnner.visible = true;
            burnner.baseTransform.x = options.obstacleX;
            burnner.baseTransform.y = options.obstacleY;
            burnner.baseTransform.z = options.obstacleZ;
            burnner.baseTransform.roll = options.obstacleRoll;
            burnner.baseTransform.pitch = options.obstaclePitch;
            burnner.baseTransform.yaw = options.obstacleYaw;
            document.robots.push_back(std::move(burnner));
            obstacleRobot = &document.robots.back();
            addDiagnostic(diagnostics, "cdf_burnner_added", "Added burnner as a static URDF robot obstacle for CDF/QP planning.");
        }

        const std::string selectedDetectorId = options.detectorId.empty()
            ? std::string("cdf_abb4600_burnner_detector")
            : options.detectorId;

        simulation_project::CollisionDetectorDesc detector =
            obstacleRobot != nullptr
                ? makeRobotRobotDetector(selectedDetectorId, robotId, obstacleRobot->id, options)
                : makeRobotObjectDetector(selectedDetectorId, robotId, obstacleObject->id, options);

        const auto detectorIt = std::find_if(
            document.collision.detectors.begin(),
            document.collision.detectors.end(),
            [&](const simulation_project::CollisionDetectorDesc& current) {
                return current.id == selectedDetectorId;
            });
        if(detectorIt == document.collision.detectors.end()) {
            document.collision.detectors.push_back(std::move(detector));
        } else {
            *detectorIt = std::move(detector);
        }

        if(detectorId != nullptr) {
            *detectorId = selectedDetectorId;
        }
        if(hasDetector(document, selectedDetectorId)) {
            return true;
        }

        addDiagnostic(diagnostics, "cdf_detector_missing", "Failed to create the CDF collision detector.");
        return false;
    }

    ProjectCdfQpRepairResult ProjectCdfQpTrajectoryRepairService::repair(
        const simulation_project::ProjectDocument& document,
        const std::filesystem::path& projectBasePath,
        const std::string& robotId,
        const std::vector<std::string>& jointNames,
        const robottrajectory::JointTrajectory& seedTrajectory,
        const ProjectCdfQpRepairOptions& options) const
    {
        ProjectCdfQpRepairResult result;
        result.statistics.inputWaypointCount = static_cast<int>(seedTrajectory.points.size());

        if(seedTrajectory.points.size() < 2) {
            addDiagnostic(&result.diagnostics, "cdf_seed_too_short", "CDF/QP repair requires at least two trajectory points.");
            return result;
        }
        if(robotId.empty()) {
            addDiagnostic(&result.diagnostics, "cdf_missing_robot_id", "Select ABB4600_urdf before running CDF/QP repair.");
            return result;
        }
        if(jointNames.empty()) {
            addDiagnostic(&result.diagnostics, "cdf_missing_joints", "Joint names are required for CDF/QP repair.");
            return result;
        }

        for(std::size_t index = 0; index < seedTrajectory.points.size(); ++index) {
            if(seedTrajectory.points[index].q.size() != jointNames.size()) {
                std::ostringstream stream;
                stream << "Seed point " << (index + 1)
                       << " has " << seedTrajectory.points[index].q.size()
                       << " joints, expected " << jointNames.size() << ".";
                addDiagnostic(&result.diagnostics, "cdf_seed_joint_count_mismatch", stream.str());
                return result;
            }
        }

        simulation_project::ProjectDocument planningDocument = document;
        std::string detectorId;
        if(!ensureCollisionSetup(
               planningDocument,
               robotId,
               options,
               &detectorId,
               &result.diagnostics)) {
            return result;
        }

        planningDocument.collision.detectors.erase(
            std::remove_if(
                planningDocument.collision.detectors.begin(),
                planningDocument.collision.detectors.end(),
                [&](const simulation_project::CollisionDetectorDesc& detector) {
                    return detector.id != detectorId;
                }),
            planningDocument.collision.detectors.end());

        ProjectPlanningRequest request;
        request.robotId = robotId;
        request.jointNames = jointNames;
        request.start = seedTrajectory.points.front().q;
        request.goal = seedTrajectory.points.back().q;
        request.collisionDetectorIds = { detectorId };
        request.validation.maxJointStep = options.validationMaxJointStep;

        std::string sceneError;
        std::unique_ptr<ProjectPlanningSceneSnapshot> scene =
            ProjectPlanningSceneBuilder::build(
                planningDocument,
                projectBaseOrParent(projectBasePath),
                request,
                &sceneError);
        if(!scene) {
            addDiagnostic(&result.diagnostics, "cdf_scene_build_failed", sceneError);
            return result;
        }

        std::vector<std::vector<double>> path;
        path.reserve(seedTrajectory.points.size());
        for(const robottrajectory::TimedJointPoint& point : seedTrajectory.points) {
            path.push_back(clampToBounds(
                point.q,
                scene->jointBounds(),
                &result.statistics.clampedSeedValues));
        }

        auto evaluatePathMinimum = [&]() {
            double minimumPhi = std::numeric_limits<double>::max();
            for(const std::vector<double>& q : path) {
                const SignedDistanceSample sample =
                    evaluateSignedPhi(
                        *scene,
                        q,
                        options.safetyMargin,
                        options.distanceThreshold,
                        result.statistics);
                if(!sample.valid) {
                    addDiagnostic(&result.diagnostics, "cdf_distance_failed", sample.message);
                    return -std::numeric_limits<double>::max();
                }
                minimumPhi = std::min(minimumPhi, sample.phi);
            }
            return minimumPhi;
        };

        result.statistics.initialMinimumPhi = evaluatePathMinimum();
        if(result.statistics.initialMinimumPhi <= -std::numeric_limits<double>::max() * 0.25) {
            return result;
        }

        const double targetPhi = options.targetClearance;
        const int maxIterations = std::max(1, options.maxIterations);
        const double trustRegion = options.trustRegion > 0.0 && std::isfinite(options.trustRegion)
            ? options.trustRegion
            : 0.03;
        const double seedTrackingWeight = options.seedTrackingWeight > 0.0 && std::isfinite(options.seedTrackingWeight)
            ? options.seedTrackingWeight
            : 0.0;
        const std::vector<std::vector<double>> seedPath = path;

        for(int iteration = 0; iteration < maxIterations; ++iteration) {
            std::vector<CdfLinearization> linearizations;
            linearizations.reserve(path.size());

            double minimumPhi = std::numeric_limits<double>::max();
            bool allSamplesValid = true;
            for(const std::vector<double>& q : path) {
                CdfLinearization linearization = linearizeSignedPhi(
                    *scene,
                    q,
                    scene->jointBounds(),
                    options.safetyMargin,
                    options.distanceThreshold,
                    options.finiteDifferenceStep,
                    result.statistics);
                if(!linearization.sample.valid) {
                    addDiagnostic(&result.diagnostics, "cdf_linearization_failed", linearization.sample.message);
                    allSamplesValid = false;
                    break;
                }
                minimumPhi = std::min(minimumPhi, linearization.sample.phi);
                linearizations.push_back(std::move(linearization));
            }
            if(!allSamplesValid) {
                return result;
            }

            result.statistics.finalMinimumPhi = minimumPhi;
            result.statistics.iterations = iteration + 1;

            double maximumCorrection = 0.0;
            std::vector<std::vector<double>> nextPath = path;
            for(std::size_t waypoint = 0; waypoint < path.size(); ++waypoint) {
                if(options.keepEndpoints && (waypoint == 0 || waypoint + 1 == path.size())) {
                    continue;
                }

                std::vector<double> delta(path[waypoint].size(), 0.0);
                if(waypoint > 0 && waypoint + 1 < path.size()) {
                    for(std::size_t joint = 0; joint < delta.size(); ++joint) {
                        const double smoothTarget =
                            0.5 * (path[waypoint - 1][joint] + path[waypoint + 1][joint]);
                        delta[joint] = options.smoothWeight * (smoothTarget - path[waypoint][joint]);
                    }
                }

                if(waypoint < seedPath.size() && seedTrackingWeight > 0.0) {
                    for(std::size_t joint = 0; joint < delta.size(); ++joint) {
                        delta[joint] += seedTrackingWeight *
                            (seedPath[waypoint][joint] - path[waypoint][joint]);
                    }
                }

                const CdfLinearization& linearization = linearizations[waypoint];
                if(linearization.sample.phi < targetPhi) {
                    const double gradientNormSquared =
                        std::max(kTiny, dot(linearization.gradient, linearization.gradient));
                    const double requiredImprovement =
                        options.repairGain * (targetPhi - linearization.sample.phi);
                    const double predictedImprovement =
                        dot(linearization.gradient, delta);
                    if(predictedImprovement < requiredImprovement &&
                        gradientNormSquared > kTiny) {
                        const double lambda =
                            (requiredImprovement - predictedImprovement) / gradientNormSquared;
                        for(std::size_t joint = 0; joint < delta.size(); ++joint) {
                            delta[joint] += lambda * linearization.gradient[joint];
                        }
                    }
                }

                const double deltaNorm = vectorNorm(delta);
                if(deltaNorm > trustRegion && deltaNorm > kTiny) {
                    const double scale = trustRegion / deltaNorm;
                    for(double& value : delta) {
                        value *= scale;
                    }
                }

                for(std::size_t joint = 0; joint < delta.size(); ++joint) {
                    nextPath[waypoint][joint] = path[waypoint][joint] + delta[joint];
                }
                nextPath[waypoint] = clampToBounds(nextPath[waypoint], scene->jointBounds(), nullptr);

                std::vector<double> actualDelta(delta.size(), 0.0);
                for(std::size_t joint = 0; joint < delta.size(); ++joint) {
                    actualDelta[joint] = nextPath[waypoint][joint] - path[waypoint][joint];
                }
                maximumCorrection = std::max(maximumCorrection, vectorNorm(actualDelta));
            }

            result.statistics.maximumCorrection =
                std::max(result.statistics.maximumCorrection, maximumCorrection);
            path = std::move(nextPath);

            if(minimumPhi >= targetPhi && maximumCorrection < 1.0e-5) {
                break;
            }
        }

        result.statistics.finalMinimumPhi = evaluatePathMinimum();
        if(result.statistics.finalMinimumPhi <= -std::numeric_limits<double>::max() * 0.25) {
            return result;
        }

        for(std::size_t index = 0; index + 1 < path.size(); ++index) {
            const StateValidationResult validation =
                scene->validateMotion(path[index], path[index + 1], request.validation);
            if(!validation.valid) {
                ++result.statistics.invalidSegmentCount;
                addDiagnostic(&result.diagnostics, "cdf_repaired_segment_invalid", segmentFailureMessage(index, validation));
            }
        }

        result.plan.id = robotId + "_cdf_qp_repaired";
        result.plan.name = "CDF/QP repaired trajectory";
        result.plan.robotId = robotId;
        result.plan.jointNames = jointNames;
        result.plan.trajectory.name = result.plan.id;
        result.plan.trajectory.interpolation = seedTrajectory.interpolation;
        result.plan.trajectory.points.reserve(seedTrajectory.points.size());
        for(std::size_t index = 0; index < seedTrajectory.points.size(); ++index) {
            robottrajectory::TimedJointPoint point = seedTrajectory.points[index];
            point.q = path[index];
            point.qd.clear();
            point.qdd.clear();
            result.plan.trajectory.points.push_back(std::move(point));
        }

        result.success =
            result.statistics.finalMinimumPhi >= targetPhi &&
            result.statistics.invalidSegmentCount == 0;
        if(!result.success && result.diagnostics.empty()) {
            addDiagnostic(
                &result.diagnostics,
                "cdf_repair_not_converged",
                "CDF/QP repair finished but did not reach the requested clearance.");
        }
        return result;
    }
}
