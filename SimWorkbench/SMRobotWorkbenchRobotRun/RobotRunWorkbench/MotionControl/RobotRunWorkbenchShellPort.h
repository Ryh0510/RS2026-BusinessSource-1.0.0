#pragma once

#include <QString>
#include <QStringList>

namespace robot_qt_viewer
{
    class RobotRunWorkbenchShellPort
    {
    public:
        virtual ~RobotRunWorkbenchShellPort() = default;

        virtual void setRobotRuntime(
            const QString& robotId,
            const QStringList& movableJoints,
            const QStringList& movableJointTypes) = 0;
        virtual void handleRobotStateUpdated() = 0;
        virtual void clearRuntime() = 0;
    };
}
