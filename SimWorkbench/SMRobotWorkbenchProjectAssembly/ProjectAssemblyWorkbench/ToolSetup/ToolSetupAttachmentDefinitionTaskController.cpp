#include "ToolSetupTaskControllers.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerFileDialog.h"
#include "RobotQtViewerSelectionModel.h"
#include "ToolSetupAppServices.h"
#include "ToolSetupModuleController.h"
#include "ToolSetupTaskSupport.h"
#include "ToolSetupWidget.h"

#include <SimulationProject/ProjectDocumentService.h>
#include <SimulationProject/ProjectSession.h>

#include <CustomLog/CustomLog.h>

#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>

#include <filesystem>

namespace robot_qt_viewer
{
    using namespace tool_setup_detail;

    ToolSetupAttachmentDefinitionTaskController::
        ToolSetupAttachmentDefinitionTaskController(
            ToolSetupModuleController& host,
            QObject* parent)
        : QObject(parent)
        , m_host(host)
    {
    }

    void ToolSetupAttachmentDefinitionTaskController::handleSelectionChanged(
        int index)
    {
        if(m_host.m_updating || index < 0) {
            return;
        }

        const QString assetId = m_host.m_widget.assetIdAt(index);
        if(assetId.isEmpty()) {
            return;
        }

        const simulation_project::AttachmentAssetDesc* asset =
            findAttachmentAssetDesc(
                m_host.m_context.document(), assetId.toStdString());
        if(asset == nullptr) {
            emit m_host.statusMessageRequested(
                QString("Attachment asset not found: %1").arg(assetId), 4000);
            return;
        }

        m_host.m_context.selectionModel().selectToolAsset(
            assetId, QStringLiteral("toolSetup"));
        m_host.refresh(
            m_host.m_appServices.selectedRobotId(),
            m_host.m_appServices.selectedLinkName(),
            QString(),
            QString(),
            assetId);
        m_host.m_widget.focusAssetEditor();
        emit m_host.statusMessageRequested(
            QString("Selected attachment asset: %1")
                .arg(conciseUiText(asset->id, 48)),
            3000);
    }

