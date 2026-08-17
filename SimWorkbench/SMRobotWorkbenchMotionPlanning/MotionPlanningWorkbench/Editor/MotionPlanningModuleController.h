#pragma once

#include "RobotQtViewerEvents.h"

#include <QObject>
#include <QString>

#include <string>
#include <vector>

class MotionPlanningEditorWidget;
class QTimer;

namespace motion_planning
{
    struct StoredMotionPlan;
}

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentContext;

    class MotionPlanningModuleController : public QObject
    {
        Q_OBJECT

    public:
        MotionPlanningModuleController(
            MotionPlanningEditorWidget& widget,
            RobotQtViewerDocumentContext& context,
            QObject* parent = nullptr);

        void handleEvent(const RobotQtViewerEvent& event);

    signals:
        void statusMessageRequested(const QString& message, int timeoutMs);
        void trajectoryPlanned(const QString& trajectoryId);

    private:
        void planTrajectory(
            const QString& startJoints,
            const QString& goalJoints,
            const QString& jointNames,
            double duration,
            int sampleCount);
        void importTrajectory();
        void solveInverseKinematics(bool useToolTransform);
        void applySelectedJointPoint(int pointIndex);
        void insertControlPointBefore(int pointIndex);
        void insertControlPointAfter(int pointIndex);
        void deleteControlPoint(int pointIndex);
        void editControlPoint(int pointIndex);
        void startJointPlayback(double durationSeconds);
        void stopJointPlayback();
        void advanceJointPlayback();
        void setSelectedTrajectory(const QString& trajectoryId);
        void setSelectedRobot(const QString& robotId);
        void refreshTrajectoryView();
        bool commitMotionPlanUpdate(
            const motion_planning::StoredMotionPlan& plan,
            const QString& sourceId);
        bool applyJointValuesToRobotRuntime(
            const std::vector<std::string>& jointNames,
            const std::vector<double>& jointValues,
            const QString& sourceId);
        bool applyJointValuesToRobot(
            const std::vector<std::string>& jointNames,
            const std::vector<double>& jointValues,
            const QString& sourceId);

        MotionPlanningEditorWidget& m_widget;
        RobotQtViewerDocumentContext& m_context;
        QString m_selectedRobotId;
        QString m_selectedTrajectoryId;
        QTimer* m_playbackTimer = nullptr;
        int m_playbackPointIndex = 0;
    };
}
