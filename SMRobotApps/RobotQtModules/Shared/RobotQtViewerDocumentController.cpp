#include "RobotQtViewerDocumentController.h"

#include <SimulationProject/ProjectAttachmentCommands.h>
#include <SimulationProject/ProjectDocumentService.h>

#include <exception>
#include <utility>

namespace
{
    simulation_project::ProjectChangeOrigin changeOrigin(
        robot_qt_viewer::ProjectDirtyPolicy policy)
    {
        switch(policy) {
        case robot_qt_viewer::ProjectDirtyPolicy::UserEdit:
            return simulation_project::ProjectChangeOrigin::UserEdit;
        case robot_qt_viewer::ProjectDirtyPolicy::LoadNormalization:
            return simulation_project::ProjectChangeOrigin::LoadMigration;
        case robot_qt_viewer::ProjectDirtyPolicy::PreviewOnly:
            return simulation_project::ProjectChangeOrigin::Preview;
        case robot_qt_viewer::ProjectDirtyPolicy::RuntimeOnly:
            return simulation_project::ProjectChangeOrigin::RuntimeOnly;
        }
        return simulation_project::ProjectChangeOrigin::UserEdit;
    }

    simulation_project::ProjectDirtyEffect dirtyEffect(
        robot_qt_viewer::ProjectDirtyPolicy policy)
    {
        return policy == robot_qt_viewer::ProjectDirtyPolicy::UserEdit
            ? simulation_project::ProjectDirtyEffect::MarkDirty
            : simulation_project::ProjectDirtyEffect::Preserve;
    }

    robot_qt_viewer::ProjectMutationResult makeMutationResult(
        const simulation_project::ProjectTransactionResult& transaction)
    {
        robot_qt_viewer::ProjectMutationResult result;
        result.success = transaction.success;
        result.changed = transaction.changed;
        result.message = QString::fromStdString(transaction.message);
        result.hasProjectChange = transaction.changes.documentChanged;
        result.projectChange = transaction.changes;
        return result;
    }
}

namespace robot_qt_viewer
{
    RobotQtViewerDocumentController::RobotQtViewerDocumentController(
        simulation_project::ProjectSession& session,
        RobotQtViewerEventHub& eventHub,
        QObject* parent)
        : QObject(parent)
        , m_session(session)
        , m_eventHub(eventHub)
        , m_transactions(simulation_project::createProjectTransactionService(session))
    {
    }

    simulation_project::ProjectSession& RobotQtViewerDocumentController::session()
    {
        return m_session;
    }

    const simulation_project::ProjectSession& RobotQtViewerDocumentController::session() const
    {
        return m_session;
    }

    simulation_project::ProjectRevision RobotQtViewerDocumentController::revision() const
    {
        return m_transactions->revision();
    }

    const simulation_project::ProjectChangeSet&
        RobotQtViewerDocumentController::lastProjectChange() const
    {
        return m_transactions->lastChange();
    }

    void RobotQtViewerDocumentController::publishProjectOpened(const QString& sourceId)
    {
        const simulation_project::ProjectChangeOrigin origin = m_session.isDirty()
            ? simulation_project::ProjectChangeOrigin::LoadMigration
            : simulation_project::ProjectChangeOrigin::Load;
        const simulation_project::ProjectChangeSet changes =
            m_transactions->recordDocumentReplacement(
                origin,
                {simulation_project::ProjectChangeDomain::Document});
        m_eventHub.publish(makeEvent(RobotQtViewerEventKind::ProjectOpened, sourceId, &changes));
        if(changes.dirtyBefore != changes.dirtyAfter) {
            m_eventHub.publish(makeEvent(
                RobotQtViewerEventKind::ProjectDirtyChanged,
                sourceId,
                &changes));
        }
    }

    void RobotQtViewerDocumentController::publishProjectSaved(const QString& sourceId)
    {
        m_transactions->synchronizeSessionState();
        m_eventHub.publish(makeEvent(RobotQtViewerEventKind::ProjectSaved, sourceId));
        publishDirtyChanged(sourceId);
    }

    void RobotQtViewerDocumentController::publishDocumentChanged(const QString& sourceId, bool markDirty)
    {
        if(markDirty) {
            m_session.markDirty();
        }
        m_eventHub.publish(makeEvent(RobotQtViewerEventKind::ProjectDocumentChanged, sourceId));
        publishDirtyChanged(sourceId);
    }

