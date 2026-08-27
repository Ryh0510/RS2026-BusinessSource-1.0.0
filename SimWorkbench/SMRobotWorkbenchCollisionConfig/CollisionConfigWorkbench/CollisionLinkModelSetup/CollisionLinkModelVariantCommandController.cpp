#include "CollisionLinkModelVariantCommandController.h"

#include "CollisionLinkModelDocumentFacade.h"
#include "RobotQtViewerViewportServices.h"

#include <SimulationProject/CollisionModelSelectionIds.h>
#include <SimulationProject/ProjectDocumentService.h>

#include <algorithm>

namespace
{
    CollisionLinkModelVariantCommandResult makeFailure(const QString& message)
    {
        CollisionLinkModelVariantCommandResult result;
        result.success = false;
        result.message = message;
        return result;
    }
}

CollisionLinkModelVariantCommandResult CollisionLinkModelVariantCommandController::showVariantOnly(
    robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
    const QString& robotId,
    const QString& linkName,
    const QString& variantId)
{
    if(viewportServices == nullptr) {
        return makeFailure("Viewport is not available.");
    }
    if(robotId.isEmpty() || linkName.isEmpty() || variantId.isEmpty()) {
        return makeFailure("Select a collision model variant first.");
    }
    if(!viewportServices->setVisibleRobotCollisionVariant(robotId, linkName, variantId)) {
        return makeFailure("Failed to show selected collision model variant.");
    }

    CollisionLinkModelVariantCommandResult result;
    result.success = true;
    result.message = "Showing only the selected collision model variant.";
    return result;
}

CollisionLinkModelVariantCommandResult CollisionLinkModelVariantCommandController::useVariantInDetector(
    simulation_project::ProjectDocumentService& service,
    robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
    const QString& detectorId,
    const QString& robotId,
    const QString& linkName,
    const QString& objectId,
    const QString& variantId)
{
    if(detectorId.isEmpty()) {
        return makeFailure("Select an active collision detector first.");
    }
    if(variantId.isEmpty()) {
        return makeFailure("Selected collision model variant has no stable modelId.");
    }
    if((robotId.isEmpty() || linkName.isEmpty()) && objectId.isEmpty()) {
        return makeFailure("Select a concrete robot link, object, or attachment target first.");
    }

    simulation_project::CollisionDetectorModelBindingDesc binding;
    binding.context = "AnyEndpoint";
    binding.mode = "ExplicitModel";
    binding.modelId = variantId.toStdString();
    if(!robotId.isEmpty() && !linkName.isEmpty()) {
        binding.robotId = robotId.toStdString();
        binding.linkName = linkName.toStdString();
    } else {
        const std::string targetId = objectId.toStdString();
        const simulation_project::ProjectDocument& document = service.document();
        const bool attachment = std::any_of(
            document.mountedAttachments.begin(), document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& item) { return item.id == targetId; });
        const bool pointCloud = std::any_of(
            document.pointClouds.begin(), document.pointClouds.end(),
            [&](const simulation_project::PointCloudDesc& item) { return item.id == targetId; });
        if(attachment) {
            binding.attachmentId = targetId;
        } else if(pointCloud) {
            binding.pointCloudId = targetId;
        } else {
            binding.objectId = targetId;
        }
    }

    bool changed = false;
    std::string error;
    if(!service.setCollisionDetectorModelBinding(
           detectorId.toStdString(), binding, &changed, &error)) {
        return makeFailure(error.empty()
            ? QString("Failed to bind collision model to detector target.")
            : QString::fromStdString(error));
    }

    bool updatedRuntime = viewportServices == nullptr;
    if(viewportServices != nullptr) {
        updatedRuntime = viewportServices->rebuildCollisionDetectorsFromDocument(service.document());
        if(updatedRuntime) {
            viewportServices->setActiveCollisionDetector(detectorId);
        }
    }

    CollisionLinkModelVariantCommandResult result;
    result.success = true;
    result.projectChanged = changed;
    result.runtimeUpdateFailed = viewportServices != nullptr && !updatedRuntime;
    result.message = QString("Bound detector target to modelId: %1").arg(variantId);
    return result;
}

CollisionLinkModelVariantCommandResult CollisionLinkModelVariantCommandController::setCurrentLinkModel(
    simulation_project::ProjectDocumentService& service,
    const QString& robotId,
    const QString& linkName,
    const QString& variantId)
{
    if(robotId.isEmpty() || linkName.isEmpty()) {
        return makeFailure("Select a robot link first.");
    }
    if(variantId.isEmpty()) {
        return makeFailure("Select a collision model variant first.");
    }

    bool changed = false;
    std::string error;
    if(!service.setActiveRobotLinkCollisionModel(
           robotId.toStdString(),
           linkName.toStdString(),
           variantId.toStdString(),
           &changed,
           &error)) {
        return makeFailure(error.empty()
            ? QString("Failed to set current collision model.")
            : QString::fromStdString(error));
    }

    CollisionLinkModelVariantCommandResult result;
    result.success = true;
    result.projectChanged = changed;
    result.message = changed
        ? QString("Current link collision model: %1").arg(variantId)
        : QString("Selected collision model is already current.");
    return result;
}

CollisionLinkModelVariantCommandResult CollisionLinkModelVariantCommandController::setCurrentObjectModel(
    simulation_project::ProjectDocumentService& service,
    const QString& objectId,
    const QString& variantId)
{
    if(objectId.isEmpty()) {
        return makeFailure("Select an object or attachment first.");
    }
    if(variantId.isEmpty()) {
        return makeFailure("Select a collision model variant first.");
    }

    bool changed = false;
    std::string error;
    if(!service.setActiveObjectCollisionModel(
           objectId.toStdString(),
           variantId.toStdString(),
           &changed,
           &error)) {
        return makeFailure(error.empty()
            ? QString("Failed to set current object collision model.")
            : QString::fromStdString(error));
    }

    CollisionLinkModelVariantCommandResult result;
    result.success = true;
    result.projectChanged = changed;
    result.message = changed
        ? QString("Current object collision model: %1").arg(variantId)
        : QString("Selected collision model is already current.");
    return result;
}
