#include "ProjectSceneDocumentProjectionSystem.h"

#include <algorithm>

namespace
{
    std::vector<std::string> jointNames(const robot::RobotModel& model)
    {
        std::vector<std::string> names;
        names.reserve(model.joints.size());
        for(const auto& joint : model.joints) {
            names.push_back(joint.name);
        }
        return names;
    }

    std::string jointTypeName(robot::JointType type)
    {
        switch(type) {
        case robot::JointType::Revolute:
            return "revolute";
        case robot::JointType::Prismatic:
            return "prismatic";
        default:
            return "fixed";
        }
    }

    std::vector<ProjectScene::RobotJointInfo> movableJointInfos(
        const robot::RobotModel& model)
    {
        std::vector<ProjectScene::RobotJointInfo> result;
        for(const robot::RobotJoint& joint : model.joints) {
            if(joint.dofIndex < 0) {
                continue;
            }
            result.push_back(ProjectScene::RobotJointInfo{ joint.name, jointTypeName(joint.type) });
        }
        return result;
    }

    std::vector<ProjectScene::RobotJointInfo> parallelControlJointInfos()
    {
        return {
            { "parallel.pose.x", "prismatic" },
            { "parallel.pose.y", "prismatic" },
            { "parallel.pose.z", "prismatic" },
            { "parallel.pose.roll", "revolute" },
            { "parallel.pose.pitch", "revolute" },
            { "parallel.pose.yaw", "revolute" },
            { "parallel.actuator.1", "prismatic" },
            { "parallel.actuator.2", "prismatic" },
            { "parallel.actuator.3", "prismatic" },
            { "parallel.actuator.4", "prismatic" },
            { "parallel.actuator.5", "prismatic" },
            { "parallel.actuator.6", "prismatic" }
        };
    }

    std::vector<std::string> parallelControlJointNames()
    {
        std::vector<std::string> names;
        for(const ProjectScene::RobotJointInfo& info : parallelControlJointInfos()) {
            names.push_back(info.jointName);
        }
        return names;
    }
}

std::vector<RuntimeRobot>& ProjectSceneDocumentProjectionSystem::robots()
{
    return m_robots;
}

const std::vector<RuntimeRobot>& ProjectSceneDocumentProjectionSystem::robots() const
{
    return m_robots;
}

std::vector<RuntimeSceneObject>& ProjectSceneDocumentProjectionSystem::objects()
{
    return m_objects;
}

const std::vector<RuntimeSceneObject>& ProjectSceneDocumentProjectionSystem::objects() const
{
    return m_objects;
}

void ProjectSceneDocumentProjectionSystem::reset()
{
    m_robots.clear();
    m_objects.clear();
    m_robotCatalog.clear();
    m_sceneObjectCatalog.clear();
    m_pointCloudCatalog.clear();
}

void ProjectSceneDocumentProjectionSystem::rebuildRobotCatalog()
{
    m_robotCatalog.clear();
    for(const RuntimeRobot& runtime : m_robots) {
        if(runtime.parallelFollowerEnabled) {
            continue;
        }
        m_robotCatalog.push_back(ProjectScene::RobotLinkGroup{
            runtime.documentId,
            runtime.name,
            runtime.model.linkNames,
            runtime.parallelControlEnabled
                ? parallelControlJointNames()
                : jointNames(runtime.model),
            runtime.parallelControlEnabled
                ? parallelControlJointInfos()
                : movableJointInfos(runtime.model) });
    }
}

void ProjectSceneDocumentProjectionSystem::appendSceneObjectCatalog(
    const RuntimeSceneObject& object)
{
    m_sceneObjectCatalog.push_back(ProjectScene::SceneObjectGroup{
        object.documentId,
        object.name });
}

void ProjectSceneDocumentProjectionSystem::appendPointCloudCatalog(
    const RuntimeSceneObject& pointCloud)
{
    m_pointCloudCatalog.push_back(ProjectScene::PointCloudGroup{
        pointCloud.documentId,
        pointCloud.name });
}

void ProjectSceneDocumentProjectionSystem::removeObjectCatalogEntry(
    const std::string& objectId)
{
    m_sceneObjectCatalog.erase(
        std::remove_if(
            m_sceneObjectCatalog.begin(),
            m_sceneObjectCatalog.end(),
            [&](const ProjectScene::SceneObjectGroup& item) {
                return item.objectId == objectId;
            }),
        m_sceneObjectCatalog.end());
    m_pointCloudCatalog.erase(
        std::remove_if(
            m_pointCloudCatalog.begin(),
            m_pointCloudCatalog.end(),
            [&](const ProjectScene::PointCloudGroup& item) {
                return item.pointCloudId == objectId;
            }),
        m_pointCloudCatalog.end());
}

const std::vector<ProjectScene::RobotLinkGroup>&
ProjectSceneDocumentProjectionSystem::robotCatalog() const
{
    return m_robotCatalog;
}

const std::vector<ProjectScene::SceneObjectGroup>&
ProjectSceneDocumentProjectionSystem::sceneObjectCatalog() const
{
    return m_sceneObjectCatalog;
}

const std::vector<ProjectScene::PointCloudGroup>&
ProjectSceneDocumentProjectionSystem::pointCloudCatalog() const
{
    return m_pointCloudCatalog;
}
