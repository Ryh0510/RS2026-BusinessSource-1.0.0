#include "ToolSetupTaskControllers.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportPreviewState.h"
#include "ToolSetupAppServices.h"
#include "ToolSetupModuleController.h"
#include "ToolSetupTaskSupport.h"
#include "ToolSetupWidget.h"

#include <SimulationProject/ProjectAttachmentCommands.h>
#include <SimulationProject/ProjectDocumentService.h>

namespace robot_qt_viewer
{
    using namespace tool_setup_detail;

    ToolSetupMountFrameTaskController::ToolSetupMountFrameTaskController(
        ToolSetupModuleController& host,
        QObject* parent)
        : QObject(parent)
        , m_host(host)
    {
    }

    void ToolSetupMountFrameTaskController::handleSelectionChanged(int index)
    {
        if(m_host.m_updating || index < 0) {
            return;
        }

        const QString mountId = m_host.m_widget.mountIdAt(index);
        if(m_host.m_taskSession->dirty() && mountId != m_activeEditId &&
            !m_host.m_context.selectionModel().prepareSelectionChange(
                QStringLiteral("toolSetupMountSelection"))) {
            m_host.refresh(
                m_host.m_appServices.selectedRobotId(),
                m_host.m_appServices.selectedLinkName(),
                m_activeEditId);
            return;
        }
        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(m_host.m_context.document(), mountId.toStdString());
        if(mount != nullptr) {
            m_mode = ToolSetupMountFrameMode::Selection;
            const QString robotId = QString::fromStdString(mount->robotId);
            const QString linkName = QString::fromStdString(mount->linkName);
            m_host.m_context.selectionModel().selectRobotMount(
                robotId,
                linkName,
                mountId,
                QStringLiteral("toolSetup"));
            const RobotQtViewerSelectionPayload committed =
                m_host.m_context.selectionModel().payload();
            if(committed.robotId != robotId || committed.linkName != linkName ||
                committed.mountId != mountId) {
                m_host.refresh(
                    m_host.m_appServices.selectedRobotId(),
                    m_host.m_appServices.selectedLinkName(),
                    m_activeEditId);
                return;
            }
            m_host.m_appServices.setRobotMountContext(robotId, linkName, mountId);
            RobotQtViewerViewportPreviewPayload preview;
            preview.setActivePreviewRobotMount = true;
            preview.activePreviewRobotMountId = QString();
            preview.setRobotMountFrameVisibility = true;
            preview.selectedLinkFrameVisible = false;
            preview.mountFrameVisible = false;
            m_host.mutateViewportPreview(preview, QStringLiteral("toolSetupMountSelection"));
            emit m_host.robotContextSelected(robotId);
            emit m_host.selectionDependentViewsRefreshRequested();
        }
        if(!mountId.isEmpty()) {
            emit m_host.statusMessageRequested(
                QString("Selected mount frame: %1").arg(mountId),
                3000);
        }
        m_host.refresh(
            m_host.m_appServices.selectedRobotId(),
            m_host.m_appServices.selectedLinkName(),
            mountId);
    }

    void ToolSetupMountFrameTaskController::focusTask(
        const QString& robotId,
        const QString& linkName,
        const QString& preferredMountId)
    {
        if(robotId.isEmpty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral("Select a robot before opening mount setup."),
                3000);
            return;
        }

        QString mountId = preferredMountId;
        const std::string robotIdValue = robotId.toStdString();
        const std::string linkNameValue = linkName.toStdString();
        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(m_host.m_context.document(), mountId.toStdString());
        if((mount == nullptr || mount->robotId != robotIdValue) && !linkName.isEmpty()) {
            for(const simulation_project::RobotMountDesc& candidate :
                m_host.m_context.document().robotMounts) {
                if(candidate.robotId == robotIdValue && candidate.linkName == linkNameValue) {
                    mount = &candidate;
                    mountId = QString::fromStdString(candidate.id);
                    break;
                }
            }
        }

