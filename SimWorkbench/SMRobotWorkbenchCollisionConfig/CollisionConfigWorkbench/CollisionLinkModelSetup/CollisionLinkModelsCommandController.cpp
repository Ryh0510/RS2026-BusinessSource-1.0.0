#include "CollisionLinkModelsCommandController.h"

#include "CollisionWorkbenchServices.h"
#include "CollisionLinkModelDocumentFacade.h"
#include "RobotQtViewerViewportServices.h"

#include <SimulationProject/CollisionModelSelectionIds.h>
#include <SimulationProject/ProjectAssetStore.h>
#include <SimulationProject/ProjectDocumentService.h>

#include <algorithm>

namespace
{
    CollisionLinkModelsCommandResult makeFailure(const QString& message)
    {
        CollisionLinkModelsCommandResult result;
        result.success = false;
        result.message = message;
        return result;
    }

    std::string normalizedCollisionRole(const std::string& role)
    {
        return role.empty() ? std::string("Exact") : role;
    }

    std::string normalizedCollisionSource(const std::string& source)
    {
        return source.empty() ? std::string("original") : source;
    }

    QString collisionProxyQualityText(const robot_qt_viewer::CollisionRuntimeProxyQualitySummary& summary)
    {
        QString text = QString(
            "sphere cover quality: generated %1/%2, input points %3, uncovered %4, max outside %5, max radius %6, axis %7, radius/axis %8")
            .arg(summary.generatedSphereCount)
            .arg(summary.requestedMaxSpheres)
            .arg(static_cast<qulonglong>(summary.inputPointCount))
            .arg(static_cast<qulonglong>(summary.uncoveredPointCount))
            .arg(summary.maxOutsideDistance, 0, 'f', 6)
            .arg(summary.maxSphereRadius, 0, 'f', 6)
            .arg(summary.mainAxisLength, 0, 'f', 6)
            .arg(summary.maxRadiusToLinkLength, 0, 'f', 6);
        if(summary.estimatedCrossSectionRadius > 0.0) {
            text += QString(", cross radius %1, radius/cross %2")
                .arg(summary.estimatedCrossSectionRadius, 0, 'f', 6)
                .arg(summary.maxRadiusToCrossSectionRadius, 0, 'f', 6);
        }
        if(summary.oversizedSphereCount > 0) {
            text += QString(", oversized %1").arg(summary.oversizedSphereCount);
        }
        if(!summary.recommendedShape.empty()) {
            text += QString(", recommended: %1").arg(QString::fromStdString(summary.recommendedShape));
        }
        if(summary.hasWarning && !summary.warning.empty()) {
            text += QString("  %1").arg(QString::fromStdString(summary.warning));
        }
        return text;
    }

    void populateGeneratedVariantResult(
        CollisionLinkModelsCommandResult& result,
        const std::vector<simulation_project::CollisionElementOverrideDesc>& elements)
    {
        result.generatedCount = static_cast<int>(elements.size());
        if(elements.empty()) {
            return;
        }

        result.selectedSource =
            QString::fromStdString(normalizedCollisionSource(elements.front().source));
        result.selectedRole =
            QString::fromStdString(normalizedCollisionRole(elements.front().role));
    }

    void populateGeneratedObjectVariantResult(
        CollisionLinkModelsCommandResult& result,
        const std::vector<simulation_project::ObjectCollisionElementOverrideDesc>& elements)
    {
        result.generatedCount = static_cast<int>(elements.size());
        if(elements.empty()) {
            return;
        }

        result.selectedSource =
            QString::fromStdString(normalizedCollisionSource(elements.front().source));
        result.selectedRole =
            QString::fromStdString(normalizedCollisionRole(elements.front().role));
    }

