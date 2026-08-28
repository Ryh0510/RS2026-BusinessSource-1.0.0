#include "RobotQtViewerWorkbenchTransitionCoordinator.h"

#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerEvents.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <CustomLog/CustomLog.h>

#include <QElapsedTimer>

#include <exception>
#include <set>

namespace robot_qt_viewer
{
    namespace
    {
        class TransitionGuard
        {
        public:
            explicit TransitionGuard(bool& active)
                : m_active(active)
            {
                m_active = true;
            }

            ~TransitionGuard()
            {
                m_active = false;
            }

        private:
            bool& m_active;
        };

        QString exceptionMessage(const char* phase, const std::exception& exception)
        {
            return QStringLiteral("Workbench lifecycle %1 threw an exception: %2")
                .arg(QString::fromLatin1(phase), QString::fromUtf8(exception.what()));
        }

        QString workbenchDisplayName(
            const RobotQtViewerWorkbenchPackageRegistry& registry,
            const QString& workbenchId)
        {
            const RobotQtViewerWorkbenchDesc* mode = registry.registeredWorkbench(workbenchId);
            return mode == nullptr ? workbenchId : mode->descriptor.displayName;
        }
    }

    RobotQtViewerWorkbenchTransitionCoordinator::RobotQtViewerWorkbenchTransitionCoordinator(
        RobotQtViewerWorkbenchPackageRegistry& registry,
        RobotQtViewerWorkbenchManager& manager,
        RobotQtViewerEventHub& eventHub)
        : m_registry(registry)
        , m_manager(manager)
        , m_eventHub(eventHub)
    {
        for(const RobotQtViewerWorkbenchDesc& mode : m_registry.workbenches()) {
            m_states[mode.descriptor.id] = RobotQtViewerWorkbenchLifecycleState::Inactive;
        }
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::initializeActiveWorkbench(
        const QString& sourceId)
    {
        return activateCommittedWorkbench(
            RobotQtViewerWorkbenchActivationKind::FirstActivation,
            RobotQtViewerWorkbenchTransitionCause::ProjectLoaded,
            sourceId);
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::requestTransition(
        RobotQtViewerWorkbenchKind target,
        const QString& sourceId,
        QWidget* promptParent,
        RobotQtViewerWorkbenchTransitionCause cause)
    {
        return requestTransition(
            robotQtViewerWorkbenchId(target), sourceId, promptParent, cause);
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::requestTransition(
        const QString& target,
        const QString& sourceId,
        QWidget* promptParent,
        RobotQtViewerWorkbenchTransitionCause cause)
    {
        if(m_shutdownComplete) {
            return workbenchTransitionRejected(
                QStringLiteral("Workbench system is shutting down."),
                QStringLiteral("workbench.shutdown"));
        }
        if(m_transitionInProgress) {
            return workbenchTransitionRejected(
                QStringLiteral("Another workbench transition is already running."),
                QStringLiteral("workbench.transition.reentrant"));
        }
        if(m_hasPreparedDeactivation) {
            return workbenchTransitionRejected(
                QStringLiteral("A prepared workbench deactivation is still pending."),
                QStringLiteral("workbench.transition.prepared"));
        }
        if(!m_registry.isWorkbenchReady(target)) {
            return workbenchTransitionFailed(
                QStringLiteral("Workbench is not ready: %1")
                    .arg(workbenchDisplayName(m_registry, target)),
                QStringLiteral("workbench.lifecycle.missing"));
        }

        const QString previous = m_manager.activeWorkbenchId();
        if(lifecycleState(previous) != RobotQtViewerWorkbenchLifecycleState::Active) {
            return workbenchTransitionFailed(
                QStringLiteral("The active workbench is not in an operational state."),
                QStringLiteral("workbench.active.failed_state"));
        }
        if(lifecycleState(target) == RobotQtViewerWorkbenchLifecycleState::Failed) {
            return workbenchTransitionFailed(
                QStringLiteral("The target workbench requires project reload after a lifecycle failure."),
                QStringLiteral("workbench.target.failed_state"));
        }
        if(previous == target) {
            return workbenchTransitionSucceeded();
        }
        IRobotQtViewerWorkbenchLifecycle* previousLifecycle = m_registry.lifecycle(previous);
        IRobotQtViewerWorkbenchLifecycle* targetLifecycle = m_registry.lifecycle(target);
        if(previousLifecycle == nullptr || targetLifecycle == nullptr) {
            return workbenchTransitionFailed(
                QStringLiteral("Workbench transition lifecycle is missing."),
                QStringLiteral("workbench.lifecycle.missing"));
        }

        TransitionGuard guard(m_transitionInProgress);
        QElapsedTimer elapsed;
        elapsed.start();
        const RobotQtViewerWorkbenchTransitionContext context = makeTransitionContext(
            previous, target, cause, sourceId, promptParent);

        RobotQtViewerWorkbenchTransitionResult result = callPrepare(*previousLifecycle, context);
        if(!result.succeeded()) {
            return result;
        }

        m_states[previous] = RobotQtViewerWorkbenchLifecycleState::Deactivating;
        result = callDeactivate(*previousLifecycle, context);
        if(!result.succeeded()) {
            m_states[previous] = RobotQtViewerWorkbenchLifecycleState::Failed;
            if(!rollbackWorkbench(previous, context)) {
                return workbenchTransitionFailed(
                    QStringLiteral("%1 Rollback of %2 also failed.")
                        .arg(result.message, workbenchDisplayName(m_registry, previous)),
                    QStringLiteral("workbench.rollback.failed"));
            }
            return result;
        }
        m_states[previous] = RobotQtViewerWorkbenchLifecycleState::Suspended;

        const RobotQtViewerWorkbenchLifecycleState targetState = lifecycleState(target);
        RobotQtViewerWorkbenchActivationContext activation;
        activation.transition = context;
        activation.activationKind = targetState == RobotQtViewerWorkbenchLifecycleState::Suspended
            ? RobotQtViewerWorkbenchActivationKind::Resume
            : RobotQtViewerWorkbenchActivationKind::FirstActivation;
        m_states[target] = RobotQtViewerWorkbenchLifecycleState::Activating;
        result = callActivate(*targetLifecycle, activation);
        if(!result.succeeded()) {
            m_states[target] = RobotQtViewerWorkbenchLifecycleState::Failed;
            RobotQtViewerWorkbenchTransitionContext cleanup = context;
            cleanup.cause = RobotQtViewerWorkbenchTransitionCause::Rollback;
            const RobotQtViewerWorkbenchTransitionResult cleanupResult =
                callDeactivate(*targetLifecycle, cleanup);
            m_states[target] = cleanupResult.succeeded()
                ? RobotQtViewerWorkbenchLifecycleState::Inactive
                : RobotQtViewerWorkbenchLifecycleState::Failed;
            if(!rollbackWorkbench(previous, context)) {
                return workbenchTransitionFailed(
                    QStringLiteral("%1 Rollback of %2 also failed.")
                        .arg(result.message, workbenchDisplayName(m_registry, previous)),
                    QStringLiteral("workbench.rollback.failed"));
            }
            if(!cleanupResult.succeeded()) {
                return workbenchTransitionFailed(
                    QStringLiteral("%1 Target cleanup also failed: %2")
                        .arg(result.message, cleanupResult.message),
                    QStringLiteral("workbench.target_cleanup.failed"));
            }
            return result;
        }

        m_states[target] = RobotQtViewerWorkbenchLifecycleState::Active;
        const RobotQtViewerWorkbenchDescriptor* targetDescriptor =
            m_registry.descriptor(target);
        if(targetDescriptor == nullptr) {
            return workbenchTransitionFailed(
                QStringLiteral("Workbench descriptor is missing: %1").arg(target),
                QStringLiteral("workbench.descriptor.missing"));
        }
        m_manager.commitWorkbench(target, *targetDescriptor, sourceId);
        publishCommitted(previous, target, context.transitionId, sourceId);
        m_hasPreparedDeactivation = false;

        LOG_DEBUG("rs2026") << "Workbench transition committed: id=" << context.transitionId
            << ", previous=" << workbenchDisplayName(m_registry, previous).toStdString()
            << ", target=" << workbenchDisplayName(m_registry, target).toStdString()
            << ", elapsedMs=" << elapsed.elapsed();
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::prepareActiveDeactivation(
        RobotQtViewerWorkbenchTransitionCause cause,
        const QString& sourceId,
        QWidget* promptParent)
    {
        if(m_transitionInProgress || m_shutdownComplete) {
            return workbenchTransitionRejected(
                QStringLiteral("Workbench transition is not available."),
                QStringLiteral("workbench.transition.busy"));
        }
        if(m_hasPreparedDeactivation) {
            return workbenchTransitionRejected(
                QStringLiteral("A prepared workbench deactivation is already pending."),
                QStringLiteral("workbench.transition.prepared"));
        }
        const QString active = m_manager.activeWorkbenchId();
        IRobotQtViewerWorkbenchLifecycle* lifecycle = m_registry.lifecycle(active);
        if(lifecycle == nullptr) {
            return workbenchTransitionFailed(
                QStringLiteral("Active workbench lifecycle is missing."),
                QStringLiteral("workbench.lifecycle.missing"));
        }

        TransitionGuard guard(m_transitionInProgress);
        const RobotQtViewerWorkbenchTransitionContext context = makeTransitionContext(
            active, active, cause, sourceId, promptParent);
        const RobotQtViewerWorkbenchTransitionResult result = callPrepare(*lifecycle, context);
        if(result.succeeded()) {
            m_preparedContext = context;
            m_hasPreparedDeactivation = true;
        }
        return result;
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::deactivateActive(
        RobotQtViewerWorkbenchTransitionCause cause,
        const QString& sourceId,
        QWidget* promptParent,
        bool alreadyPrepared)
    {
        if(m_transitionInProgress || m_shutdownComplete) {
            return workbenchTransitionRejected(
                QStringLiteral("Workbench transition is not available."),
                QStringLiteral("workbench.transition.busy"));
        }
        const QString active = m_manager.activeWorkbenchId();
        IRobotQtViewerWorkbenchLifecycle* lifecycle = m_registry.lifecycle(active);
        if(lifecycle == nullptr) {
            return workbenchTransitionFailed(
                QStringLiteral("Active workbench lifecycle is missing."),
                QStringLiteral("workbench.lifecycle.missing"));
        }
        if(alreadyPrepared && (!m_hasPreparedDeactivation ||
            m_preparedContext.previousWorkbenchId != active ||
            m_preparedContext.targetWorkbenchId != active ||
            m_preparedContext.cause != cause ||
            m_preparedContext.sourceId != sourceId ||
            m_preparedContext.projectGeneration != m_projectGeneration)) {
            return workbenchTransitionFailed(
                QStringLiteral("Prepared workbench transition no longer matches the active mode."),
                QStringLiteral("workbench.transition.stale_prepare"));
        }

        TransitionGuard guard(m_transitionInProgress);
        const RobotQtViewerWorkbenchTransitionContext context = alreadyPrepared
            ? m_preparedContext
            : makeTransitionContext(active, active, cause, sourceId, promptParent);
        if(!alreadyPrepared) {
            const RobotQtViewerWorkbenchTransitionResult prepare = callPrepare(*lifecycle, context);
            if(!prepare.succeeded()) {
                return prepare;
            }
        }
        m_hasPreparedDeactivation = false;

        m_states[active] = RobotQtViewerWorkbenchLifecycleState::Deactivating;
        const RobotQtViewerWorkbenchTransitionResult result = callDeactivate(*lifecycle, context);
        if(!result.succeeded()) {
            m_states[active] = RobotQtViewerWorkbenchLifecycleState::Failed;
            rollbackWorkbench(active, context);
            return result;
        }
        m_states[active] = RobotQtViewerWorkbenchLifecycleState::Suspended;
        return result;
    }

    void RobotQtViewerWorkbenchTransitionCoordinator::cancelPreparedDeactivation()
    {
        m_hasPreparedDeactivation = false;
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::activateCommittedWorkbench(
        RobotQtViewerWorkbenchActivationKind activationKind,
        RobotQtViewerWorkbenchTransitionCause cause,
        const QString& sourceId)
    {
        if(m_transitionInProgress || m_shutdownComplete) {
            return workbenchTransitionRejected(
                QStringLiteral("Workbench activation is not available."),
                QStringLiteral("workbench.transition.busy"));
        }
        const QString active = m_manager.activeWorkbenchId();
        IRobotQtViewerWorkbenchLifecycle* lifecycle = m_registry.lifecycle(active);
        if(lifecycle == nullptr) {
            return workbenchTransitionFailed(
                QStringLiteral("Active workbench lifecycle is missing."),
                QStringLiteral("workbench.lifecycle.missing"));
        }

        TransitionGuard guard(m_transitionInProgress);
        RobotQtViewerWorkbenchActivationContext activation;
        activation.transition = makeTransitionContext(
            active, active, cause, sourceId, nullptr);
        activation.activationKind = activationKind;
        m_states[active] = RobotQtViewerWorkbenchLifecycleState::Activating;
        const RobotQtViewerWorkbenchTransitionResult result = callActivate(*lifecycle, activation);
        m_states[active] = result.succeeded()
            ? RobotQtViewerWorkbenchLifecycleState::Active
            : RobotQtViewerWorkbenchLifecycleState::Failed;
        return result;
    }

    void RobotQtViewerWorkbenchTransitionCoordinator::releaseProject(
        const QString& sourceId) noexcept
    {
        const std::uint64_t previousGeneration = m_projectGeneration;
        ++m_projectGeneration;
        RobotQtViewerWorkbenchProjectReleaseContext context;
        context.previousProjectGeneration = previousGeneration;
        context.nextProjectGeneration = m_projectGeneration;
        context.sourceId = sourceId;

        std::set<IRobotQtViewerWorkbenchLifecycle*> released;
        for(const RobotQtViewerWorkbenchDesc& mode : m_registry.workbenches()) {
            if(mode.lifecycle == nullptr || !released.insert(mode.lifecycle).second) {
                continue;
            }
            mode.lifecycle->releaseProject(context);
        }
        for(const RobotQtViewerWorkbenchDesc& mode : m_registry.workbenches()) {
            m_states[mode.descriptor.id] = RobotQtViewerWorkbenchLifecycleState::Inactive;
        }
        m_manager.releaseProjectSessions();
        m_hasPreparedDeactivation = false;
    }

    void RobotQtViewerWorkbenchTransitionCoordinator::shutdownAll(
        const QString& sourceId) noexcept
    {
        if(m_shutdownComplete) {
            return;
        }
        m_shutdownComplete = true;
        m_hasPreparedDeactivation = false;

        RobotQtViewerWorkbenchShutdownContext context;
        context.projectGeneration = m_projectGeneration;
        context.sourceId = sourceId;
        std::set<IRobotQtViewerWorkbenchLifecycle*> shutdown;
        for(const RobotQtViewerWorkbenchDesc& mode : m_registry.workbenches()) {
            if(mode.lifecycle == nullptr || !shutdown.insert(mode.lifecycle).second) {
                continue;
            }
            mode.lifecycle->shutdown(context);
        }
        for(const RobotQtViewerWorkbenchDesc& mode : m_registry.workbenches()) {
            m_states[mode.descriptor.id] = RobotQtViewerWorkbenchLifecycleState::Inactive;
        }
    }

    bool RobotQtViewerWorkbenchTransitionCoordinator::transitionInProgress() const
    {
        return m_transitionInProgress;
    }

    bool RobotQtViewerWorkbenchTransitionCoordinator::shutdownComplete() const
    {
        return m_shutdownComplete;
    }

    std::uint64_t RobotQtViewerWorkbenchTransitionCoordinator::projectGeneration() const
    {
        return m_projectGeneration;
    }

    RobotQtViewerWorkbenchLifecycleState
    RobotQtViewerWorkbenchTransitionCoordinator::lifecycleState(
        RobotQtViewerWorkbenchKind kind) const
    {
        return lifecycleState(robotQtViewerWorkbenchId(kind));
    }

    RobotQtViewerWorkbenchLifecycleState
    RobotQtViewerWorkbenchTransitionCoordinator::lifecycleState(
        const QString& workbenchId) const
    {
        const auto it = m_states.find(workbenchId);
        return it == m_states.end()
            ? RobotQtViewerWorkbenchLifecycleState::Inactive
            : it->second;
    }

    RobotQtViewerWorkbenchTransitionContext
    RobotQtViewerWorkbenchTransitionCoordinator::makeTransitionContext(
        const QString& previous,
        const QString& target,
        RobotQtViewerWorkbenchTransitionCause cause,
        const QString& sourceId,
        QWidget* promptParent)
    {
        RobotQtViewerWorkbenchTransitionContext context;
        context.transitionId = ++m_transitionSequence;
        context.projectGeneration = m_projectGeneration;
        context.previousWorkbenchId = previous;
        context.targetWorkbenchId = target;
        robotQtViewerWorkbenchKindFromId(previous, &context.previousWorkbench);
        robotQtViewerWorkbenchKindFromId(target, &context.targetWorkbench);
        context.cause = cause;
        context.sourceId = sourceId;
        context.promptParent = promptParent;
        return context;
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::callPrepare(
        IRobotQtViewerWorkbenchLifecycle& lifecycle,
        const RobotQtViewerWorkbenchTransitionContext& context)
    {
        try {
            return lifecycle.prepareDeactivate(context);
        } catch(const std::exception& exception) {
            return workbenchTransitionFailed(
                exceptionMessage("prepareDeactivate", exception),
                QStringLiteral("workbench.lifecycle.exception"));
        } catch(...) {
            return workbenchTransitionFailed(
                QStringLiteral("Workbench prepareDeactivate threw an unknown exception."),
                QStringLiteral("workbench.lifecycle.exception"));
        }
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::callDeactivate(
        IRobotQtViewerWorkbenchLifecycle& lifecycle,
        const RobotQtViewerWorkbenchTransitionContext& context)
    {
        try {
            return lifecycle.deactivate(context);
        } catch(const std::exception& exception) {
            return workbenchTransitionFailed(
                exceptionMessage("deactivate", exception),
                QStringLiteral("workbench.lifecycle.exception"));
        } catch(...) {
            return workbenchTransitionFailed(
                QStringLiteral("Workbench deactivate threw an unknown exception."),
                QStringLiteral("workbench.lifecycle.exception"));
        }
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerWorkbenchTransitionCoordinator::callActivate(
        IRobotQtViewerWorkbenchLifecycle& lifecycle,
        const RobotQtViewerWorkbenchActivationContext& context)
    {
        try {
            return lifecycle.activate(context);
        } catch(const std::exception& exception) {
            return workbenchTransitionFailed(
                exceptionMessage("activate", exception),
                QStringLiteral("workbench.lifecycle.exception"));
        } catch(...) {
            return workbenchTransitionFailed(
                QStringLiteral("Workbench activate threw an unknown exception."),
                QStringLiteral("workbench.lifecycle.exception"));
        }
    }

    bool RobotQtViewerWorkbenchTransitionCoordinator::rollbackWorkbench(
        const QString& kind,
        const RobotQtViewerWorkbenchTransitionContext& transition)
    {
        IRobotQtViewerWorkbenchLifecycle* lifecycle = m_registry.lifecycle(kind);
        if(lifecycle == nullptr) {
            return false;
        }
        RobotQtViewerWorkbenchActivationContext rollback;
        rollback.transition = transition;
        rollback.transition.cause = RobotQtViewerWorkbenchTransitionCause::Rollback;
        rollback.activationKind = RobotQtViewerWorkbenchActivationKind::Rollback;
        m_states[kind] = RobotQtViewerWorkbenchLifecycleState::Activating;
        const RobotQtViewerWorkbenchTransitionResult result = callActivate(*lifecycle, rollback);
        m_states[kind] = result.succeeded()
            ? RobotQtViewerWorkbenchLifecycleState::Active
            : RobotQtViewerWorkbenchLifecycleState::Failed;
        return result.succeeded();
    }

    void RobotQtViewerWorkbenchTransitionCoordinator::publishCommitted(
        const QString& previous,
        const QString& active,
        std::uint64_t transitionId,
        const QString& sourceId)
    {
        RobotQtViewerEvent event;
        event.kind = RobotQtViewerEventKind::WorkbenchTransitionCommitted;
        event.sourceId = sourceId;
        event.workbench.previousWorkbenchId = previous;
        event.workbench.activeWorkbenchId = active;
        robotQtViewerWorkbenchKindFromId(previous, &event.workbench.previousWorkbench);
        robotQtViewerWorkbenchKindFromId(active, &event.workbench.activeWorkbench);
        event.workbench.transitionId = transitionId;
        m_eventHub.publish(event);
    }
}
