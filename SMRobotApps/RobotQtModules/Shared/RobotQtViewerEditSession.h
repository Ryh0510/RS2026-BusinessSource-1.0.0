#pragma once

#include "RobotQtViewerWorkbenchLifecycle.h"

#include <QString>

#include <cstdint>
#include <vector>

namespace robot_qt_viewer
{
    enum class RobotQtViewerEditSessionState
    {
        Idle,
        Clean,
        Dirty,
        Applying,
        Failed
    };

    struct RobotQtViewerEditTransitionRequest
    {
        RobotQtViewerWorkbenchTransitionCause cause =
            RobotQtViewerWorkbenchTransitionCause::SelectionChange;
        std::uint64_t projectGeneration = 0;
        QString sourceId;
        QWidget* promptParent = nullptr;
        bool restoreEditorTarget = true;
    };

    class IWorkbenchEditSession
    {
    public:
        virtual ~IWorkbenchEditSession() = default;

        virtual QString ownerWorkbenchId() const = 0;
        virtual QString taskId() const = 0;
        virtual RobotQtViewerEditSessionState editSessionState() const = 0;
        virtual RobotQtViewerWorkbenchTransitionResult prepareTransition(
            const RobotQtViewerEditTransitionRequest& request) = 0;
        virtual void releaseEditSession(std::uint64_t nextProjectGeneration) noexcept = 0;
    };

    class RobotQtViewerEditSessionCoordinator
    {
    public:
        bool registerSession(IWorkbenchEditSession& session);
        void unregisterSession(IWorkbenchEditSession& session) noexcept;

        RobotQtViewerWorkbenchTransitionResult prepareTransition(
            const RobotQtViewerEditTransitionRequest& request);
        void releaseProject(std::uint64_t nextProjectGeneration) noexcept;

        std::size_t pendingSessionCount() const;
        bool transitionInProgress() const;

    private:
        std::vector<IWorkbenchEditSession*> m_sessions;
        bool m_transitionInProgress = false;
    };
}
