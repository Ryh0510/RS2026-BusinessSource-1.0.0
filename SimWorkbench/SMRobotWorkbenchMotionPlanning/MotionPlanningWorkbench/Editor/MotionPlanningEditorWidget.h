#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

class QDoubleSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class MotionPlanningEditorWidget : public QWidget
{
    Q_OBJECT

public:
    struct TrajectoryListItem
    {
        QString id;
        QString label;
        QString kind;
        int pointCount = 0;
    };

    struct TrajectoryPointRow
    {
        int index = 0;
        QString timeText;
        QString valueText;
        QString orientationText;
    };

    explicit MotionPlanningEditorWidget(QWidget* parent = nullptr);

    void setRobotId(const QString& robotId);
    void setJointDefaults(const QString& jointNames, const QString& startJoints);
    void setResult(const QString& summary, bool success);
    void setTrajectoryView(
        const QVector<TrajectoryListItem>& trajectories,
        const QString& selectedTrajectoryId,
        const QVector<TrajectoryPointRow>& posePoints,
        const QVector<TrajectoryPointRow>& jointPoints,
        const QString& emptyPoseText,
        const QString& emptyJointText);

signals:
    void planRequested(
        const QString& startJoints,
        const QString& goalJoints,
        const QString& jointNames,
        double duration,
        int sampleCount);
    void importTrajectoryRequested();
    void inverseKinematicsRequested(bool useToolTransform);
    void applySelectedJointPointRequested(int pointIndex);
    void trajectorySelectionChanged(const QString& trajectoryId);

private:
    void updateTrajectoryActions();

    QLabel* m_robotValue = nullptr;
    QLineEdit* m_startJoints = nullptr;
    QLineEdit* m_goalJoints = nullptr;
    QLineEdit* m_jointNames = nullptr;
    QDoubleSpinBox* m_duration = nullptr;
    QSpinBox* m_sampleCount = nullptr;
    QPushButton* m_planButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QComboBox* m_ikToolMode = nullptr;
    QPushButton* m_solveIkButton = nullptr;
    QPushButton* m_applyJointPointButton = nullptr;
    QComboBox* m_trajectoryCombo = nullptr;
    QTableWidget* m_poseTable = nullptr;
    QTableWidget* m_jointTable = nullptr;
    QLabel* m_result = nullptr;
};
