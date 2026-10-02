#include "ExampleInspectionWorkbenchPlugin.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
    const QString kModeId = QStringLiteral("smrobot.mode.example-inspection");
}

QString ExampleInspectionWorkbenchPlugin::extensionApiVersion() const
{
    return QStringLiteral("1");
}

robot_qt_viewer::RobotQtViewerWorkbenchPackageDesc
ExampleInspectionWorkbenchPlugin::packageDescriptor() const
{
    robot_qt_viewer::RobotQtViewerWorkbenchPackageDesc package =
        robot_qt_viewer::makeRobotQtViewerWorkbenchPackage(
            QStringLiteral("smrobot.example.inspection"),
            QStringLiteral("Example Inspection Workbench"),
            QStringLiteral("1.0.0"));
    package.providedCapabilities =
        QStringList{ QStringLiteral("smrobot.capability.example-inspection") };
    package.dynamicallyLoadable = true;
    return package;
}

QVector<robot_qt_viewer::RobotQtViewerWorkbenchPluginModeDesc>
ExampleInspectionWorkbenchPlugin::modeDescriptors() const
{
    robot_qt_viewer::RobotQtViewerWorkbenchPluginModeDesc mode;
    mode.id = kModeId;
    mode.displayName = QStringLiteral("Example Inspection");
    mode.rightPanelTitle = QStringLiteral("Inspection Task");
    mode.toolbarActionId = QStringLiteral("exampleInspectionWorkbench");
    mode.defaultOrder = 900;
    return { mode };
}

robot_qt_viewer::IRobotQtViewerWorkbenchLifecycle*
ExampleInspectionWorkbenchPlugin::lifecycle(const QString& modeId)
{
    return modeId == kModeId ? this : nullptr;
}

robot_qt_viewer::IRobotQtViewerLanguageParticipant*
ExampleInspectionWorkbenchPlugin::languageParticipant(const QString& modeId)
{
    return modeId == kModeId ? this : nullptr;
}

QWidget* ExampleInspectionWorkbenchPlugin::createPanel(
    const QString& modeId,
    QWidget* parent)
{
    if(modeId != kModeId) {
        return nullptr;
    }
    auto* panel = new QWidget(parent);
    panel->setObjectName(QStringLiteral("exampleInspectionWorkbenchPanel"));
    auto* layout = new QVBoxLayout(panel);
    auto* title = new QLabel(QStringLiteral("Example Inspection"), panel);
    title->setObjectName(QStringLiteral("exampleInspectionWorkbenchTitle"));
    layout->addWidget(title);
    layout->addStretch(1);
    return panel;
}

robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult
ExampleInspectionWorkbenchPlugin::prepareDeactivate(
    const robot_qt_viewer::RobotQtViewerWorkbenchTransitionContext&)
{
    return robot_qt_viewer::workbenchTransitionSucceeded();
}

robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult
ExampleInspectionWorkbenchPlugin::deactivate(
    const robot_qt_viewer::RobotQtViewerWorkbenchTransitionContext&)
{
    return robot_qt_viewer::workbenchTransitionSucceeded();
}

robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult
ExampleInspectionWorkbenchPlugin::activate(
    const robot_qt_viewer::RobotQtViewerWorkbenchActivationContext&)
{
    return robot_qt_viewer::workbenchTransitionSucceeded();
}

void ExampleInspectionWorkbenchPlugin::releaseProject(
    const robot_qt_viewer::RobotQtViewerWorkbenchProjectReleaseContext&) noexcept
{
}

void ExampleInspectionWorkbenchPlugin::shutdown(
    const robot_qt_viewer::RobotQtViewerWorkbenchShutdownContext&) noexcept
{
}

void ExampleInspectionWorkbenchPlugin::retranslateUi(
    const robot_qt_viewer::RobotQtViewerLocalizationService&) noexcept
{
}
