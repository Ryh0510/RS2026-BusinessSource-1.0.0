#include "SceneEntitySemantics.h"
namespace robot_qt_viewer {
std::string attachmentRole(const simulation_project::AttachmentAssetDesc& asset)
{
    // Old bindings used assetKind=tool but copied assetType=workpiece from the STL.
    // The attachment kind describes ownership; the stale subtype must not win.
    if(asset.assetKind == "workpiece") return "workpiece";
    if(asset.assetKind == "tool") return "tool";
    return asset.assetKind.empty() ? "tool" : asset.assetKind;
}
const simulation_project::MountedAttachmentDesc* objectAttachment(
    const simulation_project::ProjectDocument& document, const std::string& objectId)
{
    for(const auto& attachment : document.mountedAttachments)
        if(attachment.sourceObjectId == objectId) return &attachment;
    return nullptr;
}
std::string sceneObjectRole(const simulation_project::ProjectDocument& document,
    const simulation_project::SceneObjectDesc& object)
{
    if(const auto* attachment = objectAttachment(document, object.id))
        for(const auto& asset : document.attachmentAssets)
            if(asset.id == attachment->assetId) return attachmentRole(asset);
    return object.objectType.empty() ? "workpiece" : object.objectType;
}
}
