#include "ProjectCollisionRuntimeProjection.h"

#include <RobotCore/RobotModel.h>

#include <algorithm>
#include <utility>

namespace
{
    std::string normalizedLinkPair(std::string first, std::string second)
    {
        if(second < first)
            std::swap(first, second);
        return first + "|" + second;
    }

    std::vector<std::string> adjacentLinkPairs(const robot::RobotModel& model)
    {
        std::vector<std::string> pairs;
        pairs.reserve(model.joints.size());
        for(const robot::RobotJoint& joint : model.joints) {
            if(!joint.parent.empty() && !joint.child.empty())
                pairs.push_back(normalizedLinkPair(joint.parent, joint.child));
        }
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        return pairs;
    }

    std::vector<simulation_runtime::RuntimeCollisionRobot> projectRobots(
        const std::vector<RuntimeRobot>& robots)
    {
        std::vector<simulation_runtime::RuntimeCollisionRobot> projected;
        projected.reserve(robots.size());
        for(const RuntimeRobot& robot : robots) {
            simulation_runtime::RuntimeCollisionRobot value;
            value.documentId = robot.documentId;
            value.collisionEnabled = robot.collisionEnabled;
            value.collisionInstance = robot.collisionInstance;
            value.adjacentLinkPairs = adjacentLinkPairs(robot.model);
            projected.push_back(std::move(value));
        }
        return projected;
    }

    void appendProjectedObject(
        std::vector<simulation_runtime::RuntimeCollisionObject>& projected,
        const RuntimeSceneObject& object,
        const RuntimeSceneCollisionObject& collisionRuntime)
    {
        const collision::CollisionObjectPtr& collisionObject = collisionRuntime.collisionObject;
        if(!collisionObject)
            return;

        simulation_runtime::RuntimeCollisionObject value;
        value.documentId = object.documentId;
        value.name = object.name;
        value.objectType = object.objectType;
        value.modelId = collisionRuntime.modelId.empty()
            ? collisionRuntime.collisionShape.modelId
            : collisionRuntime.modelId;
        value.currentModel = collisionRuntime.currentModel;
        value.collisionEnabled = object.collisionEnabled;
        value.collisionObject = collisionObject;
        value.localTransform = collisionRuntime.localTransform;
        projected.push_back(std::move(value));
    }

    std::vector<simulation_runtime::RuntimeCollisionObject> projectObjects(
        const std::vector<RuntimeSceneObject>& objects)
    {
        std::size_t collisionObjectCount = 0;
        for(const RuntimeSceneObject& object : objects)
            collisionObjectCount += object.collisionObjects.empty() ? 1 : object.collisionObjects.size();

        std::vector<simulation_runtime::RuntimeCollisionObject> projected;
        projected.reserve(collisionObjectCount);
        for(const RuntimeSceneObject& object : objects) {
            if(object.collisionObjects.empty()) {
                RuntimeSceneCollisionObject collisionRuntime;
                collisionRuntime.collisionObject = object.collisionObject;
                collisionRuntime.collisionShape = object.collisionShape;
                collisionRuntime.modelId = object.collisionShape.modelId;
                collisionRuntime.currentModel = object.collisionShape.currentModel;
                appendProjectedObject(projected, object, collisionRuntime);
                continue;
            }

            for(const RuntimeSceneCollisionObject& collisionObject : object.collisionObjects) {
                appendProjectedObject(projected, object, collisionObject);
            }
        }
        return projected;
    }

    bool detectorVisible(
        const simulation_project::ProjectDocument& document,
        const std::string& detectorId)
    {
        if(document.collision.detectors.empty())
            return true;
        const auto it = std::find_if(
            document.collision.detectors.begin(),
            document.collision.detectors.end(),
            [&](const simulation_project::CollisionDetectorDesc& detector) {
                return detector.id == detectorId;
            });
        return it == document.collision.detectors.end() || it->visualization.visible;
    }
}

std::vector<ProjectCollisionDetectorViewRuntime> buildProjectCollisionDetectorViewRuntimes(
    const simulation_project::ProjectDocument& document,
    const std::vector<RuntimeRobot>& robots,
    const std::vector<RuntimeSceneObject>& objects)
{
    const std::vector<simulation_runtime::RuntimeCollisionRobot> projectedRobots = projectRobots(robots);
    const std::vector<simulation_runtime::RuntimeCollisionObject> projectedObjects = projectObjects(objects);
    std::vector<simulation_runtime::ProjectCollisionDetectorRuntime> runtimeDetectors =
        simulation_runtime::ProjectCollisionDetectorBuilder::build(
            document,
            projectedRobots,
            projectedObjects);

    std::vector<ProjectCollisionDetectorViewRuntime> result;
    result.reserve(runtimeDetectors.size());
    for(simulation_runtime::ProjectCollisionDetectorRuntime& runtimeDetector : runtimeDetectors) {
        ProjectCollisionDetectorViewRuntime projected;
        static_cast<simulation_runtime::ProjectCollisionDetectorRuntime&>(projected) =
            std::move(runtimeDetector);
        projected.visible = detectorVisible(document, projected.id);
        result.push_back(std::move(projected));
    }
    return result;
}
