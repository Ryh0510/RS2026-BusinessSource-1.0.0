#pragma once

#include "RobotQtViewerWorkbenchLifecycle.h"

#include <map>

namespace robot_qt_viewer
{
    class RobotQtViewerEventHub;
    class RobotQtViewerWorkbenchManager;
    class RobotQtViewerWorkbenchPackageRegistry;

    class RobotQtViewerWorkbenchTransitionCoordinator
    {
    public:
        RobotQtViewerWorkbenchTransitionCoordinator(
            RobotQtViewerWorkbenchPackageRegistry& registry,
            RobotQtViewerWorkbenchManager& manager,
            RobotQtViewerEventHub& eventHub);

        RobotQtViewerWorkbenchTransitionResult initializeActiveWorkbench(
            const QString& sourceId = QString());
        RobotQtViewerWorkbenchTransitionResult requestTransition(
            RobotQtViewerWorkbenchKind target,
            const QString& sourceId,
            QWidget* promptParent = nullptr,
            RobotQtViewerWorkbenchTransitionCause cause =
                RobotQtViewerWorkbenchTransitionCause::UserWorkbenchSwitch);
        RobotQtViewerWorkbenchTransitionResult requestTransition(
            const QString& targetWorkbenchId,
            const QString& sourceId,
            QWidget* promptParent = nullptr,
            RobotQtViewerWorkbenchTransitionCause cause =
                RobotQtViewerWorkbenchTransitionCause::UserWorkbenchSwitch);

        RobotQtViewerWorkbenchTransitionResult prepareActiveDeactivation(
            RobotQtViewerWorkbenchTransitionCause cause,
            const QString& sourceId,
            QWidget* promptParent = nullptr);
        RobotQtViewerWorkbenchTransitionResult deactivateActive(
            RobotQtViewerWorkbenchTransitionCause cause,
            const QString& sourceId,
            QWidget* promptParent = nullptr,
            bool alreadyPrepared = false);
        void cancelPreparedDeactivation();
        RobotQtViewerWorkbenchTransitionResult activateCommittedWorkbench(
            RobotQtViewerWorkbenchActivationKind activationKind,
            RobotQtViewerWorkbenchTransitionCause cause,
            const QString& sourceId);
        void releaseProject(const QString& sourceId) noexcept;
        void shutdownAll(const QString& sourceId) noexcept;

        bool transitionInProgress() const;
        bool shutdownComplete() const;
        std::uint64_t projectGeneration() const;
        RobotQtViewerWorkbenchLifecycleState lifecycleState(
            RobotQtViewerWorkbenchKind kind) const;
        RobotQtViewerWorkbenchLifecycleState lifecycleState(
            const QString& workbenchId) const;

    private:
        RobotQtViewerWorkbenchTransitionContext makeTransitionContext(
            const QString& previousWorkbenchId,
            const QString& targetWorkbenchId,
            RobotQtViewerWorkbenchTransitionCause cause,
            const QString& sourceId,
            QWidget* promptParent);
        RobotQtViewerWorkbenchTransitionResult callPrepare(
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            const RobotQtViewerWorkbenchTransitionContext& context);
        RobotQtViewerWorkbenchTransitionResult callDeactivate(
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            const RobotQtViewerWorkbenchTransitionContext& context);
        RobotQtViewerWorkbenchTransitionResult callActivate(
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            const RobotQtViewerWorkbenchActivationContext& context);
        bool rollbackWorkbench(
            const QString& workbenchId,
            const RobotQtViewerWorkbenchTransitionContext& transition);
        void publishCommitted(
            const QString& previousWorkbenchId,
            const QString& activeWorkbenchId,
            std::uint64_t transitionId,
            const QString& sourceId);

        RobotQtViewerWorkbenchPackageRegistry& m_registry;
        RobotQtViewerWorkbenchManager& m_manager;
        RobotQtViewerEventHub& m_eventHub;
        std::map<QString, RobotQtViewerWorkbenchLifecycleState> m_states;
        std::uint64_t m_transitionSequence = 0;
        std::uint64_t m_projectGeneration = 1;
        RobotQtViewerWorkbenchTransitionContext m_preparedContext;
        bool m_transitionInProgress = false;
        bool m_hasPreparedDeactivation = false;
        bool m_shutdownComplete = false;
    };
}