    ProjectMutationResult RobotQtViewerDocumentController::mutateProject(
        const QString& sourceId,
        ProjectDirtyPolicy dirtyPolicy,
        const ProjectMutation& mutation)
    {
        ProjectMutationResult result;
        if(!mutation) {
            result.message = QStringLiteral("Project mutation is empty.");
            return result;
        }

        simulation_project::ProjectDocument candidate = m_transactions->snapshot();
        simulation_project::ProjectDocumentService service(candidate);
        bool changed = false;
        std::string error;
        bool succeeded = false;
        try {
            succeeded = mutation(service, changed, error);
        } catch(const std::exception& exception) {
            error = exception.what();
        } catch(...) {
            error = "Project mutation threw an unknown exception.";
        }
        if(!succeeded) {
            result.message = error.empty()
                ? QStringLiteral("Project mutation failed.")
                : QString::fromStdString(error);
            return result;
        }

        simulation_project::ProjectTransactionRequest request;
        request.candidate = std::move(candidate);
        request.origin = changeOrigin(dirtyPolicy);
        request.dirtyEffect = dirtyEffect(dirtyPolicy);
        request.domains = {simulation_project::ProjectChangeDomain::Document};
        request.changed = changed;
        const simulation_project::ProjectTransactionResult transaction =
            m_transactions->execute(std::move(request));
        result = makeMutationResult(transaction);
        if(result.success && result.changed) {
            publishCommittedChange(sourceId, result.projectChange);
        }
        return result;
    }

    ProjectBindFramesMutationResult RobotQtViewerDocumentController::executeBindFrames(
        const QString& sourceId,
        const simulation_project::BindFramesRequest& request)
    {
        ProjectBindFramesMutationResult result;
        simulation_project::ProjectDocument candidate = m_transactions->snapshot();
        simulation_project::ProjectAttachmentCommands commands(candidate);
        result.command = commands.bindFrames(request);
        if(!result.command.success) {
            result.transaction.message = QString::fromStdString(result.command.message);
            return result;
        }

        simulation_project::ProjectTransactionRequest transactionRequest;
        transactionRequest.candidate = std::move(candidate);
        transactionRequest.origin = simulation_project::ProjectChangeOrigin::UserEdit;
        transactionRequest.dirtyEffect = simulation_project::ProjectDirtyEffect::MarkDirty;
        transactionRequest.domains = {simulation_project::ProjectChangeDomain::Assembly};
        transactionRequest.affectedIds = {
            result.command.assetId,
            result.command.attachmentId,
            request.hostFrame.owner.id,
            request.boundEntity.id};
        transactionRequest.changed = result.command.projectChanged;
        const simulation_project::ProjectTransactionResult transaction =
            m_transactions->execute(std::move(transactionRequest));
        result.transaction = makeMutationResult(transaction);
        if(result.transaction.success && result.transaction.changed) {
            publishCommittedChange(sourceId, result.transaction.projectChange);
        }
        return result;
    }

    ProjectMutationResult RobotQtViewerDocumentController::executeRebindFrames(
        const QString& sourceId,
        const simulation_project::RebindFramesRequest& request)
    {
        simulation_project::ProjectDocument candidate = m_transactions->snapshot();
        simulation_project::ProjectAttachmentCommands commands(candidate);
        const simulation_project::ProjectCommandResult command = commands.rebindFrames(request);
        if(!command.success) {
            ProjectMutationResult result;
            result.message = QString::fromStdString(command.message);
            return result;
        }

        simulation_project::ProjectTransactionRequest transactionRequest;
        transactionRequest.candidate = std::move(candidate);
        transactionRequest.origin = simulation_project::ProjectChangeOrigin::UserEdit;
        transactionRequest.dirtyEffect = simulation_project::ProjectDirtyEffect::MarkDirty;
        transactionRequest.domains = {simulation_project::ProjectChangeDomain::Assembly};
        transactionRequest.affectedIds = {
            request.attachmentId,
            request.hostFrame.owner.id};
        transactionRequest.changed = command.projectChanged;
        const simulation_project::ProjectTransactionResult transaction =
            m_transactions->execute(std::move(transactionRequest));
        ProjectMutationResult result = makeMutationResult(transaction);
        if(result.success && result.changed) {
            publishCommittedChange(sourceId, result.projectChange);
        }
        return result;
    }

    void RobotQtViewerDocumentController::restoreProjectSnapshot(
        const QString& sourceId,
        simulation_project::ProjectDocument document,
        bool dirty,
        bool publishDocumentChanged)
    {
        const bool previousDirty = m_session.isDirty();
        simulation_project::ProjectTransactionRequest request;
        request.candidate = std::move(document);
        request.origin = simulation_project::ProjectChangeOrigin::CompatibilityRestore;
        request.dirtyEffect = dirty
            ? simulation_project::ProjectDirtyEffect::MarkDirty
            : simulation_project::ProjectDirtyEffect::Clear;
        request.domains = {simulation_project::ProjectChangeDomain::Document};
        const simulation_project::ProjectTransactionResult transaction =
            m_transactions->execute(std::move(request));
        if(!transaction.success) {
            return;
        }
        if(publishDocumentChanged && transaction.changed) {
            m_eventHub.publish(makeEvent(
                RobotQtViewerEventKind::ProjectDocumentChanged,
                sourceId,
                &transaction.changes));
        }
        if(previousDirty != m_session.isDirty() || publishDocumentChanged) {
            m_eventHub.publish(makeEvent(
                RobotQtViewerEventKind::ProjectDirtyChanged,
                sourceId,
                &transaction.changes));
        }
    }

