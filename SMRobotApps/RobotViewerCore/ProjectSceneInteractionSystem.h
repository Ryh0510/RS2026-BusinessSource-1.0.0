#pragma once

#include "ProjectRuntimeTypes.h"
#include "ProjectScene.h"

#include <SimulationRuntime/ProjectSelectionState.h>

#include <string>
#include <vector>

class ProjectSceneInteractionSystem
{
public:
    void setMode(ProjectSceneInteractionMode mode);
    ProjectSceneInteractionMode mode() const;

    void selectRobotLink(const std::string& robotId, const std::string& linkName);
    void selectJointFrame(const std::string& robotId, const std::string& jointName);
    void selectRobotMount(
        const std::string& robotId,
        const std::string& linkName,
        const std::string& robotMountId);
    void selectSceneObject(const std::string& objectId);
    bool selectObjectFrame(const std::string& objectId, const std::string& frameId);
    void selectMountedAttachment(
        const std::string& attachmentId,
        const simulation_runtime::RuntimeAttachmentOwner& owner);

    const simulation_runtime::ProjectSelectionState& selection() const;
    simulation_runtime::ProjectSelectionState& selection();

    const std::string& selectedJointFrameRobotId() const;
    const std::string& selectedJointFrameName() const;
    const std::string& selectedObjectFrameObjectId() const;
    const std::string& selectedObjectFrameId() const;

    void setActivePreviewRobotMountId(const std::string& robotMountId);
    const std::string& activePreviewRobotMountId() const;
    void clearActivePreviewRobotMountId(const std::string& robotMountId);

    void setRobotMountFrameVisibility(bool selectedLinkFrameVisible, bool mountFrameVisible);
    bool selectedLinkFrameVisible() const;
    bool robotMountFrameVisible() const;
    void setPinnedRobotMountFrames(const std::vector<std::string>& robotMountIds);
    const std::vector<std::string>& pinnedRobotMountFrames() const;

    std::vector<MaterialOverride>& selectionOverrides();
    std::vector<MaterialOverride>& pairPreviewOverrides();

private:
    void clearFrameSelection();

    simulation_runtime::ProjectSelectionState m_selection;
    ProjectSceneInteractionMode m_mode = ProjectSceneInteractionMode::Browse;
    std::string m_activePreviewRobotMountId;
    std::vector<MaterialOverride> m_selectionOverrides;
    std::vector<MaterialOverride> m_pairPreviewOverrides;
    std::string m_selectedJointFrameRobotId;
    std::string m_selectedJointFrameName;
    std::string m_selectedObjectFrameObjectId;
    std::string m_selectedObjectFrameId;
    bool m_selectedLinkFrameVisible = false;
    bool m_robotMountFrameVisible = false;
    std::vector<std::string> m_pinnedRobotMountFrameIds;
};