    void ToolSetupAttachmentDefinitionTaskController::importAsset()
    {
        const QString fileName = robot_qt_viewer::getOpenFileName(
            QStringLiteral("toolSetup.asset.import"),
            &m_host.m_widget,
            "Import tool model",
            QString(),
            "Tool Meshes (*.stl *.STL *.obj *.OBJ *.dae *.DAE *.ply *.PLY *.gltf *.GLTF *.glb *.GLB *.step *.STEP *.stp *.STP);;All Files (*.*)");
        if(fileName.isEmpty()) {
            return;
        }

        const std::filesystem::path path = fileName.toStdWString();
        const std::string stem = qStringToUtf8(
            QString::fromStdWString(path.stem().wstring()));
        const simulation_project::ProjectDocument previousDocument =
            m_host.m_context.document();
        const bool previousDirty = m_host.m_context.projectSession().isDirty();

        simulation_project::AttachmentAssetDesc asset;
        asset.assetKind = "tool";
        asset.assetType = "tool";
        asset.visualPath =
            m_host.m_context.projectSession().makePortableAssetPath(path);
        asset.visualScale = 1.0;
        asset.visible = true;

        std::string mountId;
        std::string attachmentId;
        const QString currentMountId = m_host.m_widget.currentMountId();
        const QString selectedRobotId = m_host.m_appServices.selectedRobotId();
        const QString selectedLinkName = m_host.m_appServices.selectedLinkName();
        const ProjectMutationResult mutationResult =
            m_host.m_context.documentController().mutateProject(
                QStringLiteral("importToolAsset"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,
                    bool& changed,
                    std::string& error) {
                    asset.id = service.makeUniqueId(
                        makeAsciiSlug(stem, "tool_asset", 32));
                    asset.name = makeToolAssetDisplayName(stem, asset.id);
                    if(!service.addAttachmentAsset(asset, &error)) {
                        return false;
                    }

                    const simulation_project::RobotMountDesc* existing =
                        findRobotMountDesc(
                            service.document(), currentMountId.toStdString());
                    if(existing != nullptr &&
                        (selectedRobotId.isEmpty() ||
                            existing->robotId == selectedRobotId.toStdString())) {
                        mountId = existing->id;
                    } else if(!selectedRobotId.isEmpty() &&
                        !selectedLinkName.isEmpty()) {
                        const std::string robotId = selectedRobotId.toStdString();
                        const std::string linkName = selectedLinkName.toStdString();
                        for(const simulation_project::RobotMountDesc& candidate :
                            service.document().robotMounts) {
                            if(candidate.robotId == robotId &&
                                candidate.linkName == linkName) {
                                mountId = candidate.id;
                                break;
                            }
                        }
                        if(mountId.empty()) {
                            simulation_project::RobotMountDesc mount;
                            mount.id = service.makeUniqueId(
                                robotId + "_" + linkName + "_mount");
                            mount.name = mount.id;
                            mount.robotId = robotId;
                            mount.linkName = linkName;
                            if(!service.addRobotMount(mount, &error)) {
                                return false;
                            }
                            mountId = mount.id;
                        }
                    }

                    if(!mountId.empty()) {
                        const simulation_project::RobotMountDesc* mount =
                            findRobotMountDesc(service.document(), mountId);
                        const std::string mountName =
                            mount != nullptr && !mount->name.empty()
                            ? mount->name
                            : mountId;
                        const std::string attachmentName =
                            makeAttachmentDisplayName(asset.name, mountName);
                        attachmentId = service.makeUniqueId(
                            makeAsciiSlug(
                                attachmentName, "tool_attachment", 48));

                        simulation_project::MountedAttachmentDesc attachment;
                        attachment.id = attachmentId;
                        attachment.name = attachmentName;
                        attachment.mountFrameId = mountId;
                        attachment.assetId = asset.id;
                        if(!service.addMountedAttachment(attachment, &error) ||
                            !service.setActiveMountedAttachment(
                                mountId, attachmentId, &error)) {
                            return false;
                        }
                    }
                    changed = true;
                    return true;
                });
        if(!mutationResult.success) {
            emit m_host.statusMessageRequested(
                QString("Import tool failed: %1").arg(mutationResult.message), 5000);
            return;
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);
        emit m_host.statusMessageRequested(
            QString("Importing tool %1...").arg(fileName), 0);
        QApplication::processEvents();
        const ToolSetupViewportReloadResult reloadResult =
            m_host.m_appServices.reloadViewport(QStringLiteral("importToolAsset"));
        QApplication::restoreOverrideCursor();

        if(!reloadResult.success) {
            m_host.m_context.documentController().restoreProjectSnapshot(
                QStringLiteral("importToolAssetRollback"),
                previousDocument,
                previousDirty,
                false);
            m_host.m_appServices.reloadViewport(
                QStringLiteral("importToolAssetRollback"));
            const QString message = reloadResult.errorMessage.isEmpty()
                ? QString("Failed to rebuild viewport scene.")
                : reloadResult.errorMessage;
            LOG_ERROR("rs2022") << "Import tool failed: "
                                << message.toStdString();
            emit m_host.statusMessageRequested(
                QString("Import tool failed: %1").arg(message), 8000);
            return;
        }

        if(!attachmentId.empty()) {
            RobotQtViewerViewportPreviewPayload preview;
            preview.setActiveMountedAttachment = true;
            preview.activeMountedAttachmentId =
                QString::fromStdString(attachmentId);
            m_host.mutateViewportPreview(
                preview, QStringLiteral("importToolAsset"));
        }
        RobotQtViewerAttachmentPayload payload;
        payload.mountId = QString::fromStdString(mountId);
        payload.attachmentId = QString::fromStdString(attachmentId);
        payload.assetId = QString::fromStdString(asset.id);
        m_host.m_context.documentController().publishAttachmentChanged(
            payload, QStringLiteral("importToolAsset"));
        m_host.m_context.documentController().publishDocumentChanged(
            QStringLiteral("importToolAsset"), false);
        if(!attachmentId.empty()) {
            m_host.m_installedDeviceTask->selectById(attachmentId);
            m_host.m_widget.focusAssetEditor();
        }
        emit m_host.statusMessageRequested(
            QString(!attachmentId.empty()
                    ? "Imported and attached tool %1"
                    : "Imported tool asset %1; select a robot link or mount frame to attach it.")
                .arg(QString::fromUtf8(asset.id.c_str())),
            5000);
    }

