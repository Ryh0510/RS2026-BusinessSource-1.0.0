#include "ProjectScenePreviewOverlayState.h"

#include <algorithm>

void ProjectScenePreviewOverlayState::clear()
{
    m_robotMounts.clear();
    m_removedRobotMounts.clear();
    m_attachmentTransforms.clear();
    m_attachmentAssets.clear();
    m_objectFrames.clear();
}

bool ProjectScenePreviewOverlayState::empty() const
{
    return m_robotMounts.empty() &&
        m_removedRobotMounts.empty() &&
        m_attachmentTransforms.empty() &&
        m_attachmentAssets.empty() &&
        m_objectFrames.empty();
}

void ProjectScenePreviewOverlayState::setRobotMount(
    const simulation_project::RobotMountDesc& mount)
{
    m_removedRobotMounts.erase(mount.id);
    m_robotMounts[mount.id] = mount;
}

void ProjectScenePreviewOverlayState::removeRobotMount(const std::string& mountId)
{
    m_robotMounts.erase(mountId);
    m_removedRobotMounts.insert(mountId);
}

void ProjectScenePreviewOverlayState::setAttachmentTransform(
    const std::string& attachmentId,
    const simulation_project::TransformDesc& transform)
{
    m_attachmentTransforms[attachmentId] = transform;
}

void ProjectScenePreviewOverlayState::setAttachmentAsset(
    const simulation_project::AttachmentAssetDesc& asset)
{
    m_attachmentAssets[asset.id] = asset;
}

void ProjectScenePreviewOverlayState::setObjectFrame(
    const std::string& objectId,
    const simulation_project::ObjectFrameDesc& frame)
{
    m_objectFrames[objectId][frame.id] = frame;
}

simulation_project::ProjectDocument ProjectScenePreviewOverlayState::apply(
    const simulation_project::ProjectDocument& source) const
{
    simulation_project::ProjectDocument result = source;

    result.robotMounts.erase(
        std::remove_if(
            result.robotMounts.begin(),
            result.robotMounts.end(),
            [&](const simulation_project::RobotMountDesc& mount) {
                return m_removedRobotMounts.count(mount.id) > 0;
            }),
        result.robotMounts.end());
    for(const auto& [id, overrideMount] : m_robotMounts) {
        const auto it = std::find_if(
            result.robotMounts.begin(),
            result.robotMounts.end(),
            [&](const simulation_project::RobotMountDesc& mount) {
                return mount.id == id;
            });
        if(it == result.robotMounts.end()) {
            result.robotMounts.push_back(overrideMount);
        } else {
            *it = overrideMount;
        }
    }

    for(simulation_project::MountedAttachmentDesc& attachment : result.mountedAttachments) {
        const auto it = m_attachmentTransforms.find(attachment.id);
        if(it != m_attachmentTransforms.end()) {
            attachment.mountToAssetMount = it->second;
        }
    }
    for(simulation_project::AttachmentAssetDesc& asset : result.attachmentAssets) {
        const auto it = m_attachmentAssets.find(asset.id);
        if(it != m_attachmentAssets.end()) {
            asset = it->second;
        }
    }
    for(simulation_project::SceneObjectDesc& object : result.objects) {
        const auto objectIt = m_objectFrames.find(object.id);
        if(objectIt == m_objectFrames.end()) {
            continue;
        }
        for(const auto& [frameId, frame] : objectIt->second) {
            const auto frameIt = std::find_if(
                object.objectFrames.begin(),
                object.objectFrames.end(),
                [&](const simulation_project::ObjectFrameDesc& existing) {
                    return existing.id == frameId;
                });
            if(frameIt == object.objectFrames.end()) {
                object.objectFrames.push_back(frame);
            } else {
                *frameIt = frame;
            }
        }
    }
    return result;
}
