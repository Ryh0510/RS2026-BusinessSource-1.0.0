#include "ProjectSceneInteractionSystem.h"

void ProjectSceneInteractionSystem::setMode(ProjectSceneInteractionMode mode)
{
    m_mode = mode;
}

ProjectSceneInteractionMode ProjectSceneInteractionSystem::mode() const
{
    return m_mode;
}

void ProjectSceneInteractionSystem::clearFrameSelection()
{
    m_selectedJointFrameRobotId.clear();
    m_selectedJointFrameName.clear();
    m_selectedObjectFrameObjectId.clear();
    m_selectedObjectFrameId.clear();
}

void ProjectSceneInteractionSystem::selectRobotLink(
    const std::string& robotId,
    const std::string& linkName)
{
    clearFrameSelection();
    m_selection.selectRobotLink(robotId, linkName);
}

void ProjectSceneInteractionSystem::selectJointFrame(
    const std::string& robotId,
    const std::string& jointName)
{
    clearFrameSelection();
    m_selection.clear();
    m_selectedJointFrameRobotId = robotId;
    m_selectedJointFrameName = jointName;
}

void ProjectSceneInteractionSystem::selectRobotMount(
    const std::string& robotId,
    const std::string& linkName,
    const std::string& robotMountId)
{
    clearFrameSelection();
    m_selection.selectRobotMount(robotId, linkName, robotMountId);
}

void ProjectSceneInteractionSystem::selectSceneObject(const std::string& objectId)
{
    clearFrameSelection();
    m_selection.selectSceneObject(objectId);
}

bool ProjectSceneInteractionSystem::selectObjectFrame(
    const std::string& objectId,
    const std::string& frameId)
{
    selectSceneObject(objectId);
    if(objectId.empty() || frameId.empty()) {
        return false;
    }
    m_selectedObjectFrameObjectId = objectId;
    m_selectedObjectFrameId = frameId;
    return true;
}

void ProjectSceneInteractionSystem::selectMountedAttachment(
    const std::string& attachmentId,
    const simulation_runtime::RuntimeAttachmentOwner& owner)
{
    clearFrameSelection();
    m_selection.selectMountedAttachment(attachmentId, owner);
}

const simulation_runtime::ProjectSelectionState& ProjectSceneInteractionSystem::selection() const
{
    return m_selection;
}

simulation_runtime::ProjectSelectionState& ProjectSceneInteractionSystem::selection()
{
    return m_selection;
}

const std::string& ProjectSceneInteractionSystem::selectedJointFrameRobotId() const
{
    return m_selectedJointFrameRobotId;
}

const std::string& ProjectSceneInteractionSystem::selectedJointFrameName() const
{
    return m_selectedJointFrameName;
}

const std::string& ProjectSceneInteractionSystem::selectedObjectFrameObjectId() const
{
    return m_selectedObjectFrameObjectId;
}

const std::string& ProjectSceneInteractionSystem::selectedObjectFrameId() const
{
    return m_selectedObjectFrameId;
}

void ProjectSceneInteractionSystem::setActivePreviewRobotMountId(const std::string& robotMountId)
{
    m_activePreviewRobotMountId = robotMountId;
}

const std::string& ProjectSceneInteractionSystem::activePreviewRobotMountId() const
{
    return m_activePreviewRobotMountId;
}

void ProjectSceneInteractionSystem::clearActivePreviewRobotMountId(const std::string& robotMountId)
{
    if(m_activePreviewRobotMountId == robotMountId) {
        m_activePreviewRobotMountId.clear();
    }
}

void ProjectSceneInteractionSystem::setRobotMountFrameVisibility(
    bool selectedLinkFrameVisible,
    bool mountFrameVisible)
{
    m_selectedLinkFrameVisible = selectedLinkFrameVisible;
    m_robotMountFrameVisible = mountFrameVisible;
}

bool ProjectSceneInteractionSystem::selectedLinkFrameVisible() const
{
    return m_selectedLinkFrameVisible;
}

bool ProjectSceneInteractionSystem::robotMountFrameVisible() const
{
    return m_robotMountFrameVisible;
}

void ProjectSceneInteractionSystem::setPinnedRobotMountFrames(
    const std::vector<std::string>& robotMountIds)
{
    m_pinnedRobotMountFrameIds = robotMountIds;
}

const std::vector<std::string>& ProjectSceneInteractionSystem::pinnedRobotMountFrames() const
{
    return m_pinnedRobotMountFrameIds;
}

std::vector<MaterialOverride>& ProjectSceneInteractionSystem::selectionOverrides()
{
    return m_selectionOverrides;
}

std::vector<MaterialOverride>& ProjectSceneInteractionSystem::pairPreviewOverrides()
{
    return m_pairPreviewOverrides;
}
