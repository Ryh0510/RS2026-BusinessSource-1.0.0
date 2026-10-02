#include "ToolSetupTaskControllers.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerSelectionModel.h"
#include "ToolAttachmentCommandController.h"
#include "ToolSetupAppServices.h"
#include "ToolSetupModuleController.h"
#include "ToolSetupTaskSupport.h"
#include "ToolSetupWidget.h"

#include <SimulationProject/ProjectAttachmentCommands.h>
#include <SimulationProject/ProjectDocumentService.h>
#include <SimulationProject/ProjectSession.h>

#include <QMessageBox>

namespace robot_qt_viewer
{
    using namespace tool_setup_detail;

    ToolSetupInstalledDeviceTaskController::ToolSetupInstalledDeviceTaskController(
        ToolSetupModuleController& host,
        QObject* parent)
        : QObject(parent)
        , m_host(host)
    {
    }

    void ToolSetupInstalledDeviceTaskController::handleSelectionChanged(int index)
    {
        if(m_host.m_updating || index < 0) {
            return;
        }

        const QString attachmentId = m_host.m_widget.attachmentIdAt(index);
        if(attachmentId.isEmpty()) {
            return;
        }

        const simulation_project::MountedAttachmentDesc* attachment =
            findMountedAttachmentDesc(m_host.m_context.document(), attachmentId.toStdString());
        if(attachment == nullptr) {
            emit m_host.statusMessageRequested(
                QString("Attachment not found: %1").arg(attachmentId), 4000);
            return;
        }

        const std::string mountFrameId = attachment->mountFrameId;
        const std::string selectedAttachmentId = attachment->id;
        const std::string selectedAssetId = attachment->assetId;
        const RobotQtViewerSelectionPayload previousSelection =
            m_host.m_context.selectionModel().payload();
        if(!m_host.m_context.selectionModel().prepareSelectionChange(
               QStringLiteral("toolSetupAttachmentSelection"))) {
            m_host.refresh(
                previousSelection.robotId,
                previousSelection.linkName,
                previousSelection.mountId,
                previousSelection.attachmentId,
                previousSelection.assetId);
            return;
        }
        const ProjectMutationResult mutationResult =
            m_host.m_context.documentController().mutateProject(
                QStringLiteral("toolSetupActiveAttachment"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,
                    bool& changed,
                    std::string& error) {
                    if(!service.applyMountedAttachmentActivationPolicy(
                           selectedAttachmentId, &error)) {
                        return false;
                    }
                    changed = true;
                    return true;
                });
        if(!mutationResult.success) {
            emit m_host.statusMessageRequested(
                QString("Attachment selection failed: %1").arg(mutationResult.message),
                5000);
            return;
        }

        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(m_host.m_context.document(), mountFrameId);
        if(mount != nullptr) {
            const QString robotId = QString::fromStdString(mount->robotId);
            const QString linkName = QString::fromStdString(mount->linkName);
            const QString mountId = QString::fromStdString(mount->id);
            m_host.m_context.selectionModel().selectMountedAttachment(
                attachmentId,
                robotId,
                linkName,
                mountId,
                QStringLiteral("toolSetup"));
            const RobotQtViewerSelectionPayload committed =
                m_host.m_context.selectionModel().payload();
            if(committed.attachmentId != attachmentId) {
                m_host.refresh(
                    previousSelection.robotId,
                    previousSelection.linkName,
                    previousSelection.mountId,
                    previousSelection.attachmentId,
                    previousSelection.assetId);
                return;
            }
            m_host.m_appServices.setToolAttachmentContext(
                robotId, linkName, mountId, attachmentId);
            emit m_host.robotContextSelected(robotId);
            emit m_host.selectionDependentViewsRefreshRequested();
        }

        RobotQtViewerAttachmentPayload payload;
        payload.mountId = QString::fromStdString(mountFrameId);
        payload.attachmentId = attachmentId;
        payload.assetId = QString::fromStdString(selectedAssetId);
        m_host.m_context.documentController().publishAttachmentChanged(
            payload, QStringLiteral("toolSetup"));
        m_host.m_context.documentController().publishDocumentChanged(
            QStringLiteral("toolSetup"), false);
        RobotQtViewerViewportPreviewPayload preview;
        preview.setActiveMountedAttachment = true;
        preview.activeMountedAttachmentId = attachmentId;
        m_host.mutateViewportPreview(
            preview, QStringLiteral("toolSetupAttachmentSelection"));
        emit m_host.statusMessageRequested(
            QString("Active attachment: %1").arg(attachmentId), 3000);
    }