        if(mount != nullptr && mount->robotId == robotIdValue) {
            m_mode = ToolSetupMountFrameMode::Edit;
            const QString mountLinkName = QString::fromStdString(mount->linkName);
            m_host.m_appServices.setRobotMountContext(robotId, mountLinkName, mountId);
            m_host.m_context.selectionModel().selectRobotMount(
                robotId,
                mountLinkName,
                mountId,
                QStringLiteral("focusRobotMountTask"));
            RobotQtViewerViewportPreviewPayload preview;
            preview.setActivePreviewRobotMount = true;
            preview.activePreviewRobotMountId = mountId;
            preview.setRobotMountFrameVisibility = true;
            preview.selectedLinkFrameVisible = true;
            preview.mountFrameVisible = true;
            m_host.mutateViewportPreview(preview, QStringLiteral("focusRobotMountTask"));
            enterViewportFocus(robotId, mountLinkName);
            m_host.refresh(robotId, mountLinkName, mountId);
            captureSnapshot(mountId);
            m_host.setTaskDirty(false);
            emit m_host.mountFrameFocusRequested(robotId, mountLinkName, mountId);
        } else {
            m_mode = ToolSetupMountFrameMode::Selection;
            m_host.m_appServices.setRobotMountContext(robotId, linkName, QString());
            if(!linkName.isEmpty()) {
                m_host.m_context.selectionModel().selectRobotLink(
                    robotId,
                    linkName,
                    QStringLiteral("focusRobotMountTask"));
                RobotQtViewerViewportPreviewPayload preview;
                preview.setActivePreviewRobotMount = true;
                preview.activePreviewRobotMountId = QString();
                preview.setRobotMountFrameVisibility = true;
                preview.selectedLinkFrameVisible = true;
                preview.mountFrameVisible = false;
                m_host.mutateViewportPreview(preview, QStringLiteral("focusRobotMountTask"));
                enterViewportFocus(robotId, linkName);
            }
            m_host.refresh(robotId, linkName);
        }

