#include "RobotQtViewerViewportEventController.h"

#include "RobotQtViewerViewportPreviewState.h"
#include "RobotQtViewerViewportPorts.h"

#include <utility>

namespace robot_qt_viewer
{
    RobotQtViewerViewportEventController::RobotQtViewerViewportEventController(
        IRobotQtViewerSelectionViewportPort& selectionViewport,
        IRobotQtViewerAssemblyViewportPort& assemblyViewport,
        IRobotQtViewerCollisionViewportPort& collisionViewport,
        const RobotQtViewerViewportPreviewState& viewportPreviewState,
        LinkFrameVisibleQuery linkFrameVisible,
        QObject* parent)
        : QObject(parent)
        , m_selectionViewport(selectionViewport)
        , m_assemblyViewport(assemblyViewport)
        , m_collisionViewport(collisionViewport)
        , m_viewportPreviewState(viewportPreviewState)
        , m_linkFrameVisible(std::move(linkFrameVisible))
    {
    }

    void RobotQtViewerViewportEventController::handleEvent(const RobotQtViewerEvent& event)
    {
        switch(event.kind) {
        case RobotQtViewerEventKind::SelectionChanged:
            applySelection(event.selection);
            break;
        case RobotQtViewerEventKind::ViewportPreviewChanged:
            applyViewportPreview(m_viewportPreviewState.lastMutation());
            break;
        case RobotQtViewerEventKind::ViewportReloaded:
            if(!event.viewport.reloadSucceeded) {
                applySelection(RobotQtViewerSelectionPayload());
            }
            break;
        default:
            break;
        }
    }

    void RobotQtViewerViewportEventController::applySelection(
        const RobotQtViewerSelectionPayload& selection)
    {
        m_selectionViewport.setActiveToolFrameRobot(selection.robotId);
        if(selection.attachmentId.isEmpty()) {
            m_assemblyViewport.setActiveMountedAttachment(QString());
        }
        if(selection.objectFrameId.isEmpty()) {
            m_selectionViewport.clearObjectFrameObjectFocus();
            m_selectionViewport.selectObjectFrame(QString(), QString());
        }

        if(selection.jointName.isEmpty()) {
            m_selectionViewport.selectRobotJointFrame(QString(), QString());
        }

        if(!selection.objectFrameId.isEmpty()) {
            m_assemblyViewport.setActivePreviewRobotMount(QString());
            m_assemblyViewport.setRobotMountFrameVisibility(false, false);
            m_selectionViewport.clearObjectFrameObjectFocus();
            m_selectionViewport.selectObjectFrame(selection.objectId, selection.objectFrameId);
            return;
        }

        if(!selection.jointName.isEmpty()) {
            m_assemblyViewport.setActivePreviewRobotMount(QString());
            m_assemblyViewport.setRobotMountFrameVisibility(false, false);
            m_selectionViewport.selectRobotJointFrame(selection.robotId, selection.jointName);
            return;
        }

        if(!selection.attachmentId.isEmpty()) {
            m_assemblyViewport.setActivePreviewRobotMount(QString());
            m_assemblyViewport.setRobotMountFrameVisibility(false, false);
            m_selectionViewport.selectMountedAttachment(selection.attachmentId);
            m_assemblyViewport.setActiveMountedAttachment(selection.attachmentId);
            return;
        }

        if(!selection.mountId.isEmpty()) {
            m_selectionViewport.selectRobotMount(selection.robotId, selection.linkName, selection.mountId);
            m_assemblyViewport.setActivePreviewRobotMount(QString());
            m_assemblyViewport.setRobotMountFrameVisibility(false, false);
            return;
        }

        if(!selection.objectId.isEmpty()) {
            m_assemblyViewport.setActivePreviewRobotMount(QString());
            m_assemblyViewport.setRobotMountFrameVisibility(false, false);
            m_selectionViewport.selectSceneObject(selection.objectId);
            return;
        }

        if(!selection.robotId.isEmpty()) {
            const bool showLinkFrame =
                m_linkFrameVisible && m_linkFrameVisible(selection.robotId, selection.linkName);
            m_selectionViewport.selectRobotLink(selection.robotId, selection.linkName);
            m_assemblyViewport.setActivePreviewRobotMount(QString());
            m_assemblyViewport.setRobotMountFrameVisibility(showLinkFrame, false);
            return;
        }

        m_selectionViewport.selectRobotLink(QString(), QString());
        m_assemblyViewport.setActivePreviewRobotMount(QString());
        m_assemblyViewport.setRobotMountFrameVisibility(false, false);
    }

