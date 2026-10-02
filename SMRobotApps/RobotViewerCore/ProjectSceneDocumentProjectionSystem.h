#pragma once

#include "ProjectRuntimeTypes.h"
#include "ProjectScene.h"

#include <vector>

class ProjectSceneDocumentProjectionSystem
{
public:
    std::vector<RuntimeRobot>& robots();
    const std::vector<RuntimeRobot>& robots() const;
    std::vector<RuntimeSceneObject>& objects();
    const std::vector<RuntimeSceneObject>& objects() const;

    void reset();
    void rebuildRobotCatalog();
    void appendSceneObjectCatalog(const RuntimeSceneObject& object);
    void appendPointCloudCatalog(const RuntimeSceneObject& pointCloud);
    void removeObjectCatalogEntry(const std::string& objectId);

    const std::vector<ProjectScene::RobotLinkGroup>& robotCatalog() const;
    const std::vector<ProjectScene::SceneObjectGroup>& sceneObjectCatalog() const;
    const std::vector<ProjectScene::PointCloudGroup>& pointCloudCatalog() const;

private:
    std::vector<RuntimeRobot> m_robots;
    std::vector<RuntimeSceneObject> m_objects;
    std::vector<ProjectScene::RobotLinkGroup> m_robotCatalog;
    std::vector<ProjectScene::SceneObjectGroup> m_sceneObjectCatalog;
    std::vector<ProjectScene::PointCloudGroup> m_pointCloudCatalog;
};
