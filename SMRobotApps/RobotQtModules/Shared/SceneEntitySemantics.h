#pragma once
#include <SimulationProject/ProjectDocument.h>
#include <string>

namespace robot_qt_viewer {
std::string attachmentRole(const simulation_project::AttachmentAssetDesc& asset);
std::string sceneObjectRole(const simulation_project::ProjectDocument& document,
    const simulation_project::SceneObjectDesc& object);
const simulation_project::MountedAttachmentDesc* objectAttachment(
    const simulation_project::ProjectDocument& document, const std::string& objectId);
}