    bool describeGeneratedProjectAsset(
        const simulation_project::ProjectDocument& document,
        robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
        const std::string& meshUri,
        simulation_project::ProjectAssetDesc& asset,
        QString& errorMessage)
    {
        if(meshUri.rfind("project://assets/", 0) != 0) {
            errorMessage = "Save the project before applying a generated COACD model.";
            return false;
        }
        if(viewportServices == nullptr || viewportServices->projectBasePath().empty()
            || document.assetStore.directory.empty()) {
            errorMessage = "The project asset store is unavailable. Save and reload the project first.";
            return false;
        }

        std::filesystem::path relative;
        std::string error;
        if(!simulation_project::ProjectAssetStore::relativePathFromUri(meshUri, relative, &error)) {
            errorMessage = QString::fromStdString(error);
            return false;
        }
        std::vector<std::string> parts;
        for(const auto& part : relative)
            parts.push_back(part.generic_u8string());
        if(parts.size() < 5 || parts[0] != "collision" || parts[1] != "coacd") {
            errorMessage = "Generated COACD URI does not follow the project asset layout.";
            return false;
        }

        const bool compactLayout = parts.size() == 5
            && parts[2].rfind("a-", 0) == 0
            && parts[3].rfind("r-", 0) == 0;
        const bool legacyLayout = parts.size() == 6
            && parts[4].rfind("input-", 0) == 0;
        if(!compactLayout && !legacyLayout) {
            errorMessage = "Generated COACD URI uses an unsupported project asset layout.";
            return false;
        }

        const std::filesystem::path revisionRelative = relative.parent_path();
        const std::filesystem::path manifestRelative = revisionRelative / "manifest.json";
        const std::string manifestUri =
            simulation_project::ProjectAssetStore::makeProjectAssetUri(manifestRelative);
        const std::filesystem::path projectMarker =
            viewportServices->projectBasePath() / "project.sys.json";
        const std::filesystem::path manifestPath =
            simulation_project::ProjectAssetStore::resolvePath(
                projectMarker,
                document.assetStore,
                manifestUri,
                &error);
        if(manifestPath.empty() || !std::filesystem::exists(manifestPath)) {
            errorMessage = error.empty()
                ? QString("Generated COACD manifest is missing.")
                : QString::fromStdString(error);
            return false;
        }

        asset.id = compactLayout
            ? "asset.collision." + parts[2] + ".coacd"
            : "asset.collision." + parts[2] + "." + parts[3] + ".coacd";
        asset.kind = "collision-derived";
        asset.uri = manifestUri;
        asset.format = "smrobot-collision-manifest-json";
        asset.revision = compactLayout ? parts[3] : parts[4];
        asset.contentHash = simulation_project::ProjectAssetStore::sha256Directory(
            manifestPath.parent_path(),
            &error);
        if(asset.contentHash.empty()) {
            errorMessage = QString::fromStdString(error);
            return false;
        }
        asset.storagePolicy = "ProjectOwned";
        if(asset.revision.rfind("r-", 0) == 0) {
            asset.sourceFingerprint = "fnv1a64:" + asset.revision.substr(2);
        } else if(asset.revision.rfind("input-", 0) == 0) {
            asset.sourceFingerprint = "fnv1a64:" + asset.revision.substr(6);
        } else {
            asset.sourceFingerprint = asset.revision;
        }
        asset.generator.id = "smrobot.coacd";
        asset.generator.version = compactLayout ? "2" : "1";
        asset.generator.serializedOptions = "{\"preset\":\"fastPreviewDefaults\"}";
        return true;
    }

    bool registerGeneratedRobotAsset(
        simulation_project::ProjectDocument& document,
        robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
        const std::string& robotId,
        const std::string& linkName,
        std::vector<simulation_project::CollisionElementOverrideDesc>& elements,
        QString& errorMessage)
    {
        if(elements.empty())
            return false;
        simulation_project::ProjectAssetDesc asset;
        if(!describeGeneratedProjectAsset(
               document, viewportServices, elements.front().meshPath, asset, errorMessage)) {
            return false;
        }

        std::vector<std::string> elementIds;
        elementIds.reserve(elements.size());
        for(const auto& element : elements)
            elementIds.push_back(element.id);

        simulation_project::CollisionModelRecordDesc model;
        model.modelId = simulation_project::makeGeneratedRobotLinkCollisionModelId(
            robotId,
            linkName,
            elements.front().source,
            elements.front().role,
            elementIds);
        model.target.kind = "robot-link";
        model.target.robotId = robotId;
        model.target.linkName = linkName;
        model.role = elements.front().role;
        model.source = elements.front().source;
        model.assetId = asset.id;
        model.assetRevision = asset.revision;
        simulation_project::ProjectAssetStore::upsertAssetRevision(document, asset);
        simulation_project::ProjectDocumentService service(document);
        service.replaceGeneratedRobotLinkCollisionVariant(model, elements);
        return true;
    }

