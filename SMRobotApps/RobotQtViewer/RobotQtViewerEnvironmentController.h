#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <ProjectSceneEnvironment.h>

class QAction;
class QActionGroup;
class QMenu;
class QWidget;
class RobotViewport;

namespace robot_qt_viewer
{
    class RobotQtViewerLocalizationService;
    class RobotQtViewerOperationStatusStore;

    bool readEnvironmentSelection(const QString& source,
        ProjectSceneEnvironmentSelection& selection, QString& error);

    class RobotQtViewerEnvironmentController final : public QObject
    {
    public:
        RobotQtViewerEnvironmentController(RobotViewport& viewport, QMenu& menu,
            RobotQtViewerLocalizationService& localization,
            RobotQtViewerOperationStatusStore& operations, QWidget& window);
        void retranslateUi();
        bool selectSource(const QString& source, bool remember = true, QString* error = nullptr);
    private:
        void refresh();
        void chooseFile();
        void remember();
        RobotViewport& m_viewport;
        RobotQtViewerLocalizationService& m_localization;
        RobotQtViewerOperationStatusStore& m_operations;
        QWidget& m_window;
        QActionGroup* m_group = nullptr;
        QHash<QString, QAction*> m_actions;
        QAction* m_load = nullptr;
        QAction* m_custom = nullptr;
        QAction* m_cutaway = nullptr;
        QString m_restoreSource;
    };
}
