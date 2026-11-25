#pragma once
#include <QObject>
#include <QString>
namespace robot_qt_viewer {
    class DigitalTwinTelemetryPanel;
    class IRobotQtViewerTelemetryPort;
    class IRobotQtViewerMotionPlanningViewportPort;
    class DigitalTwinTelemetryController final : public QObject {
    public:
        DigitalTwinTelemetryController(DigitalTwinTelemetryPanel& panel, IRobotQtViewerTelemetryPort& port,
            IRobotQtViewerMotionPlanningViewportPort* viewport = nullptr);
    private:
        void updateScene(bool force = false);
        DigitalTwinTelemetryPanel& m_panel;
        IRobotQtViewerTelemetryPort& m_port;
        IRobotQtViewerMotionPlanningViewportPort* m_viewport;
        quint64 m_sequence = 0;
        qint64 m_receivedUnixNs = 0;
        QString m_mappedRobotId;
    };
}