    bool registerGeneratedObjectAsset(
        simulation_project::ProjectDocument& document,
        robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
        const std::string& objectId,
        std::vector<simulation_project::ObjectCollisionElementOverrideDesc>& elements,
        QString& errorMessage)
    {
        if(elements.empty())
            return false;
        simulation_project::ProjectAssetDesc asset;
        if(!describeGeneratedProjectAsset(
               document, viewportServices, elements.front().meshPath, asset, errorMessage)) {
            return false;
        }

        std::vector<std::string> elementIds;
        elementIds.reserve(elements.size());
        for(const auto& element : elements)
            elementIds.push_back(element.id);

        const bool attachment = std::any_of(
            document.mountedAttachments.begin(),
            document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& candidate) {
                return candidate.id == objectId;
            });
        simulation_project::CollisionModelRecordDesc model;
        model.modelId = simulation_project::makeGeneratedObjectCollisionModelId(
            objectId,
            elements.front().source,
            elements.front().role,
            elementIds);
        model.target.kind = attachment ? "attachment" : "object";
        if(attachment)
            model.target.attachmentId = objectId;
        else
            model.target.objectId = objectId;
        model.role = elements.front().role;
        model.source = elements.front().source;
        model.assetId = asset.id;
        model.assetRevision = asset.revision;
        simulation_project::ProjectAssetStore::upsertAssetRevision(document, asset);
        simulation_project::ProjectDocumentService service(document);
        service.replaceGeneratedObjectCollisionVariant(model, elements);
        return true;
    }

    void registerInlineRobotModels(
        simulation_project::ProjectDocument& document,
        const std::string& robotId,
        const std::vector<simulation_project::CollisionElementOverrideDesc>& elements)
    {
        std::vector<std::string> linkNames;
        for(const auto& element : elements) {
            if(!element.linkName.empty()
                && std::find(linkNames.begin(), linkNames.end(), element.linkName) == linkNames.end()) {
                linkNames.push_back(element.linkName);
            }
        }

        for(const std::string& linkName : linkNames) {
            std::vector<std::string> elementIds;
            const simulation_project::CollisionElementOverrideDesc* first = nullptr;
            for(const auto& element : elements) {
                if(element.linkName != linkName)
                    continue;
                if(first == nullptr)
                    first = &element;
                elementIds.push_back(element.id);
            }
            if(first == nullptr)
                continue;

            simulation_project::CollisionModelRecordDesc model;
            model.modelId = simulation_project::makeGeneratedRobotLinkCollisionModelId(
                robotId,
                linkName,
                first->source,
                first->role,
                elementIds);
            model.target.kind = "robot-link";
            model.target.robotId = robotId;
            model.target.linkName = linkName;
            model.role = first->role;
            model.source = first->source;
            simulation_project::ProjectAssetStore::upsertCollisionModel(document, model);
        }
    }
}