    void ToolSetupAttachmentDefinitionTaskController::duplicateCurrent()
    {
        if(m_host.m_taskSession->dirty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral(
                    "Apply or cancel setup edits before duplicating a device definition."),
                4000);
            return;
        }

        const QString selectedAssetId = m_host.m_widget.currentAssetId();
        const simulation_project::AttachmentAssetDesc* source =
            findAttachmentAssetDesc(
                m_host.m_context.document(), selectedAssetId.toStdString());
        if(source == nullptr) {
            emit m_host.statusMessageRequested(
                "Select a device definition first.", 3000);
            return;
        }

        const QString sourceName = QString::fromStdString(
            source->name.empty() ? source->id : source->name);
        bool accepted = false;
        const QString duplicateName = QInputDialog::getText(
            &m_host.m_widget,
            source->assetType == "camera"
                ? QStringLiteral("Duplicate Camera Definition")
                : QStringLiteral("Duplicate Device Definition"),
            QStringLiteral("New definition name"),
            QLineEdit::Normal,
            sourceName + QStringLiteral(" Copy"),
            &accepted).trimmed();
        if(!accepted || duplicateName.isEmpty()) {
            return;
        }

        simulation_project::AttachmentAssetDesc duplicate = *source;
        const ProjectMutationResult mutationResult =
            m_host.m_context.documentController().mutateProject(
                QStringLiteral("duplicateDeviceDefinition"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,
                    bool& changed,
                    std::string& error) {
                    duplicate.id = service.makeUniqueId(makeAsciiSlug(
                        qStringToUtf8(duplicateName),
                        "device_definition",
                        32));
                    duplicate.name = qStringToUtf8(duplicateName);
                    for(std::size_t index = 0;
                        index < duplicate.functionalFrames.size();
                        ++index) {
                        simulation_project::AttachmentFunctionalFrameDesc& frame =
                            duplicate.functionalFrames[index];
                        const std::string suffix = !frame.frameType.empty()
                            ? makeAsciiSlug(frame.frameType, "frame", 20)
                            : std::string("frame_") + std::to_string(index + 1);
                        frame.id = duplicate.id + "." + suffix;
                    }
                    if(!service.addAttachmentAsset(duplicate, &error)) {
                        return false;
                    }
                    changed = true;
                    return true;
                });
        if(!mutationResult.success) {
            emit m_host.statusMessageRequested(
                QString("Duplicate definition failed: %1")
                    .arg(mutationResult.message),
                5000);
            return;
        }

