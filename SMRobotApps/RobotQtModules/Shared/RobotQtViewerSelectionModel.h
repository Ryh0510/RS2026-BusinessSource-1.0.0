#pragma once

#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerEditSession.h"
#include "RobotQtViewerEvents.h"

#include <QString>

namespace robot_qt_viewer
{
    enum class RobotQtViewerSelectionKind
    {
        None,
        RobotLink,
        RobotMount,
        MountedAttachment,
        SceneObject
    };

    struct RobotQtViewerSelectionState
    {
        RobotQtViewerSelectionKind kind = RobotQtViewerSelectionKind::None;
        QString robotId;
        QString linkName;
        QString mountId;
        QString attachmentId;
        QString objectId;
    };

    class RobotQtViewerSelectionModel
    {
    public:
        explicit RobotQtViewerSelectionModel(RobotQtViewerEventHub& eventHub);

        void setEditSessionCoordinator(
            RobotQtViewerEditSessionCoordinator* coordinator,
            QWidget* promptParent = nullptr);

        const RobotQtViewerSelectionState& state() const;
        RobotQtViewerSelectionPayload payload() const;
        bool prepareSelectionChange(const QString& sourceId = QString());

        void clear(const QString& sourceId = QString());
        void selectRobotLink(
            const QString& robotId,
            const QString& linkName,
            const QString& sourceId = QString());
        void selectRobotMount(
            const QString& robotId,
            const QString& linkName,
            const QString& mountId,
            const QString& sourceId = QString());
        void selectMountedAttachment(
            const QString& attachmentId,
            const QString& robotId,
            const QString& linkName,
            const QString& mountId,
            const QString& sourceId = QString());
        void selectSceneObject(
            const QString& objectId,
            const QString& sourceId = QString());
        void selectObjectFrame(
            const QString& objectId,
            const QString& frameId,
            const QString& sourceId = QString());
        void selectRobotJoint(
            const QString& robotId,
            const QString& jointName,
            const QString& sourceId = QString());
        void selectToolAsset(
            const QString& assetId,
            const QString& sourceId = QString());
        void setCollisionDetector(
            const QString& detectorId,
            const QString& sourceId = QString());
        void setCollisionPairA(
            const QString& robotId,
            const QString& linkName,
            const QString& sourceId = QString());

    private:
        struct Snapshot
        {
            RobotQtViewerSelectionState state;
            QString toolAssetId;
            QString objectFrameId;
            QString jointName;
            QString collisionDetectorId;
            QString collisionPairRobotA;
            QString collisionPairLinkA;
        };

        Snapshot capture() const;
        void restore(const Snapshot& snapshot);
        bool prepareCommit(const Snapshot& previous, const QString& sourceId);
        void publish(const QString& sourceId);

        RobotQtViewerEventHub& m_eventHub;
        RobotQtViewerSelectionState m_state;
        QString m_toolAssetId;
        QString m_objectFrameId;
        QString m_jointName;
        QString m_collisionDetectorId;
        QString m_collisionPairRobotA;
        QString m_collisionPairLinkA;
        RobotQtViewerEditSessionCoordinator* m_editSessionCoordinator = nullptr;
        QWidget* m_promptParent = nullptr;
    };
}