        emit m_host.robotContextSelected(robotId);
        emit m_host.selectionDependentViewsRefreshRequested();
        emit m_host.statusMessageRequested(
            QStringLiteral("Mount setup is ready for mount frame editing."),
            3000);
    }

    void ToolSetupMountFrameTaskController::createForSelectedLink()
    {
        if(m_host.m_taskSession->dirty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral("Apply or cancel mount setup edits before adding a mount frame."),
                4000);
            return;
        }

        const QString selectedRobotId = m_host.m_appServices.selectedRobotId();
        const QString selectedLinkName = m_host.m_appServices.selectedLinkName();
        if(selectedRobotId.isEmpty() || selectedLinkName.isEmpty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral("Select a robot link before adding a mount frame."),
                4000);
            return;
        }

        const std::string robotId = selectedRobotId.toStdString();
        const std::string linkName = selectedLinkName.toStdString();
        if(findRobotDesc(m_host.m_context.document(), robotId) == nullptr) {
            emit m_host.statusMessageRequested(
                QString("Robot not found: %1").arg(selectedRobotId),
                4000);
            return;
        }

        m_draftSourceRobotId = selectedRobotId;
        m_draftSourceLinkName = selectedLinkName;
        m_mode = ToolSetupMountFrameMode::Create;

        simulation_project::RobotMountDesc mount;
        const simulation_project::ProjectDocumentService service(m_host.m_context.document());
        mount.id = service.makeUniqueId(robotId + "_" + linkName + "_mount");
        mount.name = mount.id;
        mount.robotId = robotId;
        mount.linkName = linkName;

        const QString mountId = QString::fromStdString(mount.id);
        m_host.m_appServices.setRobotMountContext(selectedRobotId, selectedLinkName, mountId);
        m_hasSnapshot = true;
        m_snapshotIsNew = true;
        m_activeEditId = mountId;
        m_snapshot = mount;
        RobotQtViewerViewportPreviewPayload preview;
        preview.upsertPreviewRobotMount = true;
        preview.robotMount = mount;
        preview.setActivePreviewRobotMount = true;
        preview.activePreviewRobotMountId = mountId;
        preview.setRobotMountFrameVisibility = true;
        preview.selectedLinkFrameVisible = true;
        preview.mountFrameVisible = true;
        m_host.mutateViewportPreview(preview, QStringLiteral("createRobotMountForSelectedLink"));
        enterViewportFocus(selectedRobotId, selectedLinkName);
        m_host.refresh(selectedRobotId, selectedLinkName, mountId);
        emit m_host.robotContextSelected(selectedRobotId);
        emit m_host.mountFrameFocusRequested(selectedRobotId, selectedLinkName, mountId);
        emit m_host.selectionDependentViewsRefreshRequested();
        m_host.setTaskDirty(true, QString("New mount frame is pending: %1").arg(mountId));
        RobotQtViewerViewportPreviewPayload editorPreview;
        editorPreview.setActivePreviewRobotMount = true;
        editorPreview.activePreviewRobotMountId = mountId;
        editorPreview.setRobotMountFrameVisibility = true;
        editorPreview.selectedLinkFrameVisible = true;
        editorPreview.mountFrameVisible = true;
        m_host.mutateViewportPreview(
            editorPreview,
            QStringLiteral("createRobotMountForSelectedLinkEditor"));
        enterViewportFocus(selectedRobotId, selectedLinkName);
        emit m_host.statusMessageRequested(
            QString("Created mount frame draft: %1").arg(mountId),
            4000);
    }

    void ToolSetupMountFrameTaskController::deleteCurrent()
    {
        if(m_host.m_taskSession->dirty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral("Apply or cancel mount setup edits before deleting a mount frame."),
                4000);
            return;
        }

        const QString selectedMountId = m_host.m_widget.currentMountId();
        if(selectedMountId.isEmpty()) {
            emit m_host.statusMessageRequested(
                QStringLiteral("Select a mount frame before deleting."),
                3000);
            return;
        }

        const simulation_project::RobotMountDesc* selectedMount =
            findRobotMountDesc(m_host.m_context.document(), selectedMountId.toStdString());
        if(selectedMount == nullptr && m_snapshotIsNew && m_hasSnapshot &&
            selectedMountId == m_activeEditId) {
            selectedMount = &m_snapshot;
        }
        if(selectedMount == nullptr) {
            emit m_host.statusMessageRequested(
                QString("Mount frame not found: %1").arg(selectedMountId),
                4000);
            return;
        }

        const std::string robotId = selectedMount->robotId;
        const std::string linkName = selectedMount->linkName;
        const ProjectMutationResult mutationResult =
            m_host.m_context.documentController().mutateProject(
                QStringLiteral("deleteCurrentRobotMount"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,
                    bool& changed,
                    std::string& error) {
                    simulation_project::ProjectAttachmentCommands commands(service.document());
                    const simulation_project::ProjectCommandResult commandResult =
                        commands.removeRobotMount(selectedMountId.toStdString());
                    if(!commandResult.success) {
                        error = commandResult.message;
                        return false;
                    }
                    changed = commandResult.projectChanged;
                    return true;
                });
        if(!mutationResult.success) {
            emit m_host.statusMessageRequested(
                QString("Delete mount frame failed: %1").arg(mutationResult.message),
                5000);
            return;
        }
        m_pinnedFrameIds.remove(selectedMountId);

        const QString robotIdText = QString::fromStdString(robotId);
        const QString linkNameText = QString::fromStdString(linkName);
        m_host.m_appServices.setRobotMountContext(robotIdText, linkNameText, QString());
        m_host.m_context.selectionModel().selectRobotLink(
            robotIdText,
            linkNameText,
            QStringLiteral("deleteCurrentRobotMount"));
        m_host.m_appServices.reloadViewport(QStringLiteral("deleteCurrentRobotMount"));
        syncPinnedFrames();
        m_host.refresh(robotIdText, linkNameText);
        RobotQtViewerAttachmentPayload payload;
        payload.mountId = selectedMountId;
        m_host.m_context.documentController().publishAttachmentChanged(
            payload,
            QStringLiteral("deleteCurrentRobotMount"));
        m_host.m_context.documentController().publishDocumentChanged(
            QStringLiteral("deleteCurrentRobotMount"),
            false);
        emit m_host.robotContextSelected(robotIdText);
        emit m_host.selectionDependentViewsRefreshRequested();
        emit m_host.statusMessageRequested(
            QString("Deleted mount frame: %1").arg(selectedMountId),
            4000);
    }

    void ToolSetupMountFrameTaskController::captureSnapshot(
        const QString& mountId,
        bool newMount)
    {
        if(mountId.isEmpty()) {
            clearSnapshot();
            return;
        }

        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(m_host.m_context.document(), mountId.toStdString());
        if(mount == nullptr) {
            clearSnapshot();
            return;
        }

        m_hasSnapshot = true;
        m_snapshotIsNew = newMount;
        m_activeEditId = mountId;
        m_snapshot = *mount;
    }

    void ToolSetupMountFrameTaskController::clearSnapshot()
    {
        m_hasSnapshot = false;
        m_snapshotIsNew = false;
        m_activeEditId.clear();
        m_draftSourceRobotId.clear();
        m_draftSourceLinkName.clear();
        m_snapshot = simulation_project::RobotMountDesc();
    }

    void ToolSetupMountFrameTaskController::enterViewportFocus(
        const QString& robotId,
        const QString& linkName)
    {
        if(robotId.isEmpty() || linkName.isEmpty()) {
            return;
        }
        RobotQtViewerViewportPreviewPayload preview;
        preview.focusMountFrameLink = true;
        preview.focusMountFrameRobotId = robotId;
        preview.focusMountFrameLinkName = linkName;
        m_host.mutateViewportPreview(preview, QStringLiteral("toolSetupMountFrameFocus"));
    }

    void ToolSetupMountFrameTaskController::clearViewportFocus()
    {
        RobotQtViewerViewportPreviewPayload preview;
        preview.clearMountFrameLinkFocus = true;
        m_host.mutateViewportPreview(preview, QStringLiteral("toolSetupClearMountFrameFocus"));
    }

    void ToolSetupMountFrameTaskController::updateFrameVisibility()
    {
        const QString currentMountId = m_host.m_widget.currentMountId();
        if(!m_host.m_updating && !currentMountId.isEmpty()) {
            setPinned(currentMountId, m_host.m_widget.showRobotMountFrame());
        }

        RobotQtViewerToolFrameVisibility visibility;
        visibility.link = m_host.m_widget.showLinkFrame();
        visibility.robotMount = m_host.m_widget.showRobotMountFrame();
        visibility.toolMount = m_host.m_widget.showToolMountFrame();
        visibility.visual = m_host.m_widget.showVisualFrame();
        visibility.tcp = m_host.m_widget.showTcpFrame();
        visibility.sensorPreview = m_host.m_widget.showSensorPreview();
        RobotQtViewerViewportPreviewPayload preview;
        preview.setToolFrameVisibility = true;
        preview.toolFrameVisibility = visibility;
        const bool mountFrameTaskActive =
            m_mode == ToolSetupMountFrameMode::Create ||
            m_mode == ToolSetupMountFrameMode::Edit;
        preview.setRobotMountFrameVisibility = true;
        preview.selectedLinkFrameVisible = mountFrameTaskActive;
        preview.mountFrameVisible =
            mountFrameTaskActive || m_host.m_widget.showRobotMountFrame();
        m_host.mutateViewportPreview(preview, QStringLiteral("toolSetupFrameVisibility"));
        syncPinnedFrames();
        emit m_host.frameVisibilityChanged();
    }

    void ToolSetupMountFrameTaskController::syncPinnedFrames()
    {
        RobotQtViewerViewportPreviewPayload preview;
        preview.setPinnedRobotMountFrames = true;
        preview.pinnedRobotMountFrameIds = pinnedFrameIds();
        m_host.mutateViewportPreview(preview, QStringLiteral("toolSetupPinnedMountFrames"));
    }

    QStringList ToolSetupMountFrameTaskController::pinnedFrameIds() const
    {
        QStringList ids;
        for(const QString& id : m_pinnedFrameIds) {
            if(!id.isEmpty() &&
                findRobotMountDesc(m_host.m_context.document(), id.toStdString()) != nullptr) {
                ids.push_back(id);
            }
        }
        ids.sort();
        return ids;
    }

    void ToolSetupMountFrameTaskController::reset() noexcept
    {
        m_mode = ToolSetupMountFrameMode::Selection;
        m_pinnedFrameIds.clear();
        clearSnapshot();
    }

    ToolSetupMountFrameMode ToolSetupMountFrameTaskController::mode() const
    {
        return m_mode;
    }

    void ToolSetupMountFrameTaskController::setMode(ToolSetupMountFrameMode mode)
    {
        m_mode = mode;
    }

    bool ToolSetupMountFrameTaskController::hasSnapshot() const
    {
        return m_hasSnapshot;
    }

    bool ToolSetupMountFrameTaskController::snapshotIsNew() const
    {
        return m_snapshotIsNew;
    }

    const simulation_project::RobotMountDesc& ToolSetupMountFrameTaskController::snapshot() const
    {
        return m_snapshot;
    }

    const QString& ToolSetupMountFrameTaskController::activeEditId() const
    {
        return m_activeEditId;
    }

    const QString& ToolSetupMountFrameTaskController::draftSourceRobotId() const
    {
        return m_draftSourceRobotId;
    }

    const QString& ToolSetupMountFrameTaskController::draftSourceLinkName() const
    {
        return m_draftSourceLinkName;
    }

    bool ToolSetupMountFrameTaskController::isPinned(const QString& mountId) const
    {
        return m_pinnedFrameIds.contains(mountId);
    }

    void ToolSetupMountFrameTaskController::setPinned(const QString& mountId, bool pinned)
    {
        if(pinned) {
            m_pinnedFrameIds.insert(mountId);
        } else {
            m_pinnedFrameIds.remove(mountId);
        }
    }

    void ToolSetupMountFrameTaskController::replacePinnedId(
        const QString& oldId,
        const QString& newId)
    {
        const bool wasPinned = m_pinnedFrameIds.remove(oldId);
        if(wasPinned && !newId.isEmpty()) {
            m_pinnedFrameIds.insert(newId);
        }
    }
}
