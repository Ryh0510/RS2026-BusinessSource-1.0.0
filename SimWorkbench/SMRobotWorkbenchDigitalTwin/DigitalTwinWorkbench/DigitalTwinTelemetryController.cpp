#include "DigitalTwinTelemetryController.h"
#include "DigitalTwinTelemetryPanel.h"
#include <RobotQtViewerViewportPorts.h>
#include <QTimer>
namespace robot_qt_viewer {
    DigitalTwinTelemetryController::DigitalTwinTelemetryController(
        DigitalTwinTelemetryPanel& panel, IRobotQtViewerTelemetryPort& port,
        IRobotQtViewerMotionPlanningViewportPort* viewport)
        : QObject(&panel), m_panel(panel), m_port(port), m_viewport(viewport)
    {
        connect(&panel, &DigitalTwinTelemetryPanel::startRequested, this, [this] {
            if(m_port.start(m_panel.endpointId(), m_panel.group(), m_panel.intervalMs(), m_panel.csvPath())) {
                m_sequence = 0;
                m_receivedUnixNs = 0;
                m_mappedRobotId.clear();
                if(m_viewport) m_viewport->clearTelemetryEndEffectorTrace();
            }
            m_panel.render(m_port.snapshot());
        });
        connect(&panel, &DigitalTwinTelemetryPanel::stopRequested, this, [this] { m_port.stop(); });
        connect(&panel, &DigitalTwinTelemetryPanel::refreshRequested, this, [this] {
            m_port.discover(); m_panel.render(m_port.snapshot());
        });
        connect(&panel, &DigitalTwinTelemetryPanel::sceneRobotFollowChanged, this, [this](bool enabled) {
            if(enabled) updateScene(true);
            else m_panel.setSceneFollowStatus(QStringLiteral("\u573a\u666f\u8ddf\u968f\u5df2\u5173\u95ed"));
        });
        connect(&panel, &DigitalTwinTelemetryPanel::endEffectorTraceVisibilityChanged, this, [this](bool visible) {
            if(m_viewport) m_viewport->setTelemetryEndEffectorTraceVisible(visible);
        });
        connect(&panel, &DigitalTwinTelemetryPanel::speedStatisticsRequested, this,
            [this](double startSeconds, double endSeconds, TelemetrySpeedCalculationMethod method, double minimumIntervalSeconds) {
                m_panel.renderSpeedStatistics(m_port.speedStatistics(startSeconds, endSeconds, method, minimumIntervalSeconds));
            });
        auto* timer = new QTimer(this);
        timer->setInterval(20);
        connect(timer, &QTimer::timeout, this, [this] { updateScene(); });
        timer->start();
        auto* displayTimer = new QTimer(this);
        displayTimer->setInterval(100);
        connect(displayTimer, &QTimer::timeout, this, [this] { m_panel.render(m_port.snapshot()); });
        displayTimer->start();
    }

    void DigitalTwinTelemetryController::updateScene(bool force)
    {
        if(!m_panel.sceneRobotFollowEnabled()) return;
        const auto state = m_port.snapshot();
        if(!state.streaming || !state.sequence || state.joints.empty()) {
            m_panel.setSceneFollowStatus(QStringLiteral("\u7b49\u5f85\u6709\u6548\u901a\u4fe1\u5173\u8282\u6570\u636e"));
            return;
        }
        if(!m_viewport) {
            m_panel.setSceneFollowStatus(QStringLiteral("\u573a\u666f\u89c6\u53e3\u4e0d\u53ef\u7528"));
            return;
        }
        if(!force && state.sequence == m_sequence && state.receivedUnixNs == m_receivedUnixNs) return;
        m_sequence = state.sequence;
        m_receivedUnixNs = state.receivedUnixNs;
        QString matchedRobot;
        if(!m_viewport->setRobotJointValuesByDof(state.joints, &matchedRobot)) {
            m_panel.setSceneFollowStatus(QStringLiteral("\u672a\u627e\u5230\u552f\u4e00\u5339\u914d\u7684 %1 \u81ea\u7531\u5ea6\u673a\u5668\u4eba\uff0c\u6216\u5173\u8282\u6570\u636e\u65e0\u6548").arg(state.joints.size()));
            return;
        }
        m_mappedRobotId = matchedRobot;
        m_viewport->setTelemetryEndEffectorTraceVisible(m_panel.endEffectorTraceVisible());
        m_viewport->appendTelemetryEndEffectorTraceSample(m_mappedRobotId);
        m_panel.setSceneFollowStatus(QStringLiteral("\u6b63\u5728\u8ddf\u968f\uff1a%1\uff08%2 \u81ea\u7531\u5ea6\uff09").arg(m_mappedRobotId).arg(state.joints.size()));
    }
}
