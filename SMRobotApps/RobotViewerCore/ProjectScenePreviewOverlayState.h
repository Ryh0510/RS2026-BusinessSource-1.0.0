#pragma once

#include <SimulationProject/ProjectDocument.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

class ProjectScenePreviewOverlayState
{
public:
    void clear();
    bool empty() const;

    void setRobotMount(const simulation_project::RobotMountDesc& mount);
    void removeRobotMount(const std::string& mountId);
    void setAttachmentTransform(
        const std::string& attachmentId,
        const simulation_project::TransformDesc& transform);
    void setAttachmentAsset(const simulation_project::AttachmentAssetDesc& asset);
    void setObjectFrame(
        const std::string& objectId,
        const simulation_project::ObjectFrameDesc& frame);

    simulation_project::ProjectDocument apply(
        const simulation_project::ProjectDocument& source) const;

private:
    std::unordered_map<std::string, simulation_project::RobotMountDesc> m_robotMounts;
    std::unordered_set<std::string> m_removedRobotMounts;
    std::unordered_map<std::string, simulation_project::TransformDesc> m_attachmentTransforms;
    std::unordered_map<std::string, simulation_project::AttachmentAssetDesc> m_attachmentAssets;
    std::unordered_map<std::string,
        std::unordered_map<std::string, simulation_project::ObjectFrameDesc>> m_objectFrames;
};
