#pragma once

#include "ToolSetupViewModel.h"

#include <SimulationProject/ProjectAttachmentCommands.h>

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

namespace robot_qt_viewer
{
    class ToolSetupModuleController;

    class ToolSetupMountFrameTaskController final : public QObject
    {
    public:
        ToolSetupMountFrameTaskController(
            ToolSetupModuleController& host,
            QObject* parent);

        void handleSelectionChanged(int index);
        void focusTask(
            const QString& robotId,
            const QString& linkName,
            const QString& preferredMountId);
        void createForSelectedLink();
        void deleteCurrent();
        void captureSnapshot(const QString& mountId, bool newMount = false);
        void clearSnapshot();
        void enterViewportFocus(const QString& robotId, const QString& linkName);
        void clearViewportFocus();
        void updateFrameVisibility();
        void syncPinnedFrames();
        QStringList pinnedFrameIds() const;
        void reset() noexcept;

        ToolSetupMountFrameMode mode() const;
        void setMode(ToolSetupMountFrameMode mode);
        bool hasSnapshot() const;
        bool snapshotIsNew() const;
        const simulation_project::RobotMountDesc& snapshot() const;
        const QString& activeEditId() const;
        const QString& draftSourceRobotId() const;
        const QString& draftSourceLinkName() const;
        bool isPinned(const QString& mountId) const;
        void setPinned(const QString& mountId, bool pinned);
        void replacePinnedId(const QString& oldId, const QString& newId);

    private:
        ToolSetupModuleController& m_host;
        bool m_hasSnapshot = false;
        bool m_snapshotIsNew = false;
        ToolSetupMountFrameMode m_mode = ToolSetupMountFrameMode::Selection;
        QString m_activeEditId;
        QString m_draftSourceRobotId;
        QString m_draftSourceLinkName;
        QSet<QString> m_pinnedFrameIds;
        simulation_project::RobotMountDesc m_snapshot;
    };

    class ToolSetupTaskSessionCoordinator final : public QObject
    {
    public:
        ToolSetupTaskSessionCoordinator(
            ToolSetupModuleController& host,
            QObject* parent);

        bool dirty() const;
        bool resolvePendingChanges(QWidget* parentWidget, bool restoreEditorTarget);
        bool applyPendingChanges();
        void discardPendingChanges(const QString& message, bool restoreEditorTarget);
        void cancelTask();
        void requestTaskExit();
        void setDirty(bool dirty, const QString& message);
        void releaseProjectState() noexcept;

    private:
        ToolSetupModuleController& m_host;
        bool m_dirty = false;
    };

    class ToolSetupObjectBindingTaskController final : public QObject
    {
    public:
        ToolSetupObjectBindingTaskController(
            ToolSetupModuleController& host,
            QObject* parent);

        void focusTask(
            const QString& preferredMountId,
            const QString& preferredObjectId,
            const QString& preferredFrameId);
        void handleSelectionChanged(
            const QString& mountId,
            const QString& objectId,
            const QString& frameId);
        bool preview(
            const QString& mountId,
            const QString& objectId,
            const QString& frameId,
            QString* attachmentId = nullptr,
            QString* assetId = nullptr);
        void refreshEditor(
            const QString& mountId,
            const QString& objectId,
            const QString& frameId);
        void apply(
            const QString& mountId,
            const QString& objectId,
            const QString& frameId);
        void clearPreview();
        void reset() noexcept;

        bool active() const;
        bool hasDraft() const;
        const QString& mountId() const;
        const QString& objectId() const;
        const QString& frameId() const;

    private:
        ToolSetupModuleController& m_host;
        bool m_hasDraft = false;
        bool m_active = false;
        QString m_mountId;
        QString m_objectId;
        QString m_frameId;
        simulation_project::BindFramesRequest m_draft;
    };

    class ToolSetupInstalledDeviceTaskController final : public QObject
    {
    public:
        ToolSetupInstalledDeviceTaskController(
            ToolSetupModuleController& host,
            QObject* parent);

        void handleSelectionChanged(int index);
        void attachExistingAsset();
        void rebindCurrent();
        void configureCurrent();
        void selectById(const std::string& attachmentId);
        void unbind(const QString& attachmentId);

    private:
        ToolSetupModuleController& m_host;
    };

    class ToolSetupAttachmentDefinitionTaskController final : public QObject
    {
    public:
        ToolSetupAttachmentDefinitionTaskController(
            ToolSetupModuleController& host,
            QObject* parent);

        void handleSelectionChanged(int index);
        void importAsset();
        void duplicateCurrent();
        void editCurrent();
        bool editById(const std::string& assetId);
        void createFromSceneObject(const QString& objectId);

    private:
        ToolSetupModuleController& m_host;
    };
}
