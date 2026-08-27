#pragma once

#include <QString>

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentContext;

    struct ViewportReloadWorkflowResult
    {
        bool success = false;
        bool showCollisionGeometry = false;
        int robotCount = 0;
        int objectCount = 0;
        int detectorCount = 0;
        long long elapsedMs = 0;
        QString errorMessage;
        QString sourceId;
        QString operationId;
    };

    class ViewportReloadWorkflowController
    {
    public:
        explicit ViewportReloadWorkflowController(RobotQtViewerDocumentContext& context);

        ViewportReloadWorkflowResult reload(
            const QString& sourceId,
            const QString& operationId = QString());

    private:
        RobotQtViewerDocumentContext& m_context;
    };
}
