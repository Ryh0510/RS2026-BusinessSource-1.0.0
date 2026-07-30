#include "CDFAlgorithms/CdfDocument.h"

#include <SimulationProject/ProjectDocument.h>
#include <SimulationRuntime/ProjectCollisionRuntime.h>
#include <SimulationRuntime/ProjectSimulationRuntime.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    constexpr const char* kRobotId = "cdf_three_dof_robot_3r";
    constexpr const char* kObstacleObjectId = "cdf_obstacle_scene";
    constexpr const char* kDetectorId = "cdf_robot_obstacle_collision";

    double environmentDouble(const char* name, double fallback)
    {
        const char* value = std::getenv(name);
        if(value == nullptr || *value == '\0') {
            return fallback;
        }
        char* end = nullptr;
        const double parsed = std::strtod(value, &end);
        return end != value ? parsed : fallback;
    }

    cdf::JointVector environmentVector3(
        const char* prefix,
        const cdf::JointVector& fallback)
    {
        cdf::JointVector result = fallback;
        for(std::size_t i = 0; i < result.size() && i < 3; ++i) {
            std::string name = prefix;
            name += std::to_string(i);
            result[i] = environmentDouble(name.c_str(), result[i]);
        }
        return result;
    }

    double jointDistance(const cdf::JointVector& a, const cdf::JointVector& b)
    {
        double total = 0.0;
        for(std::size_t i = 0; i < a.size(); ++i) {
            const double delta = a[i] - b[i];
            total += delta * delta;
        }
        return std::sqrt(total);
    }

    cdf::JointVector lerp(const cdf::JointVector& a, const cdf::JointVector& b, double t)
    {
        cdf::JointVector result(a.size(), 0.0);
        for(std::size_t i = 0; i < a.size(); ++i) {
            result[i] = a[i] + (b[i] - a[i]) * t;
        }
        return result;
    }

    cdf::Path resamplePath(const cdf::Path& path, double maxStep)
    {
        if(path.size() < 2) {
            return path;
        }

        cdf::Path result;
        result.push_back(path.front());
        for(std::size_t segment = 1; segment < path.size(); ++segment) {
            const cdf::JointVector& from = path[segment - 1];
            const cdf::JointVector& to = path[segment];
            const std::size_t count = std::max<std::size_t>(
                1,
                static_cast<std::size_t>(std::ceil(jointDistance(from, to) / std::max(maxStep, 1.0e-4))));
            for(std::size_t i = 1; i <= count; ++i) {
                result.push_back(lerp(from, to, static_cast<double>(i) / static_cast<double>(count)));
            }
        }
        return result;
    }

    void appendInitialJoint(
        simulation_project::RobotDesc& robot,
        const char* jointName,
        const cdf::JointVector& values,
        std::size_t index)
    {
        simulation_project::JointValueDesc joint;
        joint.jointName = jointName;
        joint.value = index < values.size() ? values[index] : 0.0;
        robot.initialJoints.push_back(joint);
    }

    simulation_project::ProjectDocument makeProjectDocument(const cdf::JointVector& initialJoints)
    {
        simulation_project::ProjectDocument document;

        simulation_project::RobotDesc robot;
        robot.id = kRobotId;
        robot.name = "three_dof_robot_3r";
        robot.sourceType = "urdf";
        robot.sourcePath = CDF_3R_URDF_PATH;
        robot.visible = true;
        robot.collisionEnabled = true;
        appendInitialJoint(robot, "joint1", initialJoints, 0);
        appendInitialJoint(robot, "joint2", initialJoints, 1);
        appendInitialJoint(robot, "joint3", initialJoints, 2);
        document.robots.push_back(robot);

        simulation_project::SceneObjectDesc obstacles;
        obstacles.id = kObstacleObjectId;
        obstacles.name = "ThreeDOF_Robot_ObstacleScene";
        obstacles.objectType = "fixture";
        obstacles.sourcePath = CDF_OBSTACLE_SCENE_MESH_PATH;
        obstacles.visible = true;
        obstacles.collisionEnabled = true;
        obstacles.transform.x = environmentDouble("CDF_OBSTACLE_X", -0.05);
        obstacles.transform.z = environmentDouble("CDF_OBSTACLE_Z", 0.06);
        obstacles.transform.roll = 1.5707963267948966;
        obstacles.visualScale = 1.0;
        obstacles.collisionScale = 1.0;
        document.objects.push_back(obstacles);

        simulation_project::CollisionDetectorTargetDesc robotTarget;
        robotTarget.robotId = kRobotId;
        simulation_project::CollisionDetectorTargetDesc objectTarget;
        objectTarget.objectId = kObstacleObjectId;

        simulation_project::CollisionPairGeneratorDesc generator;
        generator.type = "RobotObject";
        generator.robotId = kRobotId;
        generator.objectId = kObstacleObjectId;

        simulation_project::CollisionDetectorDesc detector;
        detector.id = kDetectorId;
        detector.name = "CDF robot vs obstacle scene";
        detector.type = "RobotObject";
        detector.enabled = true;
        detector.contacts = true;
        detector.nearestPoints = true;
        detector.distance = true;
        detector.maxContacts = 32;
        detector.targets.push_back(robotTarget);
        detector.targets.push_back(objectTarget);
        detector.pairGenerators.push_back(generator);
        document.collision.detectors.push_back(detector);

        return document;
    }

    struct ProjectCollisionPathReport
    {
        bool loaded = false;
        bool collisionFree = false;
        std::size_t collidingSamples = 0;
        std::size_t sampleCount = 0;
        double minDistance = std::numeric_limits<double>::max();
        std::string firstCollisionPair;
        std::string error;
    };

    ProjectCollisionPathReport validateWithProjectCollision(const cdf::Path& path)
    {
        ProjectCollisionPathReport report;
        if(path.empty()) {
            report.error = "empty path";
            return report;
        }

        simulation_runtime::ProjectSimulationRuntime simulation;
        const simulation_project::ProjectDocument document = makeProjectDocument(path.front());
        simulation_runtime::Result result =
            simulation.loadProject(document, std::filesystem::path(CDF_PROJECT_SOURCE_PATH));
        if(!result.success) {
            report.error = result.message;
            return report;
        }

        simulation_runtime::ProjectCollisionRuntime collisionRuntime;
        result = collisionRuntime.build(simulation);
        if(!result.success) {
            report.error = result.message;
            return report;
        }
        collisionRuntime.setActiveDetector(kDetectorId);
        report.loaded = true;

        const cdf::Path samples = resamplePath(path, 0.035);
        report.sampleCount = samples.size();
        for(const cdf::JointVector& q : samples) {
            result = simulation.setRobotJointValues(kRobotId, { "joint1", "joint2", "joint3" }, q);
            if(!result.success) {
                report.error = result.message;
                return report;
            }
            simulation.update(0.0);
            result = collisionRuntime.update(simulation);
            if(!result.success) {
                report.error = result.message;
                return report;
            }
            result = collisionRuntime.checkDetector(kDetectorId);
            if(!result.success) {
                report.error = result.message;
                return report;
            }

            const collision::CollisionResult* collisionResult = collisionRuntime.resultOf(kDetectorId);
            if(collisionResult == nullptr) {
                report.error = "project collision detector returned no result";
                return report;
            }
            report.minDistance = std::min(report.minDistance, collisionResult->minDistance);
            if(collisionResult->inCollision()) {
                ++report.collidingSamples;
                if(report.firstCollisionPair.empty() && !collisionResult->contacts.empty()) {
                    const collision::Contact& contact = collisionResult->contacts.front();
                    report.firstCollisionPair = contact.infoA.linkName
                        + "/"
                        + contact.infoA.elementName
                        + " vs "
                        + contact.infoB.linkName
                        + "/"
                        + contact.infoB.elementName;
                }
            }
        }

        report.collisionFree = report.collidingSamples == 0;
        return report;
    }

    bool runCase(cdf::DemoCase demoCase, const char* label)
    {
        cdf::CdfDocument document;
        document.loadDemo(demoCase);
        const cdf::JointVector startOverride = environmentVector3("CDF_START_Q", document.request().start);
        const cdf::JointVector goalOverride = environmentVector3("CDF_GOAL_Q", document.request().goal);
        for(std::size_t i = 0; i < startOverride.size(); ++i) {
            document.setStartJoint(i, startOverride[i]);
        }
        for(std::size_t i = 0; i < goalOverride.size(); ++i) {
            document.setGoalJoint(i, goalOverride[i]);
        }
        const cdf::PlanningResult planning = document.runPlanning();
        const ProjectCollisionPathReport initialReport =
            validateWithProjectCollision(planning.initialPath);
        const ProjectCollisionPathReport repairedReport =
            validateWithProjectCollision(planning.repairedPath);

        std::cout << label
                  << ": initialCollidingSamples=" << initialReport.collidingSamples
                  << "/" << initialReport.sampleCount
                  << ", repairedCollidingSamples=" << repairedReport.collidingSamples
                  << "/" << repairedReport.sampleCount
                  << ", projectCollisionFree=" << (repairedReport.collisionFree ? "yes" : "no")
                  << ", projectMinDistance=" << repairedReport.minDistance;
        if(!repairedReport.firstCollisionPair.empty()) {
            std::cout << ", firstPair=" << repairedReport.firstCollisionPair;
        }
        if(!initialReport.error.empty()) {
            std::cout << ", initialError=" << initialReport.error;
        }
        if(!repairedReport.error.empty()) {
            std::cout << ", repairedError=" << repairedReport.error;
        }
        std::cout << '\n';

        return initialReport.loaded && repairedReport.loaded && repairedReport.collisionFree;
    }
}

int main()
{
    const bool repairOk = runCase(cdf::DemoCase::Repair, "Repair demo project collision");
    const bool seedOk = runCase(cdf::DemoCase::OmplSeed, "Seed demo project collision");
    return repairOk && seedOk ? 0 : 1;
}
