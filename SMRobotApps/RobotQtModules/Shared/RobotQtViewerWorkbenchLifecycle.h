#pragma once

#include "RobotQtViewerWorkbench.h"

#include <QString>

#include <cstdint>

class QWidget;

namespace robot_qt_viewer
{
    enum class RobotQtViewerWorkbenchLifecycleState
    {
        Inactive,
        Activating,
        Active,
        Deactivating,
        Suspended,
        Failed,
        ShuttingDown
    };

    enum class RobotQtViewerWorkbenchTransitionCause
    {
        UserModeSwitch,
        TaskHandoff,
        ProjectReplacing,
        ProjectLoaded,
        ApplicationShutdown,
        PackageDisabled,
        Rollback
    };

    enum class RobotQtViewerWorkbenchActivationKind
    {
        FirstActivation,
        Resume,
        FreshProject,
        Rollback
    };

    enum class RobotQtViewerWorkbenchTransitionStatus
    {
        Succeeded,
        Rejected,
        Failed
    };

    enum class RobotQtViewerWorkbenchExecutionPolicy
    {
        NoOwnedExecution,
        MustQuiesce,
        MayContinueHeadless
    };

    enum class RobotQtViewerWorkbenchReactivationPolicy
    {
        RestoreUiOnly,
        ManualResume,
        Restart,
        PackageDefined
    };

    struct RobotQtViewerWorkbenchLifecyclePolicy
    {
        RobotQtViewerWorkbenchExecutionPolicy execution =
            RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution;
        RobotQtViewerWorkbenchReactivationPolicy reactivation =
            RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly;
    };

    struct RobotQtViewerWorkbenchTransitionResult
    {
        RobotQtViewerWorkbenchTransitionStatus status =
            RobotQtViewerWorkbenchTransitionStatus::Succeeded;
        QString message;
        QString diagnosticCode;

        bool succeeded() const;
    };

    RobotQtViewerWorkbenchTransitionResult workbenchTransitionSucceeded(
        const QString& message = QString());
    RobotQtViewerWorkbenchTransitionResult workbenchTransitionRejected(
        const QString& message,
        const QString& diagnosticCode = QString());
    RobotQtViewerWorkbenchTransitionResult workbenchTransitionFailed(
        const QString& message,
        const QString& diagnosticCode = QString());

    struct RobotQtViewerWorkbenchTransitionContext
    {
        std::uint64_t transitionId = 0;
        std::uint64_t projectGeneration = 0;
        RobotQtViewerWorkbenchKind previousWorkbench = RobotQtViewerWorkbenchKind::Browse;
        RobotQtViewerWorkbenchKind targetWorkbench = RobotQtViewerWorkbenchKind::Browse;
        RobotQtViewerWorkbenchTransitionCause cause =
            RobotQtViewerWorkbenchTransitionCause::UserModeSwitch;
        QString sourceId;
        QWidget* promptParent = nullptr;
    };

    struct RobotQtViewerWorkbenchActivationContext
    {
        RobotQtViewerWorkbenchTransitionContext transition;
        RobotQtViewerWorkbenchActivationKind activationKind =
            RobotQtViewerWorkbenchActivationKind::FirstActivation;
    };

    struct RobotQtViewerWorkbenchProjectReleaseContext
    {
        std::uint64_t previousProjectGeneration = 0;
        std::uint64_t nextProjectGeneration = 0;
        QString sourceId;
    };

    struct RobotQtViewerWorkbenchShutdownContext
    {
        std::uint64_t projectGeneration = 0;
        QString sourceId;
    };

    class IRobotQtViewerWorkbenchLifecycle
    {
    public:
        virtual ~IRobotQtViewerWorkbenchLifecycle() = default;

        virtual RobotQtViewerWorkbenchTransitionResult prepareDeactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) = 0;
        virtual RobotQtViewerWorkbenchTransitionResult deactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) = 0;
        virtual RobotQtViewerWorkbenchTransitionResult activate(
            const RobotQtViewerWorkbenchActivationContext& context) = 0;
        virtual void releaseProject(
            const RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept = 0;
        virtual void shutdown(
            const RobotQtViewerWorkbenchShutdownContext& context) noexcept = 0;
    };

    class RobotQtViewerNoOpWorkbenchLifecycle final
        : public IRobotQtViewerWorkbenchLifecycle
    {
    public:
        RobotQtViewerWorkbenchTransitionResult prepareDeactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) override;
        RobotQtViewerWorkbenchTransitionResult deactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) override;
        RobotQtViewerWorkbenchTransitionResult activate(
            const RobotQtViewerWorkbenchActivationContext& context) override;
        void releaseProject(
            const RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept override;
        void shutdown(
            const RobotQtViewerWorkbenchShutdownContext& context) noexcept override;
    };
}