    void ToolSetupInstalledDeviceTaskController::attachExistingAsset()
    {
        const QString mountId = m_host.m_widget.currentMountId();
        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(m_host.m_context.document(), mountId.toStdString());
        if(mount == nullptr) {
            emit m_host.statusMessageRequested(
                "Select a mount frame before attaching an asset.", 4000);
            return;
        }
        if(m_host.m_context.document().attachmentAssets.empty()) {
            emit m_host.statusMessageRequested(
                "No attachment asset is available to attach.", 4000);
            return;
        }

        QVector<QPair<QString, QString>> assetItems;
        assetItems.reserve(
            static_cast<int>(m_host.m_context.document().attachmentAssets.size()));
        for(const simulation_project::AttachmentAssetDesc& asset :
            m_host.m_context.document().attachmentAssets) {
            const QString assetId = QString::fromStdString(asset.id);
            const QString assetName =
                QString::fromStdString(asset.name.empty() ? asset.id : asset.name);
            const QString typeName = asset.assetType == "camera"
                ? QStringLiteral("Camera")
                : QStringLiteral("Device");
            assetItems.push_back(qMakePair(
                QString("%1 | %2 | %3").arg(assetName, typeName, assetId),
                assetId));
        }

        EntitySelectionDialog dialog(
            QStringLiteral("Install Existing Device"),
            QStringLiteral("Device definition"),
            assetItems,
            m_host.m_widget.currentAssetId(),
            &m_host.m_widget);
        if(dialog.exec() != QDialog::Accepted) {
            return;
        }

        const std::string assetId = dialog.selectedId().toStdString();
        const simulation_project::AttachmentAssetDesc* selectedAsset =
            findAttachmentAssetDesc(m_host.m_context.document(), assetId);
        if(selectedAsset == nullptr) {
            emit m_host.statusMessageRequested(
                "Selected device definition no longer exists.", 4000);
            return;
        }

        const std::string assetName =
            selectedAsset->name.empty() ? selectedAsset->id : selectedAsset->name;
        const simulation_project::ProjectDocument previousDocument =
            m_host.m_context.document();
        const bool previousDirty = m_host.m_context.projectSession().isDirty();
        const std::string mountName = mount->name.empty() ? mount->id : mount->name;
        const std::string attachmentName =
            makeAttachmentDisplayName(assetName, mountName);
        std::string attachmentId;
        const std::string mountedAssetId = assetId;
        const std::string mountFrameId = mount->id;

        const ProjectMutationResult mutationResult =
            m_host.m_context.documentController().mutateProject(
                QStringLiteral("attachExistingToolAsset"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,
                    bool& changed,
                    std::string& error) {
                    if(findAttachmentAssetDesc(service.document(), assetId) == nullptr) {
                        error = "asset not found: " + assetId;
                        return false;
                    }
                    attachmentId = service.makeUniqueId(
                        makeAsciiSlug(attachmentName, "tool_attachment", 48));
                    simulation_project::MountedAttachmentDesc attachment;
                    attachment.id = attachmentId;
                    attachment.name = attachmentName;
                    attachment.mountFrameId = mountFrameId;
                    attachment.assetId = mountedAssetId;
                    if(!service.addMountedAttachment(attachment, &error) ||
                        !service.applyMountedAttachmentActivationPolicy(
                            attachmentId, &error)) {
                        return false;
                    }
                    changed = true;
                    return true;
                });
        if(!mutationResult.success) {
            emit m_host.statusMessageRequested(
                QString("Attach asset failed: %1").arg(mutationResult.message), 5000);
            return;
        }

        const ToolSetupViewportReloadResult reloadResult =
            m_host.m_appServices.reloadViewport(
                QStringLiteral("attachExistingToolAsset"));
        if(!reloadResult.success) {
            m_host.m_context.documentController().restoreProjectSnapshot(
                QStringLiteral("attachExistingToolAssetRollback"),
                previousDocument,
                previousDirty,
                false);
            m_host.m_appServices.reloadViewport(
                QStringLiteral("attachExistingToolAssetRollback"));
            emit m_host.statusMessageRequested(
                "Attach asset failed: viewport reload failed.", 8000);
            return;
        }

        selectById(attachmentId);
        RobotQtViewerAttachmentPayload payload;
        payload.mountId = QString::fromStdString(mountFrameId);
        payload.attachmentId = QString::fromStdString(attachmentId);
        payload.assetId = QString::fromStdString(mountedAssetId);
        m_host.m_context.documentController().publishAttachmentChanged(
            payload, QStringLiteral("attachExistingToolAsset"));
        m_host.m_context.documentController().publishDocumentChanged(
            QStringLiteral("attachExistingToolAsset"), false);
        emit m_host.statusMessageRequested(
            QString("Attached asset %1 to %2")
                .arg(QString::fromStdString(mountedAssetId), mountId),
            5000);
    }

