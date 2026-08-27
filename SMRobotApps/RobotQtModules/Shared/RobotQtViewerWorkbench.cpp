#include "RobotQtViewerWorkbench.h"

namespace robot_qt_viewer
{
    namespace
    {
        const RobotQtViewerWorkbenchDescriptor kBrowseDescriptor{
            RobotQtViewerWorkbenchKind::Browse,
            RobotQtViewerWorkbenchDomain::ProjectAssembly,
            RobotQtViewerRightPanelKind::SceneSelection,
            RobotQtViewerViewportInteractionMode::Browse,
            QStringLiteral("smrobot.mode.project-assembly"),
            QStringLiteral("Project Assembly"),
            QStringLiteral("Scene Edit Panel"),
            true,
            false,
            true,
            false
        };

        const RobotQtViewerWorkbenchDescriptor kMotionDescriptor{
            RobotQtViewerWorkbenchKind::Motion,
            RobotQtViewerWorkbenchDomain::RobotRun,
            RobotQtViewerRightPanelKind::Motion,
            RobotQtViewerViewportInteractionMode::SelectRobot,
            QStringLiteral("smrobot.mode.robot-run"),
            QStringLiteral("Robot Run"),
            QStringLiteral("Motion Panel"),
            true,
            false,
            false,
            false
        };

        const RobotQtViewerWorkbenchDescriptor kToolSetupDescriptor{
            RobotQtViewerWorkbenchKind::ToolSetup,
            RobotQtViewerWorkbenchDomain::ProjectAssembly,
            RobotQtViewerRightPanelKind::ProjectAssembly,
            RobotQtViewerViewportInteractionMode::SelectMount,
            QStringLiteral("smrobot.mode.tool-setup"),
            QStringLiteral("Project Assembly"),
            QStringLiteral("Mount Frame Editor"),
            true,
            false,
            true,
            false
        };

        const RobotQtViewerWorkbenchDescriptor kCollisionDescriptor{
            RobotQtViewerWorkbenchKind::Collision,
            RobotQtViewerWorkbenchDomain::CollisionConfig,
            RobotQtViewerRightPanelKind::CollisionConfig,
            RobotQtViewerViewportInteractionMode::SelectCollisionTarget,
            QStringLiteral("smrobot.mode.collision-config"),
            QStringLiteral("Collision Config"),
            QStringLiteral("Collision Detector Configuration"),
            false,
            true,
            false,
            true
        };

        const RobotQtViewerWorkbenchDescriptor kTrajectoryPlanningDescriptor{
            RobotQtViewerWorkbenchKind::TrajectoryPlanning,
            RobotQtViewerWorkbenchDomain::TrajectoryPlanning,
            RobotQtViewerRightPanelKind::MotionPlanning,
            RobotQtViewerViewportInteractionMode::Browse,
            QStringLiteral("smrobot.mode.motion-planning"),
            QStringLiteral("Motion Planning"),
            QStringLiteral("Motion Planning"),
            true,
            false,
            false,
            false
        };

        const RobotQtViewerWorkbenchDescriptor kSprayProcessDescriptor{
            RobotQtViewerWorkbenchKind::SprayProcess,
            RobotQtViewerWorkbenchDomain::SprayProcess,
            RobotQtViewerRightPanelKind::Status,
            RobotQtViewerViewportInteractionMode::Browse,
            QStringLiteral("smrobot.mode.spray-process"),
            QStringLiteral("Spray Process"),
            QStringLiteral("Spray Process"),
            true,
            false,
            false,
            false
        };

        const RobotQtViewerWorkbenchDescriptor kDigitalTwinDescriptor{
            RobotQtViewerWorkbenchKind::DigitalTwin,
            RobotQtViewerWorkbenchDomain::DigitalTwin,
            RobotQtViewerRightPanelKind::Status,
            RobotQtViewerViewportInteractionMode::Browse,
            QStringLiteral("smrobot.mode.digital-twin"),
            QStringLiteral("Digital Twin"),
            QStringLiteral("Digital Twin"),
            true,
            false,
            false,
            false
        };

