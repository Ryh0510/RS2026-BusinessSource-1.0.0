#include "RobotQtViewerEnvironmentController.h"
#include "RobotQtViewerFileDialog.h"
#include "RobotQtViewerLocalization.h"
#include "RobotQtViewerOperationStatus.h"
#include <RobotViewport.h>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QSettings>
#include <QSignalBlocker>
#include <QWidget>

namespace robot_qt_viewer
{
RobotQtViewerEnvironmentController::RobotQtViewerEnvironmentController(RobotViewport& viewport, QMenu& menu,
    RobotQtViewerLocalizationService& localization, RobotQtViewerOperationStatusStore& operations, QWidget& window)
    : QObject(&window), m_viewport(viewport), m_localization(localization), m_operations(operations), m_window(window)
{
    m_group = new QActionGroup(this);
    m_group->setExclusive(true);
    for(const QString& id : {"studio", "factory", "workshop", "home", "none", "factory-detailed"}) {
        if(id == "factory-detailed") menu.addSeparator();
        QAction* action = menu.addAction(QString());
        action->setObjectName("environment." + id);
        action->setCheckable(true);
        m_group->addAction(action);
        m_actions[id] = action;
        connect(action, &QAction::triggered, this, [this, id]() { selectSource(id); });
    }
    m_custom = menu.addAction(QString());
    m_custom->setCheckable(true);
    m_custom->setEnabled(false);
    m_group->addAction(m_custom);
    m_load = menu.addAction(QString());
    m_load->setObjectName("environment.load");
    connect(m_load, &QAction::triggered, this, [this]() { chooseFile(); });
    menu.addSeparator();
    m_cutaway = menu.addAction(QString());
    m_cutaway->setObjectName("environment.cutaway");
    m_cutaway->setCheckable(true);
    connect(m_cutaway, &QAction::triggered, this, [this](bool enabled) {
        auto selection = m_viewport.environmentSelection();
        selection.cutaway = enabled;
        const auto operation = m_operations.begin("shell.environment", "environment.operation", "Load environment");
        std::string error;
        if(m_viewport.setEnvironmentSelection(selection, &error)) {
            remember();
            m_operations.succeed(operation, "environment.loaded", "Environment loaded");
        } else {
            m_operations.fail(operation, "environment.failed", "Environment load failed", "environment.cutaway",
                QString::fromUtf8(error.c_str()));
        }
        refresh();
    });
    connect(&viewport, &RobotViewport::environmentCommitted, this, [this]() { refresh(); });
    connect(&viewport, &RobotViewport::environmentRestoreFailed, this, [this](const QString& detail) {
        const auto operation = m_operations.begin("shell.environment", "environment.operation", "Load environment");
        m_operations.fail(operation, "environment.failed", "Environment load failed", "environment.restore", detail);
        refresh();
    });
    QSettings settings;
    const auto old = settings.value("view/environmentPreset", "factory").toString();
    ProjectSceneEnvironmentPreset preset;
    if(parseProjectSceneEnvironmentPreset(old.toStdString(), preset)) viewport.setEnvironmentPreset(preset);
    m_restoreSource = settings.value("view/environmentSource").toString();
    connect(&viewport, &RobotViewport::environmentSceneReady, this, [this]() {
        refresh();
        if(m_restoreSource.isEmpty()) return;
        const auto source = m_restoreSource;
        m_restoreSource.clear();
        selectSource(source, false);
        refresh();
    });
    retranslateUi();
    refresh();
}

void RobotQtViewerEnvironmentController::retranslateUi()
{
    for(auto action = m_actions.begin(); action != m_actions.end(); ++action)
        action.value()->setText(m_localization.text("action.environment." + action.key(), action.key()));
    m_load->setText(m_localization.text("environment.load", "Load environment model..."));
    m_custom->setText(m_localization.text("environment.custom", "Custom model environment"));
    m_cutaway->setText(m_localization.text("environment.cutaway", "Observation cutaway (off: complete environment)"));
}

void RobotQtViewerEnvironmentController::refresh()
{
    const auto selection = m_viewport.environmentSelection();
    m_actions.value("factory-detailed")->setEnabled(m_viewport.environmentSceneInitialized());
    m_load->setEnabled(m_viewport.environmentSceneInitialized());
    const QString id = selection.isModel() ? QString::fromStdString(selection.id)
        : QString::fromLatin1(projectSceneEnvironmentPresetId(selection.preset));
    auto* action = m_actions.value(id, m_custom);
    action->setChecked(true);
    m_custom->setVisible(id == "custom");
    const QSignalBlocker blocker(m_cutaway);
    m_cutaway->setChecked(selection.cutaway);
    m_cutaway->setEnabled(selection.isModel() && selection.parts.size() > 1);
}

void RobotQtViewerEnvironmentController::remember()
{
    const auto selection = m_viewport.environmentSelection();
    QSettings settings;
    settings.setValue("view/environmentSource", QString::fromStdString(selection.source));
    settings.setValue("view/environmentCutaway", selection.cutaway);
    if(!selection.isModel()) settings.setValue("view/environmentPreset", projectSceneEnvironmentPresetId(selection.preset));
}

bool RobotQtViewerEnvironmentController::selectSource(const QString& source, bool persist, QString* error)
{
    const auto operation = m_operations.begin("shell.environment", "environment.operation", "Load environment", source, 2);
    m_operations.reportProgress(operation, "environment.parse", "Read model assets", 1, 2);
    m_window.repaint();
    ProjectSceneEnvironmentSelection selection;
    QString detail;
    bool ok = readEnvironmentSelection(source, selection, detail);
    if(ok && selection.isModel() && !m_viewport.environmentSceneInitialized()) {
        ok = false;
        detail = "The viewport scene is not initialized. Load a project before selecting a model environment.";
    }
    if(ok) {
        selection.cutaway = selection.isModel() && selection.parts.size() > 1
            ? QSettings().value("view/environmentCutaway", true).toBool() : false;
        m_operations.reportProgress(operation, "environment.upload", "Build environment display", 2, 2);
        m_window.repaint();
        std::string coreError;
        ok = m_viewport.setEnvironmentSelection(selection, &coreError);
        if(!ok) detail = QString::fromUtf8(coreError.c_str());
    }
    if(ok) {
        if(persist) remember();
        m_operations.succeed(operation, "environment.loaded", "Environment loaded");
    } else m_operations.fail(operation, "environment.failed", "Environment load failed", "environment.load", detail);
    if(error) *error = detail;
    refresh();
    return ok;
}

void RobotQtViewerEnvironmentController::chooseFile()
{
    const QString source = getOpenFileName("shell.environment.open", &m_window,
        m_localization.text("fileDialog.shell.environment.open.title", "Load environment model"), QString(),
        m_localization.text("fileDialog.shell.environment.open.filter", "Environment models (*.gltf *.environment.json)"));
    if(!source.isEmpty()) selectSource(source);
}
}
