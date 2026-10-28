#include "SceneEntityWorkflowController.h"
#include "UserCoordinateFrame.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"

#include <SimulationProject/ProjectDocument.h>
#include <SimulationProject/ProjectDocumentService.h>
#include <SimulationProject/ProjectSceneEntityCommands.h>
#include <SimulationProject/ProjectSession.h>
#include <RobotIO/IRobotLoader.h>

#include <cmath>
#include <cctype>
#include <exception>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
    struct PcdHeaderProbe
    {
        bool success = false;
        std::string error;
        std::string dataType;
        std::vector<std::string> fields;
    };

    std::string pathStemUtf8(const std::filesystem::path& path)
    {
        return path.stem().generic_u8string();
    }

    std::string lowercaseAscii(std::string value)
    {
        for(char& ch : value) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return value;
    }

    std::vector<std::string> splitWhitespace(const std::string& line)
    {
        std::istringstream stream(line);
        std::vector<std::string> words;
        std::string word;
        while(stream >> word) {
            words.push_back(word);
        }
        return words;
    }

    PcdHeaderProbe probePcdHeader(const std::filesystem::path& path)
    {
        PcdHeaderProbe probe;
        std::ifstream file(path, std::ios::binary);
        if(!file) {
            probe.error = "Failed to open PCD file.";
            return probe;
        }

        std::string line;
        while(std::getline(file, line)) {
            if(line.empty() || line[0] == '#') {
                continue;
            }
            const std::vector<std::string> words = splitWhitespace(line);
            if(words.empty()) {
                continue;
            }
            const std::string key = words.front();
            if(key == "FIELDS") {
                probe.fields.assign(words.begin() + 1, words.end());
            } else if(key == "DATA" && words.size() > 1) {
                probe.dataType = lowercaseAscii(words[1]);
                break;
            }
        }

        if(probe.dataType.empty()) {
            probe.error = "PCD header is missing DATA.";
            return probe;
        }
        const auto hasField = [&](const std::string& name) {
            for(const std::string& field : probe.fields) {
                if(field == name) {
                    return true;
                }
            }
            return false;
        };
        if(!hasField("x") || !hasField("y") || !hasField("z")) {
            probe.error = "PCD file does not contain x/y/z fields.";
            return probe;
        }
        if(probe.dataType != "ascii" &&
            probe.dataType != "binary" &&
            probe.dataType != "binary_compressed") {
            probe.error = "Unsupported PCD DATA type: " + probe.dataType;
            return probe;
        }

        probe.success = true;
        return probe;
    }

    std::string sanitizeIdBase(std::string value, const std::string& fallback)
    {
        if(value.empty()) {
            value = fallback;
        }
        for(char& ch : value) {
            if(!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                ch = '_';
            }
        }
        return value.empty() ? fallback : value;
    }

    std::string normalizedStoredPath(std::string value)
    {
        for(char& ch : value) {
            if(ch == '\\') {
                ch = '/';
            } else {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
        }
        return value;
    }

    const simulation_project::PointCloudDesc* findPointCloudWithSourcePath(
        const simulation_project::ProjectDocument& document,
        const std::string& sourcePath)
    {
        const std::string normalizedSourcePath = normalizedStoredPath(sourcePath);
        for(const simulation_project::PointCloudDesc& pointCloud : document.pointClouds) {
            if(normalizedStoredPath(pointCloud.sourcePath) == normalizedSourcePath) {
                return &pointCloud;
            }
        }
        return nullptr;
    }

    bool nearlyEqual(double lhs, double rhs)
    {
        return std::abs(lhs - rhs) <= 1.0e-9;
    }

    bool sameTransform(
        const simulation_project::TransformDesc& lhs,
        const simulation_project::TransformDesc& rhs)
    {
        return nearlyEqual(lhs.x, rhs.x) &&
            nearlyEqual(lhs.y, rhs.y) &&
            nearlyEqual(lhs.z, rhs.z) &&
            nearlyEqual(lhs.roll, rhs.roll) &&
            nearlyEqual(lhs.pitch, rhs.pitch) &&
            nearlyEqual(lhs.yaw, rhs.yaw);
    }

    bool isSimscapeSourceType(const std::string& sourceType)
    {
        return lowercaseAscii(sourceType) == "simscape";
    }

    std::string modelDisplayName(
        const std::string& sourceName,
        const robot::RobotModel& model,
        std::size_t modelIndex,
        std::size_t modelCount)
    {
        if(modelCount <= 1) {
            return sourceName;
        }
        const std::string modelName = model.name.empty()
            ? std::string("model_") + std::to_string(modelIndex + 1)
            : model.name;
        return sourceName + "_" + modelName;
    }
}

