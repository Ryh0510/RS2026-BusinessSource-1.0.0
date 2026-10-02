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

    ToolSetupObjectBindingTaskController::ToolSetupObjectBindingTaskController(
        ToolSetupModuleController& host,
        QObject* parent)
        : QObject(parent)
        , m_host(host)
    {
    }

    void ToolSetupObjectBindingTaskController::focusTask(
        const QString& preferredMountId,
        const QString& preferredObjectId,
        const QString& preferredFrameId)
    {
        if(m_host.hasPendingTaskChanges() &&
            !m_host.resolvePendingTaskChanges(&m_host.m_widget)) {
            return;
        }

        m_hasDraft = false;
        m_draft = simulation_project::BindFramesRequest();
        m_active = true;

        m_mountId = preferredMountId;
        if(m_mountId.isEmpty()) {
            m_mountId = m_host.m_widget.currentMountId();
        }
        m_objectId = preferredObjectId;
        m_frameId = preferredFrameId;

        m_host.m_widget.setObjectBindingMode(true);
        refreshEditor(m_mountId, m_objectId, m_frameId);
        m_host.setTaskDirty(false, QStringLiteral("Select binding targets."));
        emit m_host.statusMessageRequested(QStringLiteral("Object binding task is ready."), 3000);
    }

    void ToolSetupObjectBindingTaskController::handleSelectionChanged(
        const QString& mountId,
        const QString& objectId,
        const QString& frameId)
    {
        if(!m_active) {
            return;
        }
        m_mountId = mountId;
        m_objectId = objectId;
        m_frameId = frameId;
        refreshEditor(mountId, objectId, frameId);
        const bool previewReady = preview(mountId, objectId, frameId);
        m_host.setTaskDirty(previewReady, previewReady
            ? QStringLiteral("Object binding preview is pending.")
            : QStringLiteral("Select valid binding targets."));
    }

    bool ToolSetupObjectBindingTaskController::preview(
        const QString& mountId,
        const QString& objectId,
        const QString& frameId,
        QString* attachmentId,
        QString* assetId)
    {
        if(attachmentId != nullptr) {
            attachmentId->clear();
        }
        if(assetId != nullptr) {
            assetId->clear();
        }
        if(!m_active) {
            return false;
        }

        m_draft = simulation_project::BindFramesRequest();
        m_draft.hostFrame.kind = simulation_project::ProjectFrameKind::RobotMount;
        m_draft.hostFrame.owner.kind = simulation_project::ProjectEntityKind::RobotMount;
        m_draft.hostFrame.owner.id = mountId.toStdString();
        m_draft.hostFrame.frameId = mountId.toStdString();
        m_draft.boundEntity.kind = simulation_project::ProjectEntityKind::SceneObject;
        m_draft.boundEntity.id = objectId.toStdString();
        m_draft.boundFrame.kind = frameId.isEmpty()
            ? simulation_project::ProjectFrameKind::ObjectOrigin
            : simulation_project::ProjectFrameKind::ObjectFrame;
        m_draft.boundFrame.owner = m_draft.boundEntity;
        m_draft.boundFrame.frameId = frameId.toStdString();
        m_draft.mode = simulation_project::BindingMode::AlignFrames;
        m_draft.sourcePolicy =
            simulation_project::SourceEntityPolicy::HideAndDisableCollision;

        simulation_project::ProjectDocument previewDocument = m_host.m_context.document();
        simulation_project::ProjectAttachmentCommands previewCommands(previewDocument);
        const simulation_project::BindFramesResult previewResult =
            previewCommands.bindFrames(m_draft);
        if(!previewResult.success) {
            m_hasDraft = false;
            RobotQtViewerViewportPreviewPayload clearPayload;
            clearPayload.clearAttachmentBindingPreview = true;
            m_host.mutateViewportPreview(
                clearPayload,
                QStringLiteral("previewObjectBindingInvalid"));
            return false;
        }
        m_hasDraft = true;

        const QString attachmentIdText =
            QString::fromStdString(previewResult.attachmentId);
        RobotQtViewerViewportPreviewPayload previewPayload;
        previewPayload.previewAttachmentBinding = true;
        previewPayload.attachmentBinding = m_draft;
        previewPayload.setActiveMountedAttachment = true;
        previewPayload.activeMountedAttachmentId = attachmentIdText;
        previewPayload.setRobotMountFrameVisibility = true;
        previewPayload.selectedLinkFrameVisible = true;
        previewPayload.mountFrameVisible = true;
        m_host.mutateViewportPreview(previewPayload, QStringLiteral("previewObjectBinding"));
        if(attachmentId != nullptr) {
            *attachmentId = attachmentIdText;
        }
        if(assetId != nullptr) {
            *assetId = QString::fromStdString(previewResult.assetId);
        }
        return true;
    }

    void ToolSetupObjectBindingTaskController::refreshEditor(
        const QString& mountId,
        const QString& objectId,
        const QString& frameId)
    {
        const simulation_project::ProjectDocument& document = m_host.m_context.document();

        QVector<ToolSetupComboItem> objectItems;
        objectItems.reserve(static_cast<int>(document.objects.size()));
        objectItems.push_back(makeComboItem(QStringLiteral("Select object..."), QString()));
        for(const simulation_project::SceneObjectDesc& object : document.objects) {
            if(object.sourcePath.empty() || !object.visible) {
                continue;
            }
            const QString id = QString::fromStdString(object.id);
            const QString name = QString::fromStdString(object.name.empty() ? object.id : object.name);
            objectItems.push_back(makeComboItem(
                name == id ? id : QString("%1 [%2]").arg(name, conciseUiText(object.id, 32)),
                id));
        }

        QVector<ToolSetupComboItem> frameItems;
        const simulation_project::SceneObjectDesc* object =
            findSceneObjectDesc(document, objectId.toStdString());
        if(object != nullptr) {
            frameItems.push_back(makeComboItem(QStringLiteral("Object origin"), QString()));
            for(const simulation_project::ObjectFrameDesc& frame : object->objectFrames) {
                const QString id = QString::fromStdString(frame.id);
                const QString name = QString::fromStdString(frame.name.empty() ? frame.id : frame.name);
                frameItems.push_back(makeComboItem(
                    name == id ? id : QString("%1 [%2]").arg(name, conciseUiText(frame.id, 32)),
                    id));
            }
        }

        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(document, mountId.toStdString());
        const bool applyEnabled = mount != nullptr && object != nullptr && !object->sourcePath.empty();
        ToolSetupBindingView bindingView = makeBindingView(document, mountId, objectId, frameId);
        bindingView.editable = true;
        m_host.m_widget.setObjectBindingMode(true);
        m_host.m_widget.setObjectBindingEditor(
            true,
            bindingView,
            QVector<ToolSetupComboItem>(),
            mountId,
            objectItems,
            objectId,
            frameItems,
            frameId,
            QString(),
            applyEnabled);
    }

    void ToolSetupObjectBindingTaskController::apply(
        const QString& mountId,
        const QString& objectId,
        const QString& frameId)
    {
        if(!m_active) {
            return;
        }

        QString attachmentId;
        QString assetId;
        if(!preview(mountId, objectId, frameId, &attachmentId, &assetId)) {
            refreshEditor(mountId, objectId, frameId);
            return;
        }

        const ProjectBindFramesMutationResult mutationResult =
            m_host.m_context.documentController().executeBindFrames(
                QStringLiteral("applyObjectBinding"),
                m_draft);
        if(!mutationResult.transaction.success) {
            emit m_host.statusMessageRequested(
                QString("Apply object binding failed: %1").arg(mutationResult.transaction.message),
                5000);
            return;
        }
        attachmentId = QString::fromStdString(mutationResult.command.attachmentId);
        assetId = QString::fromStdString(mutationResult.command.assetId);
        const ToolSetupViewportReloadResult reloadResult =
            m_host.m_appServices.reloadViewport(QStringLiteral("applyObjectBinding"));
        if(!reloadResult.success) {
            emit m_host.statusMessageRequested(
                reloadResult.errorMessage.isEmpty()
                    ? QStringLiteral("Object binding was saved, but viewport reload failed.")
                    : reloadResult.errorMessage,
                8000);
        }
        reset();
        m_host.m_mountFrameTask->setMode(ToolSetupMountFrameMode::Selection);
        m_host.clearMountEditSnapshot();
        m_host.m_widget.setObjectBindingMode(false);
        m_host.setTaskDirty(false, QStringLiteral("Object binding applied."));
        RobotQtViewerAttachmentPayload payload;
        payload.mountId = mountId;
        payload.attachmentId = attachmentId;
        payload.assetId = assetId;
        m_host.m_context.documentController().publishAttachmentChanged(
            payload,
            QStringLiteral("applyObjectBinding"));
        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(m_host.m_context.document(), mountId.toStdString());
        if(mount != nullptr) {
            const QString robotId = QString::fromStdString(mount->robotId);
            const QString linkName = QString::fromStdString(mount->linkName);
            m_host.m_appServices.setRobotMountContext(robotId, linkName, mountId);
            m_host.m_context.selectionModel().selectRobotMount(
                robotId,
                linkName,
                mountId,
                QStringLiteral("applyObjectBinding"));
            m_host.refresh(robotId, linkName, mountId, attachmentId, assetId);
            emit m_host.robotContextSelected(robotId);
            emit m_host.mountFrameFocusRequested(robotId, linkName, mountId);
            emit m_host.selectionDependentViewsRefreshRequested();
        }
        clearPreview();
        emit m_host.statusMessageRequested(
            QString("Bound object %1 to mount %2.").arg(objectId, mountId),
            5000);
    }

    void ToolSetupObjectBindingTaskController::clearPreview()
    {
        m_host.m_context.viewportPreviewState().clearTaskPreview(
            QStringLiteral("clearObjectBindingPreviewViewportState"));
    }

    void ToolSetupObjectBindingTaskController::reset() noexcept
    {
        m_hasDraft = false;
        m_active = false;
        m_mountId.clear();
        m_objectId.clear();
        m_frameId.clear();
        m_draft = simulation_project::BindFramesRequest();
    }

    bool ToolSetupObjectBindingTaskController::active() const
    {
        return m_active;
    }

    bool ToolSetupObjectBindingTaskController::hasDraft() const
    {
        return m_hasDraft;
    }

    const QString& ToolSetupObjectBindingTaskController::mountId() const
    {
        return m_mountId;
    }

    const QString& ToolSetupObjectBindingTaskController::objectId() const
    {
        return m_objectId;
    }

    const QString& ToolSetupObjectBindingTaskController::frameId() const
    {
        return m_frameId;
    }
}