        const RobotQtViewerWorkbenchDescriptor kCoatingAnalysisDescriptor{
            RobotQtViewerWorkbenchKind::CoatingAnalysis,
            RobotQtViewerWorkbenchDomain::CoatingAnalysis,
            RobotQtViewerRightPanelKind::CoatingAnalysis,
            RobotQtViewerViewportInteractionMode::Browse,
            QStringLiteral("smrobot.mode.coating-analysis"),
            QStringLiteral("Coating Analysis"),
            QStringLiteral("Coating Analysis"),
            true,
            false,
            false,
            false
        };
    }

    QString robotQtViewerWorkbenchName(RobotQtViewerWorkbenchKind kind)
    {
        return robotQtViewerWorkbenchDescriptor(kind).displayName;
    }

    QString robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind kind)
    {
        return robotQtViewerWorkbenchDescriptor(kind).id;
    }

    bool robotQtViewerWorkbenchKindFromId(
        const QString& modeId,
        RobotQtViewerWorkbenchKind* kind)
    {
        if(kind == nullptr) {
            return false;
        }
        for(const RobotQtViewerWorkbenchKind candidate : {
                RobotQtViewerWorkbenchKind::Browse,
                RobotQtViewerWorkbenchKind::Motion,
                RobotQtViewerWorkbenchKind::ToolSetup,
                RobotQtViewerWorkbenchKind::Collision,
                RobotQtViewerWorkbenchKind::TrajectoryPlanning,
                RobotQtViewerWorkbenchKind::SprayProcess,
                RobotQtViewerWorkbenchKind::CoatingAnalysis,
                RobotQtViewerWorkbenchKind::DigitalTwin }) {
            if(robotQtViewerWorkbenchId(candidate) == modeId) {
                *kind = candidate;
                return true;
            }
        }
        return false;
    }

    const RobotQtViewerWorkbenchDescriptor& robotQtViewerWorkbenchDescriptor(
        RobotQtViewerWorkbenchKind kind)
    {
        switch(kind) {
        case RobotQtViewerWorkbenchKind::Browse:
            return kBrowseDescriptor;
        case RobotQtViewerWorkbenchKind::Motion:
            return kMotionDescriptor;
        case RobotQtViewerWorkbenchKind::ToolSetup:
            return kToolSetupDescriptor;
        case RobotQtViewerWorkbenchKind::Collision:
            return kCollisionDescriptor;
        case RobotQtViewerWorkbenchKind::TrajectoryPlanning:
            return kTrajectoryPlanningDescriptor;
        case RobotQtViewerWorkbenchKind::SprayProcess:
            return kSprayProcessDescriptor;
        case RobotQtViewerWorkbenchKind::CoatingAnalysis:
            return kCoatingAnalysisDescriptor;
        case RobotQtViewerWorkbenchKind::DigitalTwin:
            return kDigitalTwinDescriptor;
        }
        return kBrowseDescriptor;
    }

    QString robotQtViewerViewportInteractionModeName(RobotQtViewerViewportInteractionMode mode)
    {
        switch(mode) {
        case RobotQtViewerViewportInteractionMode::Browse:
            return QStringLiteral("Browse");
        case RobotQtViewerViewportInteractionMode::SelectRobot:
            return QStringLiteral("SelectRobot");
        case RobotQtViewerViewportInteractionMode::SelectLink:
            return QStringLiteral("SelectLink");
        case RobotQtViewerViewportInteractionMode::SelectMount:
            return QStringLiteral("SelectMount");
        case RobotQtViewerViewportInteractionMode::SelectAttachment:
            return QStringLiteral("SelectAttachment");
        case RobotQtViewerViewportInteractionMode::EditTransformPreview:
            return QStringLiteral("EditTransformPreview");
        case RobotQtViewerViewportInteractionMode::EditCollisionProxy:
            return QStringLiteral("EditCollisionProxy");
        case RobotQtViewerViewportInteractionMode::SelectCollisionTarget:
            return QStringLiteral("SelectCollisionTarget");
        }
        return QStringLiteral("Browse");
    }

    RobotQtViewerWorkbenchKind RobotQtViewerWorkbenchManager::activeWorkbench() const
    {
        return m_session.workbench;
    }

    const RobotQtViewerWorkbenchDescriptor& RobotQtViewerWorkbenchManager::activeDescriptor() const
    {
        return robotQtViewerWorkbenchDescriptor(m_session.workbench);
    }

    RobotQtViewerViewportInteractionMode RobotQtViewerWorkbenchManager::viewportMode() const
    {
        return m_session.viewportMode;
    }

    const RobotQtViewerTaskSession& RobotQtViewerWorkbenchManager::session() const
    {
        return m_session;
    }

    bool RobotQtViewerWorkbenchManager::enterWorkbench(
        RobotQtViewerWorkbenchKind kind,
        const QString& sourceId)
    {
        if(!canExitActiveWorkbench()) {
            return false;
        }

        commitWorkbench(kind, sourceId);
        return true;
    }

    void RobotQtViewerWorkbenchManager::setInitialWorkbench(
        RobotQtViewerWorkbenchKind kind)
    {
        m_suspendedSessions.clear();
        m_session = RobotQtViewerTaskSession{};
        const RobotQtViewerWorkbenchDescriptor& descriptor =
            robotQtViewerWorkbenchDescriptor(kind);
        m_session.workbench = kind;
        m_session.taskId = descriptor.id;
        m_session.viewportMode = descriptor.defaultViewportMode;
    }

    void RobotQtViewerWorkbenchManager::commitWorkbench(
        RobotQtViewerWorkbenchKind kind,
        const QString& sourceId)
    {
        if(kind == m_session.workbench) {
            if(!sourceId.isEmpty()) {
                m_session.taskId = sourceId;
            }
            return;
        }

        m_suspendedSessions[m_session.workbench] = m_session;
        const auto suspended = m_suspendedSessions.find(kind);
        m_session = suspended == m_suspendedSessions.end()
            ? RobotQtViewerTaskSession{}
            : suspended->second;
        const RobotQtViewerWorkbenchDescriptor& descriptor =
            robotQtViewerWorkbenchDescriptor(kind);
        m_session.workbench = kind;
        m_session.taskId = sourceId.isEmpty() ? descriptor.id : sourceId;
        if(suspended == m_suspendedSessions.end()) {
            m_session.viewportMode = descriptor.defaultViewportMode;
        }
    }

    bool RobotQtViewerWorkbenchManager::exitToBrowse(const QString& sourceId)
    {
        return enterWorkbench(RobotQtViewerWorkbenchKind::Browse, sourceId);
    }

    void RobotQtViewerWorkbenchManager::releaseProjectSessions()
    {
        const RobotQtViewerWorkbenchKind active = m_session.workbench;
        m_suspendedSessions.clear();
        m_session = RobotQtViewerTaskSession{};
        const RobotQtViewerWorkbenchDescriptor& descriptor =
            robotQtViewerWorkbenchDescriptor(active);
        m_session.workbench = active;
        m_session.taskId = descriptor.id;
        m_session.viewportMode = descriptor.defaultViewportMode;
    }

    bool RobotQtViewerWorkbenchManager::canExitActiveWorkbench() const
    {
        return m_session.canExit;
    }

    void RobotQtViewerWorkbenchManager::setSessionDirty(bool dirty)
    {
        m_session.dirty = dirty;
    }

    void RobotQtViewerWorkbenchManager::setSessionCanExit(bool canExit)
    {
        m_session.canExit = canExit;
    }

    void RobotQtViewerWorkbenchManager::setViewportMode(RobotQtViewerViewportInteractionMode mode)
    {
        m_session.viewportMode = mode;
    }

    void RobotQtViewerWorkbenchManager::setSessionTarget(
        const QString& robotId,
        const QString& linkName,
        const QString& mountId,
        const QString& attachmentId)
    {
        m_session.targetRobotId = robotId;
        m_session.targetLinkName = linkName;
        m_session.targetMountId = mountId;
        m_session.targetAttachmentId = attachmentId;
    }
}