    void RobotQtViewerViewportEventController::applyViewportPreview(
        const RobotQtViewerViewportPreviewPayload& preview)
    {
        if(preview.removePreviewRobotMount) {
            m_assemblyViewport.removePreviewRobotMount(preview.removePreviewRobotMountId);
        }
        if(preview.upsertPreviewRobotMount) {
            m_assemblyViewport.upsertPreviewRobotMount(preview.robotMount);
        }
        if(preview.previewRobotMountTransform) {
            m_assemblyViewport.previewRobotMountTransform(
                preview.previewRobotMountId,
                preview.robotMountTransform);
        }
        if(preview.previewMountedAttachmentTransform) {
            m_assemblyViewport.previewMountedAttachmentTransform(
                preview.previewMountedAttachmentId,
                preview.mountedAttachmentTransform);
        }
        if(preview.previewAttachmentAsset) {
            m_assemblyViewport.previewAttachmentAsset(preview.attachmentAsset);
        }
        if(preview.clearAttachmentBindingPreview) {
            m_assemblyViewport.clearAttachmentBindingPreview();
        }
        if(preview.previewAttachmentBinding) {
            m_assemblyViewport.previewAttachmentBinding(preview.attachmentBinding);
        }
        if(preview.setActivePreviewRobotMount) {
            m_assemblyViewport.setActivePreviewRobotMount(preview.activePreviewRobotMountId);
        }
        if(preview.setRobotMountFrameVisibility) {
            m_assemblyViewport.setRobotMountFrameVisibility(
                preview.selectedLinkFrameVisible,
                preview.mountFrameVisible);
        }
        if(preview.upsertPreviewObjectFrame) {
            m_assemblyViewport.upsertPreviewObjectFrame(
                preview.objectFrameObjectId,
                preview.objectFrame);
        }
        if(preview.previewRobotBaseTransform) {
            m_assemblyViewport.previewRobotBaseTransform(
                preview.robotBaseRobotId,
                preview.robotBaseTransform);
        }
        if(preview.previewObjectFrameTransform) {
            m_assemblyViewport.previewObjectFrameTransform(
                preview.previewObjectFrameObjectId,
                preview.previewObjectFrameId,
                preview.objectFrameTransform);
        }
        if(preview.previewSceneObjectTransform) {
            m_assemblyViewport.previewSceneObjectTransform(
                preview.sceneObjectId,
                preview.sceneObjectTransform);
        }
        if(preview.clearObjectFrameObjectFocus) {
            m_selectionViewport.clearObjectFrameObjectFocus();
        }
        if(preview.focusObjectFrameObject) {
            m_selectionViewport.focusObjectFrameObject(preview.focusObjectFrameObjectId);
        }
        if(preview.clearMountFrameLinkFocus) {
            m_selectionViewport.clearMountFrameLinkFocus();
        }
        if(preview.focusMountFrameLink) {
            m_selectionViewport.focusMountFrameLink(
                preview.focusMountFrameRobotId,
                preview.focusMountFrameLinkName);
        }
        if(preview.clearMountedAttachmentFocus) {
            m_selectionViewport.clearMountedAttachmentFocus();
        }
        if(preview.focusMountedAttachment) {
            m_selectionViewport.focusMountedAttachment(preview.focusMountedAttachmentId);
        }
        if(preview.clearObjectCollisionModelVariantPreview) {
            m_collisionViewport.clearObjectCollisionModelVariantPreview();
        }
        if(preview.previewObjectCollisionModelVariant) {
            m_collisionViewport.previewObjectCollisionModelVariant(
                preview.previewObjectCollisionModelObjectId,
                preview.previewObjectCollisionModelVariantId);
        }
        if(preview.setActiveMountedAttachment) {
            m_assemblyViewport.setActiveMountedAttachment(preview.activeMountedAttachmentId);
        }
        if(preview.setToolFrameVisibility) {
            m_assemblyViewport.setToolFrameVisibility(preview.toolFrameVisibility);
        }
        if(preview.setPinnedRobotMountFrames) {
            m_assemblyViewport.setPinnedRobotMountFrames(preview.pinnedRobotMountFrameIds);
        }
    }
}