CollisionLinkModelsCommandResult CollisionLinkModelsCommandController::generateFromVisual(
    simulation_project::ProjectDocument& document,
    robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
    const QString& robotId,
    const QString& linkName,
    const robot_qt_viewer::CollisionRuntimeProxyRequest& request,
    bool replaceOriginal)
{
    if(robotId.isEmpty() || linkName.isEmpty()) {
        return makeFailure("Select a robot link first.");
    }
    if(viewportServices == nullptr) {
        return makeFailure("Viewport is not available.");
    }

    std::vector<simulation_project::CollisionElementOverrideDesc> elements;
    if(!viewportServices->generateRobotCollisionProxies(robotId, linkName, request, elements)) {
        return makeFailure(
            QString("Failed to generate %1 collision from visual mesh.")
                .arg(QString::fromStdString(request.proxyType)));
    }

    CollisionLinkModelDocumentFacade documentFacade(document);
    documentFacade.setReplaceOriginal(robotId.toStdString(), replaceOriginal);
    CollisionLinkModelsCommandResult result;
    result.success = true;
    result.projectChanged = true;
    populateGeneratedVariantResult(result, elements);

    robot_qt_viewer::CollisionRuntimeProxyQualitySummary qualitySummary;
    if(request.proxyType == "sphereCover" &&
        viewportServices->evaluateRobotCollisionProxyQuality(
            robotId,
            linkName,
            request,
            elements,
            false,
            qualitySummary)) {
        result.qualityMessage = collisionProxyQualityText(qualitySummary);
    }

    registerInlineRobotModels(document, robotId.toStdString(), elements);
    documentFacade.appendUniqueElements(robotId.toStdString(), elements);

    const QString qualitySuffix = result.qualityMessage.isEmpty()
        ? QString()
        : QString("  %1").arg(result.qualityMessage);
    result.message = QString("Generated %1 %2 collision element(s) from %3.%4%5")
        .arg(result.generatedCount)
        .arg(QString::fromStdString(request.proxyType), robotId, linkName, qualitySuffix);
    return result;
}

CollisionLinkModelsCommandResult CollisionLinkModelsCommandController::generateFromExistingCollision(
    simulation_project::ProjectDocument& document,
    robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
    const QString& robotId,
    const QString& linkName,
    const robot_qt_viewer::CollisionRuntimeProxyRequest& request,
    bool replaceOriginal)
{
    if(robotId.isEmpty() || linkName.isEmpty()) {
        return makeFailure("Select a robot link first.");
    }
    if(viewportServices == nullptr) {
        return makeFailure("Viewport is not available.");
    }

    std::vector<simulation_project::CollisionElementOverrideDesc> elements;
    if(!viewportServices->generateRobotCollisionProxiesFromExistingCollision(
           robotId,
           linkName,
           request,
           elements)) {
        return makeFailure(
            QString("Failed to generate %1 from existing collision.")
                .arg(QString::fromStdString(request.proxyType)));
    }

    CollisionLinkModelDocumentFacade documentFacade(document);
    documentFacade.setReplaceOriginal(robotId.toStdString(), replaceOriginal);

    CollisionLinkModelsCommandResult result;
    result.success = true;
    result.projectChanged = true;
    populateGeneratedVariantResult(result, elements);

    robot_qt_viewer::CollisionRuntimeProxyQualitySummary qualitySummary;
    if(request.proxyType == "sphereCover" &&
        viewportServices->evaluateRobotCollisionProxyQuality(
            robotId,
            linkName,
            request,
            elements,
            true,
            qualitySummary)) {
        result.qualityMessage = collisionProxyQualityText(qualitySummary);
    }

    registerInlineRobotModels(document, robotId.toStdString(), elements);
    documentFacade.appendUniqueElements(robotId.toStdString(), elements);

    const QString qualitySuffix = result.qualityMessage.isEmpty()
        ? QString()
        : QString("  %1").arg(result.qualityMessage);
    result.message = QString("Generated %1 %2 element(s) from existing collision%3")
        .arg(result.generatedCount)
        .arg(QString::fromStdString(request.proxyType), qualitySuffix);
    return result;
}

