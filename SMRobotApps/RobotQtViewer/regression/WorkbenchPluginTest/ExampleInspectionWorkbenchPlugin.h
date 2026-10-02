#pragma once

#include "RobotQtViewerWorkbenchPlugin.h"

#include <QObject>

class ExampleInspectionWorkbenchPlugin final
    : public QObject
    , public robot_qt_viewer::IRobotQtViewerWorkbenchPlugin
    , public robot_qt_viewer::IRobotQtViewerWorkbenchLifecycle
    , public robot_qt_viewer::IRobotQtViewerLanguageParticipant
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.smrobot.RobotQtViewer.WorkbenchPlugin/1.0")
    Q_INTERFACES(robot_qt_viewer::IRobotQtViewerWorkbenchPlugin)

public:
    QString extensionApiVersion() const override;
    robot_qt_viewer::RobotQtViewerWorkbenchPackageDesc packageDescriptor() const override;
    QVector<robot_qt_viewer::RobotQtViewerWorkbenchPluginModeDesc>
        modeDescriptors() const override;
    robot_qt_viewer::IRobotQtViewerWorkbenchLifecycle* lifecycle(
        const QString& modeId) override;
    robot_qt_viewer::IRobotQtViewerLanguageParticipant* languageParticipant(
        const QString& modeId) override;
    QWidget* createPanel(const QString& modeId, QWidget* parent) override;

    robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult prepareDeactivate(
        const robot_qt_viewer::RobotQtViewerWorkbenchTransitionContext& context) override;
    robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult deactivate(
        const robot_qt_viewer::RobotQtViewerWorkbenchTransitionContext& context) override;
    robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult activate(
        const robot_qt_viewer::RobotQtViewerWorkbenchActivationContext& context) override;
    void releaseProject(
        const robot_qt_viewer::RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept override;
    void shutdown(
        const robot_qt_viewer::RobotQtViewerWorkbenchShutdownContext& context) noexcept override;
    void retranslateUi(
        const robot_qt_viewer::RobotQtViewerLocalizationService& localization) noexcept override;
};
