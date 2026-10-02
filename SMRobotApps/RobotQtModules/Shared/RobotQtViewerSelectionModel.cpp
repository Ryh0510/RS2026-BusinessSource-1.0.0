#include "RobotQtViewerSelectionModel.h"

namespace robot_qt_viewer
{
    RobotQtViewerSelectionModel::RobotQtViewerSelectionModel(RobotQtViewerEventHub& eventHub)
        : m_eventHub(eventHub)
    {
    }

    void RobotQtViewerSelectionModel::setEditSessionCoordinator(
        RobotQtViewerEditSessionCoordinator* coordinator,
        QWidget* promptParent)
    {
        m_editSessionCoordinator = coordinator;
        m_promptParent = promptParent;
    }

    const RobotQtViewerSelectionState& RobotQtViewerSelectionModel::state() const
    {
        return m_state;
    }

    RobotQtViewerSelectionPayload RobotQtViewerSelectionModel::payload() const
    {
        RobotQtViewerSelectionPayload value;
        value.robotId = m_state.robotId;
        value.linkName = m_state.linkName;
        value.objectId = m_state.objectId;
        value.objectFrameId = m_objectFrameId;
        value.mountId = m_state.mountId;
        value.attachmentId = m_state.attachmentId;
        value.assetId = m_toolAssetId;
        value.jointName = m_jointName;
        value.collisionDetectorId = m_collisionDetectorId;
        value.collisionPairRobotA = m_collisionPairRobotA;
        value.collisionPairLinkA = m_collisionPairLinkA;
        return value;
    }

    void RobotQtViewerSelectionModel::clear(const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_state = {};
        m_toolAssetId.clear();
        m_objectFrameId.clear();
        m_jointName.clear();
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::selectRobotLink(
        const QString& robotId,
        const QString& linkName,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_state = {};
        m_state.kind = RobotQtViewerSelectionKind::RobotLink;
        m_state.robotId = robotId;
        m_state.linkName = linkName;
        m_toolAssetId.clear();
        m_objectFrameId.clear();
        m_jointName.clear();
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::selectRobotMount(
        const QString& robotId,
        const QString& linkName,
        const QString& mountId,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_state = {};
        m_state.kind = RobotQtViewerSelectionKind::RobotMount;
        m_state.robotId = robotId;
        m_state.linkName = linkName;
        m_state.mountId = mountId;
        m_toolAssetId.clear();
        m_objectFrameId.clear();
        m_jointName.clear();
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::selectMountedAttachment(
        const QString& attachmentId,
        const QString& robotId,
        const QString& linkName,
        const QString& mountId,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_state = {};
        m_state.kind = RobotQtViewerSelectionKind::MountedAttachment;
        m_state.attachmentId = attachmentId;
        m_state.robotId = robotId;
        m_state.linkName = linkName;
        m_state.mountId = mountId;
        m_toolAssetId.clear();
        m_objectFrameId.clear();
        m_jointName.clear();
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::selectSceneObject(
        const QString& objectId,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_state = {};
        m_state.kind = RobotQtViewerSelectionKind::SceneObject;
        m_state.objectId = objectId;
        m_toolAssetId.clear();
        m_objectFrameId.clear();
        m_jointName.clear();
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::selectObjectFrame(
        const QString& objectId,
        const QString& frameId,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_state = {};
        m_state.kind = RobotQtViewerSelectionKind::SceneObject;
        m_state.objectId = objectId;
        m_toolAssetId.clear();
        m_objectFrameId = frameId;
        m_jointName.clear();
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::selectRobotJoint(
        const QString& robotId,
        const QString& jointName,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_state = {};
        m_state.kind = RobotQtViewerSelectionKind::RobotLink;
        m_state.robotId = robotId;
        m_toolAssetId.clear();
        m_objectFrameId.clear();
        m_jointName = jointName;
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::selectToolAsset(
        const QString& assetId,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_state = {};
        m_toolAssetId = assetId;
        m_objectFrameId.clear();
        m_jointName.clear();
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::setCollisionDetector(
        const QString& detectorId,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_collisionDetectorId = detectorId;
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    void RobotQtViewerSelectionModel::setCollisionPairA(
        const QString& robotId,
        const QString& linkName,
        const QString& sourceId)
    {
        const Snapshot previous = capture();
        m_collisionPairRobotA = robotId;
        m_collisionPairLinkA = linkName;
        if(prepareCommit(previous, sourceId)) {
            publish(sourceId);
        }
    }

    RobotQtViewerSelectionModel::Snapshot RobotQtViewerSelectionModel::capture() const
    {
        Snapshot value;
        value.state = m_state;
        value.toolAssetId = m_toolAssetId;
        value.objectFrameId = m_objectFrameId;
        value.jointName = m_jointName;
        value.collisionDetectorId = m_collisionDetectorId;
        value.collisionPairRobotA = m_collisionPairRobotA;
        value.collisionPairLinkA = m_collisionPairLinkA;
        return value;
    }

    bool RobotQtViewerSelectionModel::prepareSelectionChange(const QString& sourceId)
    {
        if(m_editSessionCoordinator == nullptr) {
            return true;
        }

        RobotQtViewerEditTransitionRequest request;
        request.cause = RobotQtViewerWorkbenchTransitionCause::SelectionChange;
        request.sourceId = sourceId;
        request.promptParent = m_promptParent;
        return m_editSessionCoordinator->prepareTransition(request).succeeded();
    }

    void RobotQtViewerSelectionModel::restore(const Snapshot& snapshot)
    {
        m_state = snapshot.state;
        m_toolAssetId = snapshot.toolAssetId;
        m_objectFrameId = snapshot.objectFrameId;
        m_jointName = snapshot.jointName;
        m_collisionDetectorId = snapshot.collisionDetectorId;
        m_collisionPairRobotA = snapshot.collisionPairRobotA;
        m_collisionPairLinkA = snapshot.collisionPairLinkA;
    }

    bool RobotQtViewerSelectionModel::prepareCommit(
        const Snapshot& previous,
        const QString& sourceId)
    {
        const Snapshot current = capture();
        const bool changed =
            previous.state.kind != current.state.kind ||
            previous.state.robotId != current.state.robotId ||
            previous.state.linkName != current.state.linkName ||
            previous.state.mountId != current.state.mountId ||
            previous.state.attachmentId != current.state.attachmentId ||
            previous.state.objectId != current.state.objectId ||
            previous.toolAssetId != current.toolAssetId ||
            previous.objectFrameId != current.objectFrameId ||
            previous.jointName != current.jointName ||
            previous.collisionDetectorId != current.collisionDetectorId ||
            previous.collisionPairRobotA != current.collisionPairRobotA ||
            previous.collisionPairLinkA != current.collisionPairLinkA;
        if(!changed || m_editSessionCoordinator == nullptr) {
            return true;
        }

        if(prepareSelectionChange(sourceId)) {
            return true;
        }

        restore(previous);
        publish(sourceId + QStringLiteral(".rejected"));
        return false;
    }

    void RobotQtViewerSelectionModel::publish(const QString& sourceId)
    {
        RobotQtViewerEvent event;
        event.kind = RobotQtViewerEventKind::SelectionChanged;
        event.sourceId = sourceId;
        event.selection = payload();
        m_eventHub.publish(event);
    }
}