CollisionLinkModelsCommandResult CollisionLinkModelsCommandController::generateRobotFromExistingCollision(
    simulation_project::ProjectDocument& document,
    robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
    const QString& robotId,
    const robot_qt_viewer::CollisionRuntimeProxyRequest& request,
    bool replaceOriginal)
{
    if(robotId.isEmpty()) {
        return makeFailure("Select a robot first.");
    }
    if(viewportServices == nullptr) {
        return makeFailure("Viewport is not available.");
    }

    std::vector<simulation_project::CollisionElementOverrideDesc> elements;
    if(!viewportServices->generateRobotCollisionProxiesFromExistingCollision(robotId, request, elements)) {
        return makeFailure(
            QString("Failed to generate robot %1 from existing collision.")
                .arg(QString::fromStdString(request.proxyType)));
    }

    CollisionLinkModelDocumentFacade documentFacade(document);
    documentFacade.setReplaceOriginal(robotId.toStdString(), replaceOriginal);

    CollisionLinkModelsCommandResult result;
    result.success = true;
    result.projectChanged = true;
    populateGeneratedVariantResult(result, elements);
    registerInlineRobotModels(document, robotId.toStdString(), elements);
    documentFacade.appendUniqueElements(robotId.toStdString(), elements);

    result.message = QString("Generated %1 robot %2 element(s) from existing collision")
        .arg(result.generatedCount)
        .arg(QString::fromStdString(request.proxyType));
    return result;
}

CollisionLinkModelsCommandResult CollisionLinkModelsCommandController::generateMissingFromVisual(
    simulation_project::ProjectDocument& document,
    robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
    const QString& robotId,
    const robot_qt_viewer::CollisionRuntimeProxyRequest& request,
    bool replaceOriginal)
{
    if(robotId.isEmpty()) {
        return makeFailure("Select a robot first.");
    }
    if(viewportServices == nullptr) {
        return makeFailure("Viewport is not available.");
    }

    std::vector<simulation_project::CollisionElementOverrideDesc> elements;
    if(!viewportServices->generateMissingRobotCollisionProxies(robotId, request, elements)) {
        return makeFailure("No missing visual collision proxies could be generated.");
    }

    CollisionLinkModelDocumentFacade documentFacade(document);
    documentFacade.setReplaceOriginal(robotId.toStdString(), replaceOriginal);

    CollisionLinkModelsCommandResult result;
    result.success = true;
    result.projectChanged = true;
    populateGeneratedVariantResult(result, elements);
    registerInlineRobotModels(document, robotId.toStdString(), elements);
    documentFacade.appendUniqueElements(robotId.toStdString(), elements);

    result.message =
        QString("Generated %1 missing collision proxies for %2").arg(result.generatedCount).arg(robotId);
    return result;
}

CollisionLinkModelsCommandResult CollisionLinkModelsCommandController::generateCoacdForLink(
    simulation_project::ProjectDocument& document,
    robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
    const QString& robotId,
    const QString& linkName)
{
    if(robotId.isEmpty() || linkName.isEmpty()) {
        return makeFailure("Select a robot link first.");
    }
    if(viewportServices == nullptr) {
        return makeFailure("Viewport is not available.");
    }

    std::vector<simulation_project::CollisionElementOverrideDesc> elements;
    QString inputLabel = "existing collision";
    if(!viewportServices->generateRobotCollisionCoacdFromExistingCollision(
           robotId,
           linkName,
           elements)) {
        inputLabel = "visual mesh";
        if(!viewportServices->generateRobotCollisionCoacdFromVisual(
               robotId,
               linkName,
               elements)) {
            return makeFailure("Failed to generate simplified collision model with COACD.");
        }
    }

    CollisionLinkModelsCommandResult result;
    result.success = true;
    result.projectChanged = true;
    populateGeneratedVariantResult(result, elements);

    QString registrationError;
    if(!registerGeneratedRobotAsset(
           document,
           viewportServices,
           robotId.toStdString(),
           linkName.toStdString(),
           elements,
           registrationError)) {
        return makeFailure(registrationError);
    }

    result.message = QString("Generated %1 COACD collision part(s) from %2.")
        .arg(result.generatedCount)
        .arg(inputLabel);
    return result;
}

