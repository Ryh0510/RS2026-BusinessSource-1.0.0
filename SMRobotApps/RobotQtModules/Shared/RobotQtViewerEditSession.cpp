#include "RobotQtViewerEditSession.h"

#include <algorithm>
#include <exception>

namespace robot_qt_viewer
{
    bool RobotQtViewerEditSessionCoordinator::registerSession(IWorkbenchEditSession& session)
    {
        if(std::find(m_sessions.begin(), m_sessions.end(), &session) != m_sessions.end()) {
            return false;
        }
        m_sessions.push_back(&session);
        return true;
    }

    void RobotQtViewerEditSessionCoordinator::unregisterSession(
        IWorkbenchEditSession& session) noexcept
    {
        m_sessions.erase(
            std::remove(m_sessions.begin(), m_sessions.end(), &session),
            m_sessions.end());
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerEditSessionCoordinator::prepareTransition(
        const RobotQtViewerEditTransitionRequest& request)
    {
        if(m_transitionInProgress) {
            return workbenchTransitionRejected(
                QStringLiteral("An edit-session transition is already running."),
                QStringLiteral("edit_session.transition.reentrant"));
        }

        std::vector<IWorkbenchEditSession*> pending;
        for(IWorkbenchEditSession* session : m_sessions) {
            if(session != nullptr &&
                session->editSessionState() == RobotQtViewerEditSessionState::Dirty) {
                pending.push_back(session);
            }
        }
        if(pending.empty()) {
            return workbenchTransitionSucceeded();
        }
        if(pending.size() > 1) {
            return workbenchTransitionFailed(
                QStringLiteral("Multiple edit sessions own pending project changes."),
                QStringLiteral("edit_session.multiple_mutation_owners"));
        }

        m_transitionInProgress = true;
        try {
            const RobotQtViewerWorkbenchTransitionResult result =
                pending.front()->prepareTransition(request);
            m_transitionInProgress = false;
            return result;
        } catch(const std::exception& exception) {
            m_transitionInProgress = false;
            return workbenchTransitionFailed(
                QStringLiteral("Edit-session transition failed: %1")
                    .arg(QString::fromLocal8Bit(exception.what())),
                QStringLiteral("edit_session.transition.exception"));
        } catch(...) {
            m_transitionInProgress = false;
            return workbenchTransitionFailed(
                QStringLiteral("Edit-session transition failed."),
                QStringLiteral("edit_session.transition.unknown_exception"));
        }
    }

    void RobotQtViewerEditSessionCoordinator::releaseProject(
        std::uint64_t nextProjectGeneration) noexcept
    {
        for(IWorkbenchEditSession* session : m_sessions) {
            if(session != nullptr) {
                session->releaseEditSession(nextProjectGeneration);
            }
        }
        m_transitionInProgress = false;
    }

    std::size_t RobotQtViewerEditSessionCoordinator::pendingSessionCount() const
    {
        return static_cast<std::size_t>(std::count_if(
            m_sessions.begin(),
            m_sessions.end(),
            [](const IWorkbenchEditSession* session) {
                return session != nullptr &&
                    session->editSessionState() == RobotQtViewerEditSessionState::Dirty;
            }));
    }

    bool RobotQtViewerEditSessionCoordinator::transitionInProgress() const
    {
        return m_transitionInProgress;
    }
}