namespace robot_qt_viewer
{
    SceneEntityWorkflowController::SceneEntityWorkflowController(RobotQtViewerDocumentContext& context)
        : m_context(context)
    {
    }

    SceneEntityImportResult SceneEntityWorkflowController::importRobotFromPath(
        const std::filesystem::path& path,
        const std::string& sourceType)
    {
        SceneEntityImportResult result;
        result.previousDocument = m_context.document();
        result.previousDirty = m_context.projectSession().isDirty();

        simulation_project::AddRobotEntityCommand command;
        const std::string sourceName = pathStemUtf8(path);
        command.displayName = sourceName;
        command.sourceType = sourceType;
        command.sourcePath = m_context.projectSession().makePortableAssetPath(path);
        std::vector<std::string> displayNames;
        if(isSimscapeSourceType(sourceType)) {
            std::vector<robot::RobotModel> models;
            try {
                models = IRobotLoader::get_robots(RobotType::SimscapeRobot, path.generic_u8string());
            } catch(const std::exception& ex) {
                result.message = QString("Import robot failed: %1").arg(QString::fromStdString(ex.what()));
                return result;
            }
            if(models.empty()) {
                result.message = QString("Import robot failed: no Simscape models loaded from %1")
                    .arg(QString::fromStdWString(path.wstring()));
                return result;
            }
            displayNames.reserve(models.size());
            for(std::size_t index = 0; index < models.size(); ++index) {
                displayNames.push_back(modelDisplayName(sourceName, models[index], index, models.size()));
            }
        } else {
            displayNames.push_back(sourceName);
        }

        std::size_t importedCount = 0;
        simulation_project::ProjectSceneEntityCommandResult commandResult;
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("importRobot"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                simulation_project::ProjectSceneEntityCommands commands(service.document());
                for(std::size_t index = 0; index < displayNames.size(); ++index) {
                    command.displayName = displayNames[index];
                    command.sourceModelIndex = static_cast<int>(index);
                    simulation_project::ProjectSceneEntityCommandResult currentResult =
                        commands.addRobot(command);
                    if(!currentResult.success) {
                        error = currentResult.message;
                        return false;
                    }
                    if(index == 0) {
                        commandResult = currentResult;
                    }
                    ++importedCount;
                }
                changed = true;
                return true;
            });
        result.success = mutationResult.success;
        result.message = mutationResult.success && importedCount > 1
            ? QString("Imported %1 robots from %2")
                .arg(static_cast<qulonglong>(importedCount))
                .arg(QString::fromStdString(command.sourcePath))
            : mutationResult.success
                ? QString::fromStdString(commandResult.message)
            : mutationResult.message;
        result.entityId = QString::fromStdString(commandResult.entityId);
        result.storedPath = QString::fromStdString(commandResult.storedPath);
        return result;
    }

    SceneEntityImportResult SceneEntityWorkflowController::importSceneObjectFromPath(
        const std::filesystem::path& path,
        const std::string& objectType,
        SceneObjectImportUnit unit)
    {
        SceneEntityImportResult result;
        result.previousDocument = m_context.document();
        result.previousDirty = m_context.projectSession().isDirty();
        double scale = 1.0;
        switch(unit) {
        case SceneObjectImportUnit::Meters: break;
        case SceneObjectImportUnit::Millimeters: scale = 0.001; break;
        default:
            result.message = QStringLiteral("Unsupported object import unit.");
            return result;
        }
        return importSceneObjectFromPath(path, objectType, scale);
    }

    SceneEntityImportResult SceneEntityWorkflowController::importSceneObjectFromPath(
        const std::filesystem::path& path,
        const std::string& objectType,
        double scale)
    {
        SceneEntityImportResult result;
        result.previousDocument = m_context.document();
        result.previousDirty = m_context.projectSession().isDirty();

        if(!std::isfinite(scale) || scale <= 0.0) {
            result.message = QStringLiteral("Import scale must be positive and finite.");
            return result;
        }

        simulation_project::AddSceneObjectEntityCommand command;
        command.displayName = pathStemUtf8(path);
        command.objectType = objectType;
        command.sourcePath = m_context.projectSession().makePortableAssetPath(path);
        simulation_project::ProjectSceneEntityCommandResult commandResult;
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("importObject"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                simulation_project::ProjectSceneEntityCommands commands(service.document());
                commandResult = commands.addSceneObject(command);
                if(!commandResult.success) {
                    error = commandResult.message;
                    return false;
                }
                auto* object = service.findSceneObject(commandResult.entityId);
                if(!object) {
                    error = "Imported object is missing from the project document.";
                    return false;
                }
                // Persist one unit conversion for both rendering and collision geometry.
                object->visualScale = scale;
                object->collisionScale = scale;
                if(belongsToUserFrame(service.document(), *object))
                    object->transform = userCoordinateFrame(service.document());
                changed = true;
                return true;
            });
        result.success = mutationResult.success;
        result.message = mutationResult.success
            ? QString::fromStdString(commandResult.message)
            : mutationResult.message;
        result.entityId = QString::fromStdString(commandResult.entityId);
        result.storedPath = QString::fromStdString(commandResult.storedPath);
        return result;
    }

    SceneEntityImportResult SceneEntityWorkflowController::importPointCloudFromPath(
        const std::filesystem::path& path,
        const std::string& format)
    {
        SceneEntityImportResult result;
        result.previousDocument = m_context.document();
        result.previousDirty = m_context.projectSession().isDirty();

        if(!std::filesystem::exists(path)) {
            result.message = QString("Point cloud file not found: %1").arg(QString::fromStdWString(path.wstring()));
            return result;
        }
        const PcdHeaderProbe headerProbe = probePcdHeader(path);
        if(!headerProbe.success) {
            result.message = QString("Import point cloud failed: %1").arg(QString::fromStdString(headerProbe.error));
            return result;
        }

        const std::string displayName = pathStemUtf8(path);
        const std::string storedPath = m_context.projectSession().makePortableAssetPath(path);
        simulation_project::PointCloudDesc pointCloud;
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("importPointCloud"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                pointCloud.id = service.makeUniqueId(sanitizeIdBase(displayName, "point_cloud"));
                pointCloud.name = displayName.empty() ? pointCloud.id : displayName;
                pointCloud.sourcePath = storedPath;
                pointCloud.format = format.empty() ? "pcd" : format;
                pointCloud.scale = 1.0;
                if(const simulation_project::PointCloudDesc* existing =
                       findPointCloudWithSourcePath(service.document(), pointCloud.sourcePath)) {
                    pointCloud.visualization = existing->visualization;
                } else {
                    pointCloud.visualization.visible = true;
                    pointCloud.visualization.pointSize = 2.5;
                    pointCloud.visualization.maxRenderPoints = 300000;
                }
                pointCloud.collision.enabled = false;
                if(!service.addPointCloud(pointCloud, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        result.success = mutationResult.success;
        result.message = mutationResult.success
            ? QString("Imported point cloud %1").arg(QString::fromStdString(pointCloud.id))
            : mutationResult.message;
        result.entityId = QString::fromStdString(pointCloud.id);
        result.storedPath = QString::fromStdString(pointCloud.sourcePath);
        return result;
    }

    void SceneEntityWorkflowController::restoreImportState(const SceneEntityImportResult& result)
    {
        m_context.documentController().restoreProjectSnapshot(
            QStringLiteral("restoreImportState"),
            result.previousDocument,
            result.previousDirty);
    }

    SceneEntityDeleteResult SceneEntityWorkflowController::deleteEntity(
        SceneEntityKind kind,
        const QString& entityId)
    {
        SceneEntityDeleteResult result;
        result.kind = kind;
        result.entityId = entityId;
        if(entityId.isEmpty()) {
            result.message = QStringLiteral("No scene item selected.");
            return result;
        }

        simulation_project::ProjectReferenceCleanupReport report;
        bool removed = false;
        QString itemKind;
        if(kind == SceneEntityKind::Robot) {
            itemKind = QStringLiteral("robot");
        } else if(kind == SceneEntityKind::PointCloud) {
            itemKind = QStringLiteral("point cloud");
        } else {
            itemKind = QStringLiteral("object");
        }
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("deleteSceneEntity"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                if(kind == SceneEntityKind::Robot) {
                    removed = service.removeRobot(entityId.toStdString(), report, &error);
                } else if(kind == SceneEntityKind::PointCloud) {
                    removed = service.removePointCloud(entityId.toStdString(), report, &error);
                } else if(kind == SceneEntityKind::AttachmentAsset) {
                    if(!service.removeAttachmentAssetIfUnused(entityId.toStdString(), &removed, &error)) {
                        return false;
                    }
                    if(!removed) {
                        error = "Device definition is still in use. Delete or unbind its installed instances first.";
                    }
                } else if(kind == SceneEntityKind::MountedAttachment) {
                    const auto* attachment = service.findMountedAttachment(entityId.toStdString());
                    const std::string assetId = attachment != nullptr ? attachment->assetId : std::string();
                    removed = service.removeMountedAttachment(entityId.toStdString(), &error);
                    if(removed && !assetId.empty() &&
                        !service.removeAttachmentAssetIfUnused(assetId, nullptr, &error)) {
                        return false;
                    }
                } else {
                    removed = service.removeObject(entityId.toStdString(), report, &error);
                }
                changed = removed;
                return removed;
            });
        if(!mutationResult.success) {
            result.message = mutationResult.message.isEmpty()
                ? QString("Selected %1 is not in project.").arg(itemKind)
                : mutationResult.message;
            return result;
        }

        result.success = true;
        result.removedCollisionDetectors = report.removedCollisionDetectors.size();
        result.removedTools = report.removedMountedAttachments.size();
        result.removedSensors = 0;
        result.message = QString("Deleted %1 %2; removed %3 detectors, %4 tools, %5 sensors")
            .arg(itemKind, entityId)
            .arg(static_cast<qulonglong>(result.removedCollisionDetectors))
            .arg(static_cast<qulonglong>(result.removedTools))
            .arg(static_cast<qulonglong>(result.removedSensors));
        return result;
    }

    SceneEntityMutationResult SceneEntityWorkflowController::setEntityRole(
        SceneEntityKind kind, const QString& entityId, const QString& role)
    {
        SceneEntityMutationResult result;
        result.entityId = entityId;
        if(role != "tool" && role != "workpiece") {
            result.message = "Role must be tool or workpiece.";
            return result;
        }
        const auto mutation = m_context.documentController().mutateProject(
            QStringLiteral("setEntityRole"), ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                auto& doc = service.document();
                const auto id = entityId.toStdString();
                std::vector<std::string> assetIds;
                std::vector<std::string> sourceIds;
                if(kind == SceneEntityKind::Object) {
                    auto* object = service.findSceneObject(id);
                    if(!object) { error = "Object not found"; return false; }
                    sourceIds.push_back(id);
                    for(const auto& attachment : doc.mountedAttachments)
                        if(attachment.sourceObjectId == id) assetIds.push_back(attachment.assetId);
                } else if(kind == SceneEntityKind::MountedAttachment) {
                    auto* attachment = service.findMountedAttachment(id);
                    if(!attachment) { error = "Attachment not found"; return false; }
                    // A role belongs to this installed instance, not every use of a shared asset.
                    const auto oldAssetId = attachment->assetId;
                    if(std::count_if(doc.mountedAttachments.begin(), doc.mountedAttachments.end(),
                        [&](const auto& value) { return value.assetId == oldAssetId; }) > 1) {
                        auto copy = *service.findAttachmentAsset(oldAssetId);
                        copy.id = service.makeUniqueId(oldAssetId + "_instance");
                        doc.attachmentAssets.push_back(copy);
                        attachment->assetId = copy.id;
                        changed = true;
                    }
                    assetIds.push_back(attachment->assetId);
                    if(!attachment->sourceObjectId.empty()) sourceIds.push_back(attachment->sourceObjectId);
                } else if(kind == SceneEntityKind::AttachmentAsset) {
                    if(!service.findAttachmentAsset(id)) { error = "Definition not found"; return false; }
                    assetIds.push_back(id);
                    for(const auto& attachment : doc.mountedAttachments)
                        if(attachment.assetId == id && !attachment.sourceObjectId.empty()) sourceIds.push_back(attachment.sourceObjectId);
                } else { error = "Select an object or installed device"; return false; }
                for(auto& asset : doc.attachmentAssets) {
                    if(std::find(assetIds.begin(), assetIds.end(), asset.id) == assetIds.end()) continue;
                    if(asset.hasSensorIntrinsics || asset.assetKind == "sensor") {
                        error = "A sensor definition cannot be converted into a tool/workpiece";
                        return false;
                    }
                    changed |= asset.assetKind != role.toStdString() || asset.assetType != role.toStdString();
                    asset.assetKind = role.toStdString();
                    asset.assetType = role.toStdString();
                }
                for(auto& object : doc.objects) {
                    if(std::find(sourceIds.begin(), sourceIds.end(), object.id) == sourceIds.end()) continue;
                    changed |= object.objectType != role.toStdString();
                    object.objectType = role.toStdString();
                }
                return true;
            });
        result.success = mutation.success;
        result.message = mutation.success ? QString("Role set to %1.").arg(role) : mutation.message;
        return result;
    }

    SceneEntityMutationResult SceneEntityWorkflowController::setMountedTcp(
        const QString& attachmentId, const simulation_project::TransformDesc& assetToTcp,
        const QString& convention, const QString& referenceLink)
    {
        SceneEntityMutationResult result;
        result.entityId = attachmentId;
        for(double value : {assetToTcp.x, assetToTcp.y, assetToTcp.z, assetToTcp.roll, assetToTcp.pitch, assetToTcp.yaw}) {
            if(!std::isfinite(value)) { result.message = "TCP values must be finite."; return result; }
        }
        if(convention != "fanuc" && convention != "abb" && convention != "generic") {
            result.message = "Unknown TCP convention.";
            return result;
        }
        const auto mutation = m_context.documentController().mutateProject(
            QStringLiteral("setMountedTcp"), ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                auto* attachment = service.findMountedAttachment(attachmentId.toStdString());
                if(!attachment) { error = "Installed device not found"; return false; }
                auto& doc = service.document();
                const auto oldAssetId = attachment->assetId;
                auto* asset = service.findAttachmentAsset(oldAssetId);
                if(!asset || asset->hasSensorIntrinsics || asset->assetKind == "sensor") {
                    error = "Select a tool, not a sensor"; return false;
                }
                if(std::count_if(doc.mountedAttachments.begin(), doc.mountedAttachments.end(),
                    [&](const auto& value) { return value.assetId == oldAssetId; }) > 1) {
                    auto copy = *asset;
                    copy.id = service.makeUniqueId(oldAssetId + "_tcp");
                    doc.attachmentAssets.push_back(copy);
                    attachment->assetId = copy.id;
                    asset = service.findAttachmentAsset(copy.id);
                }
                asset->assetKind = "tool";
                asset->assetType = "tool";
                if(auto* source = service.findSceneObject(attachment->sourceObjectId)) source->objectType = "tool";
                auto& frames = asset->functionalFrames;
                frames.erase(std::remove_if(frames.begin(), frames.end(), [](const auto& f) {
                    return f.frameType == "tcp";
                }), frames.end());
                for(auto& frame : frames) frame.primary = false;
                simulation_project::AttachmentFunctionalFrameDesc tcp;
                tcp.id = asset->id + ".tcp";
                tcp.name = "TCP";
                tcp.frameType = "tcp";
                tcp.primary = true;
                tcp.assetMountToFrame = assetToTcp;
                frames.push_back(tcp);
                if(!service.setActiveMountedAttachment(attachment->mountFrameId, attachment->id, &error)) return false;
                const auto key = "rws.tcp." + attachment->id;
                const auto payload = QJsonDocument(QJsonObject{{"convention", convention},
                    {"reference_link", referenceLink}}).toJson(QJsonDocument::Compact).toStdString();
                auto pref = std::find_if(doc.extensions.begin(), doc.extensions.end(), [&](const auto& e) { return e.key == key; });
                if(pref == doc.extensions.end()) doc.extensions.push_back({key, 1, payload});
                else pref->serializedPayload = payload;
                if(coordinateFrameDeleted(doc,"tcp:"+attachment->id))
                    setCoordinateFrameState(doc,"tcp:"+attachment->id,true,false);
                changed = true;
                return true;
            });
        result.success = mutation.success;
        result.message = mutation.success ? QStringLiteral("TCP updated.") : mutation.message;
        return result;
    }

    SceneEntityMutationResult SceneEntityWorkflowController::setRobotBaseTransform(
        const QString& robotId,
        const simulation_project::TransformDesc& transform)
    {
        SceneEntityMutationResult result;
        result.entityId = robotId;
        if(robotId.isEmpty()) {
            result.message = QStringLiteral("No robot selected.");
            return result;
        }

        const simulation_project::ProjectDocument& document = m_context.document();
        simulation_project::ProjectDocumentService readService(document);
        const simulation_project::RobotDesc* robot = readService.findRobot(robotId.toStdString());
        if(robot != nullptr && sameTransform(robot->baseTransform, transform)) {
            result.success = true;
            result.message = QString("Robot base unchanged: %1").arg(robotId);
            return result;
        }
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("robotBaseTransform"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                const simulation_project::RobotDesc* current = service.findRobot(robotId.toStdString());
                if(current != nullptr && sameTransform(current->baseTransform, transform)) {
                    changed = false;
                    return true;
                }
                if(!service.setRobotBaseTransform(robotId.toStdString(), transform, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!mutationResult.success) {
            result.message = QString("Move base failed: %1").arg(mutationResult.message);
            return result;
        }
        result.success = true;
        result.message = mutationResult.changed
            ? QString("Moved base of %1").arg(robotId)
            : QString("Robot base unchanged: %1").arg(robotId);
        return result;
    }

    SceneEntityMutationResult SceneEntityWorkflowController::setSceneObjectTransform(
        const QString& objectId,
        const simulation_project::TransformDesc& transform)
    {
        SceneEntityMutationResult result;
        result.entityId = objectId;
        if(objectId.isEmpty()) {
            result.message = QStringLiteral("No object selected.");
            return result;
        }

        const simulation_project::ProjectDocument& document = m_context.document();
        simulation_project::ProjectDocumentService readService(document);
        const simulation_project::SceneObjectDesc* object = readService.findSceneObject(objectId.toStdString());
        if(object != nullptr && sameTransform(object->transform, transform)) {
            result.success = true;
            result.message = QString("Object transform unchanged: %1").arg(objectId);
            return result;
        }
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("sceneObjectTransform"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                const simulation_project::SceneObjectDesc* current = service.findSceneObject(objectId.toStdString());
                if(current != nullptr && sameTransform(current->transform, transform)) {
                    changed = false;
                    return true;
                }
                if(!service.setSceneObjectTransform(objectId.toStdString(), transform, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!mutationResult.success) {
            result.message = QString("Move object failed: %1").arg(mutationResult.message);
            return result;
        }

        result.success = true;
        result.message = mutationResult.changed
            ? QString("Moved object %1").arg(objectId)
            : QString("Object transform unchanged: %1").arg(objectId);
        return result;
    }

    SceneEntityMutationResult SceneEntityWorkflowController::setPointCloudTransform(
        const QString& pointCloudId,
        const simulation_project::TransformDesc& transform)
    {
        SceneEntityMutationResult result;
        result.entityId = pointCloudId;
        if(pointCloudId.isEmpty()) {
            result.message = QStringLiteral("No point cloud selected.");
            return result;
        }

        const simulation_project::ProjectDocument& document = m_context.document();
        simulation_project::ProjectDocumentService readService(document);
        const simulation_project::PointCloudDesc* pointCloud = readService.findPointCloud(pointCloudId.toStdString());
        if(pointCloud != nullptr && sameTransform(pointCloud->transform, transform)) {
            result.success = true;
            result.message = QString("Point cloud transform unchanged: %1").arg(pointCloudId);
            return result;
        }
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("pointCloudTransform"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                const simulation_project::PointCloudDesc* current = service.findPointCloud(pointCloudId.toStdString());
                if(current != nullptr && sameTransform(current->transform, transform)) {
                    changed = false;
                    return true;
                }
                if(!service.setPointCloudTransform(pointCloudId.toStdString(), transform, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!mutationResult.success) {
            result.message = QString("Move point cloud failed: %1").arg(mutationResult.message);
            return result;
        }

        result.success = true;
        result.message = mutationResult.changed
            ? QString("Moved point cloud %1").arg(pointCloudId)
            : QString("Point cloud transform unchanged: %1").arg(pointCloudId);
        return result;
    }

    SceneEntityMutationResult SceneEntityWorkflowController::renamePointCloud(
        const QString& pointCloudId,
        const QString& name)
    {
        SceneEntityMutationResult result;
        result.entityId = pointCloudId;
        if(pointCloudId.isEmpty()) {
            result.message = QStringLiteral("No point cloud selected.");
            return result;
        }

        const std::string newName = name.toStdString();
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("pointCloudRename"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                const simulation_project::PointCloudDesc* pointCloud =
                    service.findPointCloud(pointCloudId.toStdString());
                if(pointCloud != nullptr &&
                    pointCloud->name == (newName.empty() ? pointCloud->id : newName)) {
                    changed = false;
                    return true;
                }
                if(!service.renamePointCloud(pointCloudId.toStdString(), newName, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!mutationResult.success) {
            result.message = QString("Rename point cloud failed: %1").arg(mutationResult.message);
            return result;
        }

        result.success = true;
        result.message = mutationResult.changed
            ? QString("Renamed point cloud %1").arg(pointCloudId)
            : QString("Point cloud unchanged: %1").arg(pointCloudId);
        return result;
    }

    SceneEntityMutationResult SceneEntityWorkflowController::replaceRobotMountsForRobot(
        const QString& robotId,
        const std::vector<simulation_project::RobotMountDesc>& mounts)
    {
        SceneEntityMutationResult result;
        result.entityId = robotId;
        if(robotId.isEmpty()) {
            result.message = QStringLiteral("No robot selected.");
            return result;
        }

        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("robotFlangeConfig"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                if(!service.replaceRobotMountsForRobot(robotId.toStdString(), mounts, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!mutationResult.success) {
            result.message =
                QString("Configure robot flange failed: %1").arg(mutationResult.message);
            return result;
        }

        result.success = true;
        result.message = QString("Configured robot flange for %1").arg(robotId);
        return result;
    }

}