CollisionLinkModelsCommandResult CollisionLinkModelsCommandController::generateCoacdForObject(
    simulation_project::ProjectDocument& document,
    robot_qt_viewer::RobotQtViewerViewportServices* viewportServices,
    const QString& objectId)
{
    if(objectId.isEmpty()) {
        return makeFailure("Select an object or attachment first.");
    }
    if(viewportServices == nullptr) {
        return makeFailure("Viewport is not available.");
    }

    std::vector<simulation_project::ObjectCollisionElementOverrideDesc> elements;
    if(!viewportServices->generateObjectCollisionCoacdFromVisual(objectId, elements)) {
        return makeFailure("Failed to generate simplified object collision model with COACD.");
    }

    CollisionLinkModelsCommandResult result;
    result.success = true;
    result.projectChanged = true;
    populateGeneratedObjectVariantResult(result, elements);

    QString registrationError;
    if(!registerGeneratedObjectAsset(
           document,
           viewportServices,
           objectId.toStdString(),
           elements,
           registrationError)) {
        return makeFailure(registrationError);
    }

    result.message = QString("Generated %1 COACD object collision part(s).")
        .arg(result.generatedCount);
    return result;
}

CollisionLinkModelsCommandResult CollisionLinkModelsCommandController::saveSidecar(
    simulation_project::ProjectDocument& document,
    robot_qt_viewer::CollisionWorkbenchServices& appServices,
    const QString& robotId,
    const std::filesystem::path& sidecarPath,
    const std::string& portableSidecarPath)
{
    if(robotId.isEmpty()) {
        return makeFailure("Select a robot first.");
    }

    CollisionLinkModelDocumentFacade documentFacade(document);
    simulation_project::RobotCollisionOverrideDesc* collisionOverride =
        documentFacade.findOverride(robotId.toStdString());
    if(collisionOverride == nullptr || collisionOverride->elements.empty()) {
        return makeFailure("Selected robot has no collision overrides.");
    }

    const simulation_project::RobotDesc* robotDesc = documentFacade.findRobot(robotId.toStdString());
    if(robotDesc != nullptr && collisionOverride->sourceRobotPath.empty()) {
        collisionOverride->sourceRobotPath = robotDesc->sourcePath;
    }

    std::string error;
    if(!appServices.saveRobotCollisionOverride(sidecarPath, *collisionOverride, &error)) {
        return makeFailure(QString("Sidecar save failed: %1").arg(QString::fromStdString(error)));
    }

    collisionOverride->overridePath = portableSidecarPath;

    CollisionLinkModelsCommandResult result;
    result.success = true;
    result.projectChanged = true;
    result.message = QString("Saved sidecar %1").arg(QString::fromStdWString(sidecarPath.wstring()));
    return result;
}

CollisionLinkModelsCommandResult CollisionLinkModelsCommandController::exportUrdf(
    simulation_project::ProjectDocument& document,
    robot_qt_viewer::CollisionWorkbenchServices& appServices,
    const QString& robotId,
    const std::filesystem::path& sourceUrdfPath,
    const std::filesystem::path& outputUrdfPath)
{
    if(robotId.isEmpty()) {
        return makeFailure("Select a robot first.");
    }

    CollisionLinkModelDocumentFacade documentFacade(document);
    simulation_project::RobotDesc* robotDesc = documentFacade.findRobot(robotId.toStdString());
    if(robotDesc == nullptr) {
        return makeFailure("Selected robot is not in project.");
    }
    if(robotDesc->sourceType != "urdf") {
        return makeFailure("URDF collision export only supports URDF robots.");
    }

    simulation_project::RobotCollisionOverrideDesc* collisionOverride =
        documentFacade.findOverride(robotId.toStdString());
    if(collisionOverride == nullptr || collisionOverride->elements.empty()) {
        return makeFailure("Selected robot has no collision overrides.");
    }

    collisionOverride->sourceRobotPath = robotDesc->sourcePath;

    std::string error;
    if(!appServices.exportRobotCollisionOverrideToUrdf(
           sourceUrdfPath,
           outputUrdfPath,
           *collisionOverride,
           &error)) {
        return makeFailure(QString("URDF export failed: %1").arg(QString::fromStdString(error)));
    }

    CollisionLinkModelsCommandResult result;
    result.success = true;
    result.message = QString("Exported URDF %1").arg(QString::fromStdWString(outputUrdfPath.wstring()));
    return result;
}