        const QString duplicateId = QString::fromStdString(duplicate.id);
        m_host.m_context.selectionModel().selectToolAsset(
            duplicateId, QStringLiteral("duplicateDeviceDefinition"));
        RobotQtViewerAttachmentPayload payload;
        payload.assetId = duplicateId;
        m_host.m_context.documentController().publishAttachmentChanged(
            payload, QStringLiteral("duplicateDeviceDefinition"));
        m_host.m_context.documentController().publishDocumentChanged(
            QStringLiteral("duplicateDeviceDefinition"), false);
        m_host.refresh(
            m_host.m_appServices.selectedRobotId(),
            m_host.m_appServices.selectedLinkName(),
            QString(),
            QString(),
            duplicateId);
        m_host.m_widget.focusAssetEditor();
        emit m_host.selectionDependentViewsRefreshRequested();
        emit m_host.statusMessageRequested(
            QString("Created independent device definition: %1")
                .arg(duplicateName),
            5000);
    }

    void ToolSetupAttachmentDefinitionTaskController::editCurrent()
    {
        const QString attachmentId = m_host.m_widget.currentAttachmentId();
        const simulation_project::MountedAttachmentDesc* attachment =
            findMountedAttachmentDesc(
                m_host.m_context.document(), attachmentId.toStdString());
        const QString selectedAssetId = m_host.m_widget.currentAssetId();
        const std::string assetId = attachment != nullptr
            ? attachment->assetId
            : selectedAssetId.toStdString();
        const simulation_project::AttachmentAssetDesc* asset =
            findAttachmentAssetDesc(m_host.m_context.document(), assetId);
        if(asset == nullptr) {
            emit m_host.statusMessageRequested(
                assetId.empty()
                    ? QStringLiteral("Select an attachment asset first.")
                    : QString("Attachment asset not found: %1")
                        .arg(QString::fromStdString(assetId)),
                5000);
            return;
        }

        m_host.m_widget.focusAssetEditor();
        emit m_host.statusMessageRequested(
            QString("Editing attachment asset in task panel: %1")
                .arg(conciseUiText(asset->id, 48)),
            3000);
    }

    bool ToolSetupAttachmentDefinitionTaskController::editById(
        const std::string& assetId)
    {
        const simulation_project::AttachmentAssetDesc* attachmentAsset =
            findAttachmentAssetDesc(m_host.m_context.document(), assetId);
        if(attachmentAsset == nullptr) {
            emit m_host.statusMessageRequested(
                QString("Attachment asset not found: %1")
                    .arg(QString::fromStdString(assetId)),
                5000);
            return false;
        }

        const simulation_project::MountedAttachmentDesc* attachment =
            findMountedAttachmentByAssetId(m_host.m_context.document(), assetId);
        if(attachment == nullptr) {
            const QString assetIdText = QString::fromStdString(assetId);
            m_host.m_context.selectionModel().selectToolAsset(
                assetIdText, QStringLiteral("editToolAssetById"));
            m_host.refresh(
                m_host.m_appServices.selectedRobotId(),
                m_host.m_appServices.selectedLinkName(),
                QString(),
                QString(),
                assetIdText);
            m_host.m_widget.focusAssetEditor();
            emit m_host.statusMessageRequested(
                QString("Editing unmounted attachment asset in task panel: %1")
                    .arg(conciseUiText(assetId, 48)),
                3000);
            return true;
        }

        m_host.m_installedDeviceTask->selectById(attachment->id);
        m_host.m_widget.focusAssetEditor();
        emit m_host.statusMessageRequested(
            QString("Editing attachment asset in task panel: %1")
                .arg(conciseUiText(assetId, 48)),
            3000);
        return true;
    }

    void ToolSetupAttachmentDefinitionTaskController::createFromSceneObject(
        const QString& objectId)
    {
        if(objectId.isEmpty()) {
            return;
        }

        const simulation_project::SceneObjectDesc* object = findSceneObjectDesc(
            m_host.m_context.document(), objectId.toStdString());
        if(object == nullptr) {
            emit m_host.statusMessageRequested(
                QStringLiteral("Selected object is not in project."), 3000);
            return;
        }
        if(object->sourcePath.empty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral("Selected object has no source model path."), 4000);
            return;
        }

        const std::string objectName =
            object->name.empty() ? object->id : object->name;
        simulation_project::AttachmentAssetDesc asset;
        asset.assetKind = "tool";
        asset.assetType =
            object->objectType.empty() ? std::string("tool") : object->objectType;
        asset.visualPath = object->sourcePath;
        asset.visualScale = object->visualScale > 0.0 ? object->visualScale : 1.0;
        asset.visible = object->visible;

        const ProjectMutationResult mutationResult =
            m_host.m_context.documentController().mutateProject(
                QStringLiteral("createToolAssetFromSceneObject"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,
                    bool& changed,
                    std::string& error) {
                    asset.id = service.makeUniqueId(
                        makeAsciiSlug(objectName, "tool_asset", 32));
                    asset.name = makeToolAssetDisplayName(objectName, asset.id);
                    if(!service.addAttachmentAsset(asset, &error)) {
                        return false;
                    }
                    changed = true;
                    return true;
                });
        if(!mutationResult.success) {
            emit m_host.statusMessageRequested(
                QString("Create tool asset failed: %1")
                    .arg(mutationResult.message),
                5000);
            return;
        }

        RobotQtViewerAttachmentPayload payload;
        payload.assetId = QString::fromStdString(asset.id);
        m_host.m_context.documentController().publishAttachmentChanged(
            payload, QStringLiteral("createToolAssetFromSceneObject"));
        emit m_host.statusMessageRequested(
            QString(
                "Created tool asset %1 from object %2; attach it before editing in mount setup")
                .arg(QString::fromStdString(asset.id), objectId),
            5000);
    }
}
