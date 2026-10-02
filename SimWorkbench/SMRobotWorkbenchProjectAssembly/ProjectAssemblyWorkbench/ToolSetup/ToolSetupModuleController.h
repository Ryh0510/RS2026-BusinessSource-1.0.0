#pragma once

#include "ToolSetupWorkbenchShellPort.h"

#include "RobotQtViewerEvents.h"
#include "ToolSetupViewModel.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <SimulationProject/ProjectAttachmentCommands.h>
#include <SimulationProject/ProjectDocument.h>

#include <string>

class ToolSetupWidget;
class QWidget;

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentContext;
    class ToolSetupAppServices;
    class ToolSetupAttachmentDefinitionTaskController;
    class ToolSetupInstalledDeviceTaskController;
    class ToolSetupMountFrameTaskController;
    class ToolSetupObjectBindingTaskController;
    class ToolSetupTaskSessionCoordinator;

    class ToolSetupModuleController : public QObject, public ToolSetupWorkbenchShellPort
    {
        Q_OBJECT

    public:
        ToolSetupModuleController(
            ToolSetupWidget& widget,
            RobotQtViewerDocumentContext& context,
            ToolSetupAppServices& appServices,
            QObject* parent = nullptr);

        void refresh(
            const QString& selectedRobotId,
            const QString& selectedLinkName,
            const QString& preferredMountId = QString(),
            const QString& preferredAttachmentId = QString(),
            const QString& preferredAssetId = QString());
        void handleEvent(
            const RobotQtViewerEvent& event,
            const QString& selectedRobotId,
            const QString& selectedLinkName);
        void importToolAsset();
        void attachExistingToolAsset();
        void focusRobotMountTask(
            const QString& robotId,
            const QString& linkName = QString(),
            const QString& preferredMountId = QString());
        void createRobotMountForSelectedLink();
        void deleteCurrentRobotMount();
        void configureCurrentToolAttachment();
        void selectToolAttachmentById(const std::string& attachmentId);
        void editCurrentToolAsset();
        bool editToolAssetById(const std::string& assetId);
        void createToolAssetFromSceneObject(const QString& objectId);
        void focusObjectBindingTask(
            const QString& preferredMountId = QString(),
            const QString& preferredObjectId = QString(),
            const QString& preferredFrameId = QString());
        void unbindMountedAttachment(const QString& attachmentId);
        void updateToolFrameVisibility();
        QString currentMountId() const override;
        QString currentAttachmentId() const override;
        bool hasPendingTaskChanges() const;
        bool resolvePendingTaskChanges(QWidget* parentWidget = nullptr, bool restoreEditorTarget = true);
        void releaseProjectState() noexcept;

    signals:
        void frameVisibilityChanged();
        void viewModelRefreshed();
        void robotContextSelected(const QString& robotId);
        void mountFrameFocusRequested(
            const QString& robotId,
            const QString& linkName,
            const QString& mountId);
        void linkFocusRequested(
            const QString& robotId,
            const QString& linkName);
        void selectionDependentViewsRefreshRequested();
        void taskDirtyChanged(bool dirty);
        void taskExitRequested();
        void statusMessageRequested(const QString& message, int timeoutMs);

    private:
        friend class ToolSetupObjectBindingTaskController;
        friend class ToolSetupAttachmentDefinitionTaskController;
        friend class ToolSetupInstalledDeviceTaskController;
        friend class ToolSetupMountFrameTaskController;
        friend class ToolSetupTaskSessionCoordinator;

        void handleMountSelectionChanged(int index);
        void handleAttachmentSelectionChanged(int index);
        void handleAssetSelectionChanged(int index);
        void duplicateCurrentAsset();
        void rebindCurrentAttachment();
        bool applyPendingTaskChanges();
        void discardPendingTaskChanges(const QString& message = QString(), bool restoreEditorTarget = true);
        void handleObjectBindingSelectionChanged(
            const QString& mountId,
            const QString& objectId,
            const QString& frameId);
        void applyObjectBinding(
            const QString& mountId,
            const QString& objectId,
            const QString& frameId);
        void applyTaskChanges(
            bool hasMountTransform,
            const simulation_project::TransformDesc& mountTransform,
            bool hasAttachmentInstance,
            const simulation_project::MountedAttachmentDesc& attachmentInstance,
            bool hasAttachmentOffset,
            const simulation_project::TransformDesc& attachmentOffset,
            bool hasToolAsset,
            const simulation_project::AttachmentAssetDesc& toolAsset);
        void cancelTaskChanges();
        void requestTaskExit();
        void setTaskDirty(bool dirty, const QString& message = QString());
        void captureMountEditSnapshot(const QString& mountId, bool newMount = false);
        void clearMountEditSnapshot();
        void enterMountFrameViewportFocus(const QString& robotId, const QString& linkName);
        void clearMountFrameViewportFocus();
        void clearObjectBindingPreviewViewportState();
        void mutateViewportPreview(
            const RobotQtViewerViewportPreviewPayload& preview,
            const QString& sourceId);
        void syncPinnedRobotMountFrames();
        QStringList pinnedRobotMountFrameIds() const;
        bool previewObjectBinding(
            const QString& mountId,
            const QString& objectId,
            const QString& frameId,
            QString* attachmentId = nullptr,
            QString* assetId = nullptr);
        void refreshObjectBindingEditor(
            const QString& mountId,
            const QString& objectId,
            const QString& frameId);
        ToolSetupWidget& m_widget;
        RobotQtViewerDocumentContext& m_context;
        ToolSetupAppServices& m_appServices;
        bool m_updating = false;
        ToolSetupObjectBindingTaskController* m_objectBindingTask = nullptr;
        ToolSetupAttachmentDefinitionTaskController* m_attachmentDefinitionTask = nullptr;
        ToolSetupInstalledDeviceTaskController* m_installedDeviceTask = nullptr;
        ToolSetupMountFrameTaskController* m_mountFrameTask = nullptr;
        ToolSetupTaskSessionCoordinator* m_taskSession = nullptr;
    };
}
