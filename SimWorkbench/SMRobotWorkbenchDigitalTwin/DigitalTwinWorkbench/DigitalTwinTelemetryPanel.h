#pragma once
#include "RobotQtViewerTelemetryPort.h"
#include <QWidget>
#include <QVector>
class QComboBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QTabWidget;
class QLabel;
class QPushButton;
class QTableWidget;
class QDialog;
namespace robot_qt_viewer {
    class DigitalTwinTelemetryPanel final : public QWidget {
        Q_OBJECT
    public:
        explicit DigitalTwinTelemetryPanel(QWidget* parent);
        void render(const TelemetrySnapshot& state);
        QString endpointId() const;
        QString csvPath() const;
        int group() const;
        int intervalMs() const;
        double speedStartSeconds() const;
        double speedEndSeconds() const;
        TelemetrySpeedCalculationMethod speedCalculationMethod() const;
        double speedMinimumIntervalSeconds() const;
        bool sceneRobotFollowEnabled() const;
        bool endEffectorTraceVisible() const;
        void setSceneFollowStatus(const QString& message);
        void renderSpeedStatistics(const TelemetrySpeedStatistics& statistics);
        void openSpeedStatisticsWindow();
    signals:
        void startRequested();
        void stopRequested();
        void refreshRequested();
        void speedStatisticsRequested(double startSeconds, double endSeconds,
            TelemetrySpeedCalculationMethod method, double minimumIntervalSeconds);
        void sceneRobotFollowChanged(bool enabled);
        void endEffectorTraceVisibilityChanged(bool visible);
    private:
        QComboBox* m_endpoints;
        QLineEdit* m_csv;
        QSpinBox* m_group;
        QSpinBox* m_interval;
        QCheckBox* m_sceneRobotFollow = nullptr;
        QCheckBox* m_endEffectorTraceVisible = nullptr;
        QLabel* m_sceneFollowStatus = nullptr;
        QLabel* m_status;
        QLabel* m_time;
        QLabel* m_source;
        QTableWidget* m_joints;
        QTableWidget* m_pose;
        QLabel* m_poseInfo;
        QDoubleSpinBox* m_speedStart;
        QDoubleSpinBox* m_speedEnd;
        QComboBox* m_speedMethod = nullptr;
        QDoubleSpinBox* m_speedMinimumInterval = nullptr;
        QTabWidget* m_speedTabs;
        QVector<QWidget*> m_speedPlots;
        QLabel* m_speedInfo;
        QDialog* m_speedDialog = nullptr;
        TelemetrySpeedStatistics m_lastSpeedStatistics;
        QPushButton* m_start;
        QPushButton* m_stop;
        QPushButton* m_refresh;
        QPushButton* m_browse;
    };
}