    void ToolSetupInstalledDeviceTaskController::rebindCurrent()
    {
        if(m_host.m_taskSession->dirty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral(
                    "Apply or cancel setup edits before moving an installed device."),
                4000);
            return;
        }

        const QString attachmentId = m_host.m_widget.currentAttachmentId();
        const simulation_project::MountedAttachmentDesc* attachment =
            findMountedAttachmentDesc(
                m_host.m_context.document(), attachmentId.toStdString());
        if(attachment == nullptr) {
            emit m_host.statusMessageRequested(
                "Select an installed device first.", 3000);
            return;
        }

        QVector<QPair<QString, QString>> mountItems;
        mountItems.reserve(
            static_cast<int>(m_host.m_context.document().robotMounts.size()));
        for(const simulation_project::RobotMountDesc& mount :
            m_host.m_context.document().robotMounts) {
            const QString name =
                QString::fromStdString(mount.name.empty() ? mount.id : mount.name);
            mountItems.push_back(qMakePair(
                QString("%1 | %2 / %3 | %4").arg(
                    name,
                    QString::fromStdString(mount.robotId),
                    QString::fromStdString(mount.linkName),
                    QString::fromStdString(mount.id)),
                QString::fromStdString(mount.id)));
        }
        if(mountItems.isEmpty()) {
            emit m_host.statusMessageRequested("No mount frame is available.", 4000);
            return;
        }

        EntitySelectionDialog dialog(
            QStringLiteral("Move / Rebind Installed Device"),
            QStringLiteral("Target host frame"),
            mountItems,
            QString::fromStdString(attachment->mountFrameId),
            &m_host.m_widget);
        if(dialog.exec() != QDialog::Accepted) {
            return;
        }
        const QString targetMountId = dialog.selectedId();
        const simulation_project::RobotMountDesc* targetMount =
            findRobotMountDesc(
                m_host.m_context.document(), targetMountId.toStdString());
        if(targetMount == nullptr ||
            targetMountId == QString::fromStdString(attachment->mountFrameId)) {
            return;
        }

        const simulation_project::ProjectDocument previousDocument =
            m_host.m_context.document();
        const bool previousDirty = m_host.m_context.projectSession().isDirty();
        const std::string targetRobotId = targetMount->robotId;
        const std::string targetLinkName = targetMount->linkName;
        const std::string targetMountStableId = targetMount->id;
        const std::string attachmentAssetId = attachment->assetId;
        simulation_project::RebindFramesRequest request;
        request.attachmentId = attachmentId.toStdString();
        request.hostFrame.kind = simulation_project::ProjectFrameKind::RobotMount;
        request.hostFrame.owner.kind = simulation_project::ProjectEntityKind::RobotMount;
        request.hostFrame.owner.id = targetMountStableId;
        request.hostFrame.frameId = targetMountStableId;
        request.hostToBoundOffset = attachment->mountToAssetMount;
        const ProjectMutationResult mutationResult =
            m_host.m_context.documentController().executeRebindFrames(
                QStringLiteral("rebindMountedDevice"),
                request);
        if(!mutationResult.success) {
            emit m_host.statusMessageRequested(
                QString("Rebind failed: %1").arg(mutationResult.message), 5000);
            return;
        }