    void RobotQtViewerDocumentController::publishDirtyChanged(const QString& sourceId)
    {
        m_eventHub.publish(makeEvent(RobotQtViewerEventKind::ProjectDirtyChanged, sourceId));
    }

    void RobotQtViewerDocumentController::publishViewportReloadRequested(const QString& sourceId)
    {
        RobotQtViewerEvent event = makeEvent(RobotQtViewerEventKind::ViewportReloadRequested, sourceId);
        event.viewportReloadRequested = true;
        event.viewport.reloadRequested = true;
        m_eventHub.publish(event);
    }

    void RobotQtViewerDocumentController::publishViewportReloaded(bool succeeded, const QString& sourceId)
    {
        RobotQtViewerEvent event = makeEvent(RobotQtViewerEventKind::ViewportReloaded, sourceId);
        event.viewport.reloadSucceeded = succeeded;
        m_eventHub.publish(event);
    }

    void RobotQtViewerDocumentController::publishRobotRuntimeChanged(const QString& sourceId)
    {
        m_eventHub.publish(makeEvent(RobotQtViewerEventKind::RobotRuntimeChanged, sourceId));
    }

    void RobotQtViewerDocumentController::publishAttachmentChanged(
        const RobotQtViewerAttachmentPayload& attachment,
        const QString& sourceId)
    {
        RobotQtViewerEvent event = makeEvent(RobotQtViewerEventKind::AttachmentChanged, sourceId);
        event.attachment = attachment;
        m_eventHub.publish(event);
    }

    void RobotQtViewerDocumentController::publishCollisionChanged(
        const RobotQtViewerCollisionPayload& collision,
        const QString& sourceId)
    {
        RobotQtViewerEvent event = makeEvent(RobotQtViewerEventKind::CollisionChanged, sourceId);
        event.collision = collision;
        m_eventHub.publish(event);
    }

    void RobotQtViewerDocumentController::publishCoatingAnalysisChanged(
        const RobotQtViewerCoatingAnalysisPayload& coatingAnalysis,
        const QString& sourceId)
    {
        RobotQtViewerEvent event = makeEvent(RobotQtViewerEventKind::CoatingAnalysisChanged, sourceId);
        event.coatingAnalysis = coatingAnalysis;
        m_eventHub.publish(event);
    }

    void RobotQtViewerDocumentController::publishSelectionChanged(
        const RobotQtViewerSelectionPayload& selection,
        const QString& sourceId)
    {
        RobotQtViewerEvent event = makeEvent(RobotQtViewerEventKind::SelectionChanged, sourceId);
        event.selection = selection;
        m_eventHub.publish(event);
    }

    void RobotQtViewerDocumentController::publishStatusMessage(
        const QString& message,
        int timeoutMs,
        const QString& sourceId)
    {
        RobotQtViewerEvent event = makeEvent(RobotQtViewerEventKind::StatusMessageRequested, sourceId);
        event.message = message;
        event.timeoutMs = timeoutMs;
        m_eventHub.publish(event);
    }

    void RobotQtViewerDocumentController::publishCommittedChange(
        const QString& sourceId,
        const simulation_project::ProjectChangeSet& changes)
    {
        m_eventHub.publish(makeEvent(
            RobotQtViewerEventKind::ProjectDocumentChanged,
            sourceId,
            &changes));
        if(changes.dirtyBefore != changes.dirtyAfter) {
            m_eventHub.publish(makeEvent(
                RobotQtViewerEventKind::ProjectDirtyChanged,
                sourceId,
                &changes));
        }
    }

    RobotQtViewerEvent RobotQtViewerDocumentController::makeEvent(
        RobotQtViewerEventKind kind,
        const QString& sourceId,
        const simulation_project::ProjectChangeSet* changes) const
    {
        RobotQtViewerEvent event;
        event.kind = kind;
        event.sourceId = sourceId;
        event.projectChanged = m_session.isDirty();
        event.projectDirty = m_session.isDirty();
        if(changes != nullptr) {
            event.hasProjectChange = true;
            event.projectChange = *changes;
            for(const std::string& id : changes->affectedIds) {
                event.affectedIds.push_back(QString::fromStdString(id));
            }
        }
        return event;
    }
}
