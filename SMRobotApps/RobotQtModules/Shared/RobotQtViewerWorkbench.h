#pragma once

#include <QString>

#include <map>

namespace robot_qt_viewer
{
    enum class RobotQtViewerWorkbenchKind
    {
        Browse,
        Motion,
        ToolSetup,
        Collision,
        TrajectoryPlanning,
        SprayProcess,
        CoatingAnalysis,
        DigitalTwin
    };

    enum class RobotQtViewerWorkbenchDomain
    {
        ProjectAssembly,
        RobotRun,
        CollisionConfig,
        TrajectoryPlanning,
        SprayProcess,
        CoatingAnalysis,
        DigitalTwin
    };

    enum class RobotQtViewerRightPanelKind
    {
        Status,
        SceneSelection,
        Motion,
        MotionPlanning,
        ProjectAssembly,
        CollisionConfig,
        CoatingAnalysis
    };

    enum class RobotQtViewerViewportInteractionMode
    {
        Browse,
        SelectRobot,
        SelectLink,
        SelectMount,
        SelectAttachment,
        EditTransformPreview,
        EditCollisionProxy,
        SelectCollisionTarget
    };

    struct RobotQtViewerWorkbenchDescriptor
    {
        RobotQtViewerWorkbenchKind kind = RobotQtViewerWorkbenchKind::Browse;
        RobotQtViewerWorkbenchDomain domain = RobotQtViewerWorkbenchDomain::ProjectAssembly;
        RobotQtViewerRightPanelKind rightPanel = RobotQtViewerRightPanelKind::SceneSelection;
        RobotQtViewerViewportInteractionMode defaultViewportMode =
            RobotQtViewerViewportInteractionMode::Browse;
        QString id;
        QString displayName;
        QString rightPanelTitle;
        bool projectAssemblyTreeProjection = true;
        bool collisionTreeProjection = false;
        bool projectAssemblyActions = true;
        bool collisionActions = false;
    };

    struct RobotQtViewerTaskSession
    {
        RobotQtViewerWorkbenchKind workbench = RobotQtViewerWorkbenchKind::Browse;
        QString workbenchId = QStringLiteral("smrobot.mode.project-assembly");
        RobotQtViewerWorkbenchDescriptor descriptor;
        RobotQtViewerViewportInteractionMode viewportMode = RobotQtViewerViewportInteractionMode::Browse;
        QString taskId;
        QString targetRobotId;
        QString targetLinkName;
        QString targetObjectId;
        QString targetMountId;
        QString targetAttachmentId;
        QString targetCollisionDetectorId;
        bool dirty = false;
        bool canExit = true;
    };

    QString robotQtViewerWorkbenchName(RobotQtViewerWorkbenchKind kind);
    QString robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind kind);
    bool robotQtViewerWorkbenchKindFromId(
        const QString& workbenchId,
        RobotQtViewerWorkbenchKind* kind);
    const RobotQtViewerWorkbenchDescriptor& robotQtViewerWorkbenchDescriptor(
        RobotQtViewerWorkbenchKind kind);
    QString robotQtViewerViewportInteractionModeName(RobotQtViewerViewportInteractionMode mode);

    class RobotQtViewerWorkbenchManager
    {
    public:
        RobotQtViewerWorkbenchKind activeWorkbench() const;
        QString activeWorkbenchId() const;
        const RobotQtViewerWorkbenchDescriptor& activeDescriptor() const;
        RobotQtViewerViewportInteractionMode viewportMode() const;
        const RobotQtViewerTaskSession& session() const;

        bool enterWorkbench(RobotQtViewerWorkbenchKind kind, const QString& sourceId = QString());
        bool enterWorkbench(
            const QString& workbenchId,
            const RobotQtViewerWorkbenchDescriptor& descriptor,
            const QString& sourceId = QString());
        void setInitialWorkbench(RobotQtViewerWorkbenchKind kind);
        void setInitialWorkbench(
            const QString& workbenchId,
            const RobotQtViewerWorkbenchDescriptor& descriptor);
        void commitWorkbench(RobotQtViewerWorkbenchKind kind, const QString& sourceId = QString());
        void commitWorkbench(
            const QString& workbenchId,
            const RobotQtViewerWorkbenchDescriptor& descriptor,
            const QString& sourceId = QString());
        bool exitToBrowse(const QString& sourceId = QString());
        void releaseProjectSessions();
        bool canExitActiveWorkbench() const;
        void setSessionDirty(bool dirty);
        void setSessionCanExit(bool canExit);
        void setViewportMode(RobotQtViewerViewportInteractionMode mode);
        void setSessionTarget(
            const QString& robotId,
            const QString& linkName = QString(),
            const QString& mountId = QString(),
            const QString& attachmentId = QString());

    private:
        RobotQtViewerTaskSession m_session;
        std::map<QString, RobotQtViewerTaskSession> m_suspendedSessions;
    };
}

