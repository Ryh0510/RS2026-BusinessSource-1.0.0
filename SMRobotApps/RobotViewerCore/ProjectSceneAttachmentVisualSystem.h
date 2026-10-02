#pragma once

#include "ProjectScene.h"

#include <Collision/CollisionTypes.h>
#include <RobotRenderBridge/MountedAttachmentVisualBridge.h>
#include <SceneCore/ModelNode.h>
#include <SceneCore/SceneNode.h>
#include <SimulationRuntime/RuntimeMountedAttachment.h>

#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

struct RuntimeToolAttachmentVisual
{
    std::string documentId;
    std::string name;
    std::string robotId;
    std::string linkName;
    std::string robotMountId;
    std::string toolAssetId;
    std::string assetKind;
    std::string assetType;
    std::string functionalFrameType;
    std::filesystem::path visualPath;
    double visualScale = 1.0;
    collision::Transform3 linkToMount = collision::Transform3::Identity();
    collision::Transform3 mountToAssetMount = collision::Transform3::Identity();
    collision::Transform3 assetMountToVisual = collision::Transform3::Identity();
    collision::Transform3 assetMountToTcp = collision::Transform3::Identity();
    collision::Transform3 worldLink = collision::Transform3::Identity();
    collision::Transform3 worldRobotMount = collision::Transform3::Identity();
    collision::Transform3 worldToolMount = collision::Transform3::Identity();
    collision::Transform3 worldVisual = collision::Transform3::Identity();
    collision::Transform3 worldTcp = collision::Transform3::Identity();
    bool visible = true;
    bool enabled = true;
    bool hasSensorIntrinsics = false;
    simulation_project::SensorIntrinsicsDesc sensorIntrinsics;
    robot_render::MountedAttachmentVisual visual;
    std::size_t collisionObjectIndex = std::numeric_limits<std::size_t>::max();
};

struct RuntimeToolAssetPreview
{
    simulation_project::AttachmentAssetDesc asset;
    std::filesystem::path basePath;
    std::filesystem::path visualPath;
    collision::Transform3 mountToVisual = collision::Transform3::Identity();
    collision::Transform3 mountToTcp = collision::Transform3::Identity();
    std::shared_ptr<scenecore::SceneNode> rootNode;
    std::shared_ptr<scenecore::ModelNode> modelNode;
    glm::mat4 modelBaseLocal = glm::mat4(1.0f);
    glm::mat4 modelLocal = glm::mat4(1.0f);
};

class ProjectSceneAttachmentVisualSystem
{
public:
    void useMountedGraph(const simulation_runtime::RuntimeMountedAttachmentGraph& graph);
    void usePreviewGraph();
    simulation_runtime::RuntimeMountedAttachmentGraph& previewGraph();
    const simulation_runtime::RuntimeMountedAttachmentGraph& mountedGraph() const;
    std::vector<RuntimeToolAttachmentVisual>& visuals();
    const std::vector<RuntimeToolAttachmentVisual>& visuals() const;

    void resetVisuals();
    void selectInitialActive(std::size_t firstEnabled, std::size_t firstVisible);
    bool cycleActive(bool reverse);
    bool setActive(const std::string& id);
    const RuntimeToolAttachmentVisual* activeToolFrameAttachment() const;
    std::size_t activeIndex() const;

    void setActiveToolFrameRobot(const std::string& robotId);
    const std::string& activeToolFrameRobot() const;
    void setToolFrameVisibility(const ProjectScene::ToolFrameVisibility& visibility);
    const ProjectScene::ToolFrameVisibility& toolFrameVisibility() const;
    void applyVisibility(bool sceneFocusActive);

    void setPreview(
        const simulation_project::AttachmentAssetDesc& asset,
        const std::filesystem::path& basePath);
    void clearPreview();
    bool hasPreview() const;
    RuntimeToolAssetPreview& preview();
    const RuntimeToolAssetPreview& preview() const;

private:
    simulation_runtime::RuntimeMountedAttachmentGraph m_previewGraph;
    const simulation_runtime::RuntimeMountedAttachmentGraph* m_mountedGraph = nullptr;
    std::vector<RuntimeToolAttachmentVisual> m_visuals;
    bool m_hasPreview = false;
    RuntimeToolAssetPreview m_preview;
    std::size_t m_activeIndex = static_cast<std::size_t>(-1);
    std::string m_activeToolFrameRobotId;
    ProjectScene::ToolFrameVisibility m_toolFrameVisibility;
};
