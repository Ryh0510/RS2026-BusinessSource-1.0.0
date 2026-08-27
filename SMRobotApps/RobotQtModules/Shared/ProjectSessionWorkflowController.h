#pragma once

#include <QString>

#include <filesystem>

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentContext;

    struct ProjectSessionWorkflowResult
    {
        bool success = false;
        bool shouldReloadViewport = false;
        bool migratedCollisionDetectors = false;
        QString message;
        QString sourceId;
        QString operationId;
    };

    class ProjectSessionWorkflowController
    {
    public:
        explicit ProjectSessionWorkflowController(RobotQtViewerDocumentContext& context);

        ProjectSessionWorkflowResult resetNew(const QString& sourceId);
        ProjectSessionWorkflowResult loadFromPath(
            const std::filesystem::path& path,
            const QString& sourceId,
            const QString& operationId = QString());
        ProjectSessionWorkflowResult loadStartupProject(
            const std::filesystem::path& path,
            const QString& sourceId,
            const QString& operationId = QString());
        ProjectSessionWorkflowResult saveToPath(
            const std::filesystem::path& path,
            bool saveAsV3,
            const QString& sourceId);

    private:
        ProjectSessionWorkflowResult loadProject(
            const std::filesystem::path& path,
            const QString& sourceId,
            bool requireSaveAs,
            const QString& operationId);

        RobotQtViewerDocumentContext& m_context;
    };
}
