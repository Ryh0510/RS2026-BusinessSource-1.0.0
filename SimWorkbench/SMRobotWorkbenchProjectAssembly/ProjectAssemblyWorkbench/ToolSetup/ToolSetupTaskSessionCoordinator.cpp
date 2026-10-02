#include "ToolSetupTaskControllers.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportPreviewState.h"
#include "ToolSetupAppServices.h"
#include "ToolSetupModuleController.h"
#include "ToolSetupWidget.h"

#include <QMessageBox>

namespace robot_qt_viewer
{
    ToolSetupTaskSessionCoordinator::ToolSetupTaskSessionCoordinator(
        ToolSetupModuleController& host,
        QObject* parent)
        : QObject(parent)
        , m_host(host)
    {
    }

    bool ToolSetupTaskSessionCoordinator::dirty() const
    {
        return m_dirty;
    }

    bool ToolSetupTaskSessionCoordinator::resolvePendingChanges(
        QWidget* parentWidget,
        bool restoreEditorTarget)
    {
        if(!m_dirty) {
            return true;
        }

        const bool objectBindingActive = m_host.m_objectBindingTask->active();
        const QMessageBox::StandardButton result = QMessageBox::question(
            parentWidget != nullptr ? parentWidget : &m_host.m_widget,
            objectBindingActive
                ? QStringLiteral("Unsaved Object Binding")
                : QStringLiteral("Unsaved Frame Editor Changes"),
            objectBindingActive
                ? QStringLiteral("Apply the current object binding before leaving Frame Editor?")
                : QStringLiteral("Save changes to the current mount frame before leaving Frame Editor?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if(result == QMessageBox::Cancel) {
            return false;
        }
        if(result == QMessageBox::Discard) {
            discardPendingChanges(
                QStringLiteral("Discarded unsaved frame edits."),
                restoreEditorTarget);
            return true;
        }

        return applyPendingChanges();
    }

    bool ToolSetupTaskSessionCoordinator::applyPendingChanges()
    {
        if(!m_dirty) {
            return true;
        }

        if(m_host.m_objectBindingTask->active()) {
            m_host.m_objectBindingTask->apply(
                m_host.m_objectBindingTask->mountId(),
                m_host.m_objectBindingTask->objectId(),
                m_host.m_objectBindingTask->frameId());
            return !m_dirty;
        }

        m_host.applyTaskChanges(
            m_host.m_widget.hasMountTransformEditor(),
            m_host.m_widget.currentMountTransform(),
            m_host.m_widget.hasAttachmentInstanceEditor(),
            m_host.m_widget.attachmentInstance(),
            m_host.m_widget.hasAttachmentOffsetEditor(),
            m_host.m_widget.currentAttachmentOffset(),
            m_host.m_widget.hasAssetEditor(),
            m_host.m_widget.currentAssetEditorAsset());
        return !m_dirty;
    }

    void ToolSetupTaskSessionCoordinator::discardPendingChanges(
        const QString& message,
        bool restoreEditorTarget)
    {
        const bool discardingObjectBinding = m_host.m_objectBindingTask->active();
        const bool discardingNewMount = m_host.m_mountFrameTask->snapshotIsNew();
        const QString draftSourceRobotId = m_host.m_mountFrameTask->draftSourceRobotId();
        const QString draftSourceLinkName = m_host.m_mountFrameTask->draftSourceLinkName();
        RobotQtViewerViewportPreviewPayload preview;
        if(m_host.m_mountFrameTask->snapshotIsNew()) {
            m_host.m_mountFrameTask->setPinned(
                m_host.m_mountFrameTask->activeEditId(),
                false);
            preview.removePreviewRobotMount = true;
            preview.removePreviewRobotMountId = m_host.m_mountFrameTask->activeEditId();
            m_host.mutateViewportPreview(preview, QStringLiteral("discardMountFrameDraft"));
        } else if(m_host.m_mountFrameTask->hasSnapshot()) {
            preview.upsertPreviewRobotMount = true;
            preview.robotMount = m_host.m_mountFrameTask->snapshot();
            preview.previewRobotMountTransform = true;
            preview.previewRobotMountId = m_host.m_mountFrameTask->activeEditId();
            preview.robotMountTransform = m_host.m_mountFrameTask->snapshot().linkToMount;
            preview.setActivePreviewRobotMount = true;
            preview.activePreviewRobotMountId = m_host.m_mountFrameTask->activeEditId();
            m_host.mutateViewportPreview(preview, QStringLiteral("discardMountFrameDraft"));
        }

        const QString robotId = m_host.m_mountFrameTask->hasSnapshot()
            ? QString::fromStdString(m_host.m_mountFrameTask->snapshot().robotId)
            : m_host.m_appServices.selectedRobotId();
        const QString linkName = m_host.m_mountFrameTask->hasSnapshot()
            ? QString::fromStdString(m_host.m_mountFrameTask->snapshot().linkName)
            : m_host.m_appServices.selectedLinkName();
        const QString mountId = m_host.m_mountFrameTask->snapshotIsNew()
            ? QString()
            : m_host.m_mountFrameTask->activeEditId();

        setDirty(false, message);
        m_host.m_mountFrameTask->setMode(ToolSetupMountFrameMode::Selection);
        m_host.m_objectBindingTask->reset();
        m_host.m_widget.setObjectBindingEditor(
            false,
            ToolSetupBindingView(),
            QVector<ToolSetupComboItem>(),
            QString(),
            QVector<ToolSetupComboItem>(),
            QString(),
            QVector<ToolSetupComboItem>(),
            QString(),
            QString(),
            false);
        m_host.m_widget.setObjectBindingMode(false);
        m_host.clearMountEditSnapshot();
        if(discardingObjectBinding) {
            m_host.clearObjectBindingPreviewViewportState();
        }
        if(!restoreEditorTarget) {
            m_host.clearMountFrameViewportFocus();
            if(discardingNewMount && !draftSourceRobotId.isEmpty() && !draftSourceLinkName.isEmpty()) {
                m_host.m_appServices.setRobotMountContext(
                    draftSourceRobotId,
                    draftSourceLinkName,
                    QString());
                m_host.m_context.selectionModel().selectRobotLink(
                    draftSourceRobotId,
                    draftSourceLinkName,
                    QStringLiteral("discardMountFrameDraft"));
                emit m_host.robotContextSelected(draftSourceRobotId);
                emit m_host.linkFocusRequested(draftSourceRobotId, draftSourceLinkName);
            }
            emit m_host.selectionDependentViewsRefreshRequested();
            return;
        }
        if(!robotId.isEmpty()) {
            m_host.m_appServices.setRobotMountContext(robotId, linkName, mountId);
        }
        m_host.refresh(robotId, linkName, mountId);
        if(!mountId.isEmpty()) {
            m_host.enterMountFrameViewportFocus(robotId, linkName);
            emit m_host.mountFrameFocusRequested(robotId, linkName, mountId);
        } else {
            m_host.clearMountFrameViewportFocus();
        }
        emit m_host.selectionDependentViewsRefreshRequested();
    }

    void ToolSetupTaskSessionCoordinator::cancelTask()
    {
        discardPendingChanges(QStringLiteral("Canceled unsaved frame edits."), false);
        emit m_host.statusMessageRequested(QStringLiteral("Mount setup edits canceled."), 3000);
        emit m_host.taskExitRequested();
    }

    void ToolSetupTaskSessionCoordinator::requestTaskExit()
    {
        if(!resolvePendingChanges(&m_host.m_widget, true)) {
            return;
        }
        m_host.clearMountFrameViewportFocus();
        emit m_host.taskExitRequested();
    }

    void ToolSetupTaskSessionCoordinator::setDirty(bool dirty, const QString& message)
    {
        if(m_dirty == dirty && message.isEmpty()) {
            return;
        }
        m_dirty = dirty;
        m_host.m_widget.setTaskDirty(dirty, message);
        emit m_host.taskDirtyChanged(dirty);
    }

    void ToolSetupTaskSessionCoordinator::releaseProjectState() noexcept
    {
        if(m_host.m_objectBindingTask->active() || m_host.m_objectBindingTask->hasDraft()) {
            m_host.m_objectBindingTask->clearPreview();
        }
        m_dirty = false;
        m_host.m_mountFrameTask->reset();
        m_host.m_objectBindingTask->reset();
    }
}