        const ToolSetupViewportReloadResult reloadResult =
            m_host.m_appServices.reloadViewport(QStringLiteral("rebindMountedDevice"));
        if(!reloadResult.success) {
            m_host.m_context.documentController().restoreProjectSnapshot(
                QStringLiteral("rebindMountedDeviceRollback"),
                previousDocument,
                previousDirty,
                false);
            m_host.m_appServices.reloadViewport(
                QStringLiteral("rebindMountedDeviceRollback"));
            emit m_host.statusMessageRequested(
                "Rebind failed: viewport reload failed.", 8000);
            return;
        }

        const QString robotId = QString::fromStdString(targetRobotId);
        const QString linkName = QString::fromStdString(targetLinkName);
        m_host.m_context.selectionModel().selectMountedAttachment(
            attachmentId,
            robotId,
            linkName,
            targetMountId,
            QStringLiteral("rebindMountedDevice"));
        RobotQtViewerViewportPreviewPayload preview;
        preview.setActiveMountedAttachment = true;
        preview.activeMountedAttachmentId = attachmentId;
        m_host.mutateViewportPreview(preview, QStringLiteral("rebindMountedDevice"));

        RobotQtViewerAttachmentPayload payload;
        payload.mountId = targetMountId;
        payload.attachmentId = attachmentId;
        payload.assetId = QString::fromStdString(attachmentAssetId);
        m_host.m_context.documentController().publishAttachmentChanged(
            payload, QStringLiteral("rebindMountedDevice"));
        m_host.m_context.documentController().publishDocumentChanged(
            QStringLiteral("rebindMountedDevice"), false);
        m_host.refresh(robotId, linkName, targetMountId, attachmentId, payload.assetId);
        emit m_host.selectionDependentViewsRefreshRequested();
        emit m_host.statusMessageRequested(
            QString(
                "Moved installed device to %1; instance identity and offset were preserved.")
                .arg(targetMountId),
            5000);
    }

    void ToolSetupInstalledDeviceTaskController::configureCurrent()
    {
        const QString attachmentId = m_host.m_widget.currentAttachmentId();
        const simulation_project::MountedAttachmentDesc* attachment =
            findMountedAttachmentDesc(
                m_host.m_context.document(), attachmentId.toStdString());
        if(attachment == nullptr) {
            emit m_host.statusMessageRequested("Select an attachment first.", 3000);
            return;
        }

        m_host.m_widget.focusAttachmentInstanceEditor();
        emit m_host.statusMessageRequested(
            QString("Editing attachment in task panel: %1")
                .arg(conciseUiText(attachment->id, 48)),
            3000);
    }

    void ToolSetupInstalledDeviceTaskController::selectById(
        const std::string& attachmentId)
    {
        const simulation_project::MountedAttachmentDesc* attachment =
            findMountedAttachmentDesc(m_host.m_context.document(), attachmentId);
        const simulation_project::RobotMountDesc* mount = attachment != nullptr
            ? findRobotMountDesc(
                m_host.m_context.document(), attachment->mountFrameId)
            : nullptr;
        if(mount == nullptr) {
            return;
        }

        const QString robotId = QString::fromStdString(mount->robotId);
        const QString linkName = QString::fromStdString(mount->linkName);
        const QString mountId = QString::fromStdString(mount->id);
        const QString attachmentIdText = QString::fromStdString(attachmentId);
        m_host.m_context.selectionModel().selectMountedAttachment(
            attachmentIdText,
            robotId,
            linkName,
            mountId,
            QStringLiteral("selectToolAttachmentById"));
        if(m_host.m_context.selectionModel().payload().attachmentId !=
            attachmentIdText) {
            return;
        }
        m_host.m_appServices.setToolAttachmentContext(
            robotId, linkName, mountId, attachmentIdText);
        emit m_host.robotContextSelected(robotId);
        emit m_host.selectionDependentViewsRefreshRequested();
        m_host.refresh(robotId, linkName, mountId, attachmentIdText);
        RobotQtViewerViewportPreviewPayload preview;
        preview.setActiveMountedAttachment = true;
        preview.activeMountedAttachmentId = attachmentIdText;
        m_host.mutateViewportPreview(
            preview, QStringLiteral("selectToolAttachmentById"));
    }

    void ToolSetupInstalledDeviceTaskController::unbind(
        const QString& attachmentId)
    {
        if(attachmentId.isEmpty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral("No mounted attachment selected."), 3000);
            return;
        }
        if(!m_host.resolvePendingTaskChanges(&m_host.m_widget)) {
            emit m_host.statusMessageRequested(
                QStringLiteral("Finish or cancel the current task first."), 3000);
            return;
        }

        const simulation_project::MountedAttachmentDesc* attachment =
            findMountedAttachmentDesc(
                m_host.m_context.document(), attachmentId.toStdString());
        if(attachment == nullptr) {
            emit m_host.statusMessageRequested(
                QString("Mounted attachment not found: %1").arg(attachmentId), 5000);
            return;
        }

        const std::string mountId = attachment->mountFrameId;
        const std::string assetId = attachment->assetId;
        const std::string attachmentName =
            attachment->name.empty() ? attachment->id : attachment->name;
        const QMessageBox::StandardButton choice = QMessageBox::question(
            &m_host.m_widget,
            QStringLiteral("Unbind Attachment"),
            QString("Unbind attachment %1 from its mount frame?")
                .arg(QString::fromStdString(attachmentName)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if(choice != QMessageBox::Yes) {
            return;
        }

        const simulation_project::ProjectDocument previousDocument =
            m_host.m_context.document();
        const bool previousDirty = m_host.m_context.projectSession().isDirty();
        bool assetRemoved = false;
        const ProjectMutationResult mutationResult =
            m_host.m_context.documentController().mutateProject(
                QStringLiteral("unbindMountedAttachment"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,
                    bool& changed,
                    std::string& error) {
                    simulation_project::AttachmentUnbindRequest request;
                    request.attachmentId = attachmentId.toStdString();
                    request.removeUnusedAsset = true;
                    simulation_project::ProjectAttachmentCommands commands(
                        service.document());
                    const simulation_project::AttachmentUnbindResult commandResult =
                        commands.unbindAttachment(request);
                    if(!commandResult.success) {
                        error = commandResult.message;
                        return false;
                    }
                    assetRemoved = commandResult.assetRemoved;
                    changed = commandResult.projectChanged;
                    return true;
                });
        if(!mutationResult.success) {
            emit m_host.statusMessageRequested(
                QString("Unbind attachment failed: %1")
                    .arg(mutationResult.message),
                5000);
            return;
        }

        const ToolSetupViewportReloadResult reloadResult =
            m_host.m_appServices.reloadViewport(
                QStringLiteral("unbindMountedAttachment"));
        if(!reloadResult.success) {
            m_host.m_context.documentController().restoreProjectSnapshot(
                QStringLiteral("unbindMountedAttachmentRollback"),
                previousDocument,
                previousDirty,
                false);
            m_host.m_appServices.reloadViewport(
                QStringLiteral("unbindMountedAttachmentRollback"));
            emit m_host.statusMessageRequested(
                reloadResult.errorMessage.isEmpty()
                    ? QStringLiteral(
                        "Unbind attachment failed: viewport reload failed.")
                    : reloadResult.errorMessage,
                8000);
            return;
        }

        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(m_host.m_context.document(), mountId);
        if(mount != nullptr) {
            m_host.m_context.selectionModel().selectRobotMount(
                QString::fromStdString(mount->robotId),
                QString::fromStdString(mount->linkName),
                QString::fromStdString(mount->id),
                QStringLiteral("unbindMountedAttachment"));
        }

        RobotQtViewerAttachmentPayload payload;
        payload.mountId = QString::fromStdString(mountId);
        payload.attachmentId = attachmentId;
        payload.assetId = QString::fromStdString(assetId);
        m_host.m_context.documentController().publishAttachmentChanged(
            payload, QStringLiteral("unbindMountedAttachment"));
        m_host.m_context.documentController().publishDocumentChanged(
            QStringLiteral("unbindMountedAttachment"), false);
        emit m_host.selectionDependentViewsRefreshRequested();
        emit m_host.statusMessageRequested(
            assetRemoved
                ? QString("Unbound attachment %1 and removed unused asset.")
                    .arg(attachmentId)
                : QString("Unbound attachment %1.").arg(attachmentId),
            5000);
    }
}
