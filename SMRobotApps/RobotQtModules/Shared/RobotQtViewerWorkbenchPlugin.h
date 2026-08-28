#pragma once

#include "RobotQtViewerLocalization.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtPlugin>

#include <filesystem>
#include <memory>
#include <vector>

class QPluginLoader;
class QWidget;

namespace robot_qt_viewer
{
    inline constexpr const char* kRobotQtViewerWorkbenchPluginIid =
        "org.smrobot.RobotQtViewer.WorkbenchPlugin/1.0";

    struct RobotQtViewerWorkbenchPluginModeDesc
    {
        QString id;
        QString displayName;
        QString rightPanelTitle;
        QString toolbarActionId;
        QStringList featureIds;
        QStringList requiredModeIds;
        QStringList conflictsWithModeIds;
        int defaultOrder = 0;
    };

    class IRobotQtViewerWorkbenchPlugin
    {
    public:
        virtual ~IRobotQtViewerWorkbenchPlugin() = default;

        virtual QString extensionApiVersion() const = 0;
        virtual RobotQtViewerWorkbenchPackageDesc packageDescriptor() const = 0;
        virtual QVector<RobotQtViewerWorkbenchPluginModeDesc> modeDescriptors() const = 0;
        virtual IRobotQtViewerWorkbenchLifecycle* lifecycle(const QString& modeId) = 0;
        virtual IRobotQtViewerLanguageParticipant* languageParticipant(
            const QString& modeId) = 0;
        virtual QWidget* createPanel(const QString& modeId, QWidget* parent) = 0;
    };

    class RobotQtViewerWorkbenchPluginLoader
    {
    public:
        RobotQtViewerWorkbenchPluginLoader();
        ~RobotQtViewerWorkbenchPluginLoader();

        RobotQtViewerWorkbenchPluginLoader(
            const RobotQtViewerWorkbenchPluginLoader&) = delete;
        RobotQtViewerWorkbenchPluginLoader& operator=(
            const RobotQtViewerWorkbenchPluginLoader&) = delete;

        bool discover(
            const std::filesystem::path& workbenchesDirectory,
            RobotQtViewerWorkbenchPackageRegistry& registry,
            QStringList* diagnostics = nullptr);
        bool loadEnabled(
            const QStringList& enabledWorkbenchIds,
            RobotQtViewerWorkbenchPackageRegistry& registry,
            QString* errorMessage = nullptr);

        QStringList discoveredWorkbenchIds() const;
        QStringList loadedWorkbenchIds() const;
        QWidget* createPanel(const QString& workbenchId, QWidget* parent) const;
        bool ownsWorkbench(const QString& workbenchId) const;

    private:
        struct Record;
        std::vector<std::unique_ptr<Record>> m_records;
    };
}

Q_DECLARE_INTERFACE(
    robot_qt_viewer::IRobotQtViewerWorkbenchPlugin,
    "org.smrobot.RobotQtViewer.WorkbenchPlugin/1.0")
