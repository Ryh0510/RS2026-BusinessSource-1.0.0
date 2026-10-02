#pragma once

#include <QString>

#include <string>

namespace robot_qt_viewer
{
    class ToolSetupWorkbenchShellPort
    {
    public:
        virtual ~ToolSetupWorkbenchShellPort() = default;

        virtual void focusRobotMountTask(
            const QString& robotId,
            const QString& linkName = QString(),
            const QString& preferredMountId = QString()) = 0;
        virtual void createRobotMountForSelectedLink() = 0;
        virtual void selectToolAttachmentById(const std::string& attachmentId) = 0;
        virtual void createToolAssetFromSceneObject(const QString& objectId) = 0;
        virtual void focusObjectBindingTask(
            const QString& preferredMountId = QString(),
            const QString& preferredObjectId = QString(),
            const QString& preferredFrameId = QString()) = 0;
        virtual void unbindMountedAttachment(const QString& attachmentId) = 0;
        virtual void updateToolFrameVisibility() = 0;
        virtual QString currentMountId() const = 0;
        virtual QString currentAttachmentId() const = 0;
    };
}
