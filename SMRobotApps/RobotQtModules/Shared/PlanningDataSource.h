#pragma once



#include <SprayTrajectoryCore/SprayTrajectory.h>

#include <QObject>
#include <QString>

namespace simulation_project { struct ProjectDocument; }

namespace robot_qt_viewer
{
    struct PlanningDataSnapshot
    {
        QString modelObjectId;
        QString modelPath;
        QString modelName;
        double sceneScale{ 1.0 };
        int sprayAxis{4};
        int feedAxis{2};
        spraytrajectory::SprayTrajectory trajectory;
        quint64 revision{ 0 };
        // Freeform output is model-local (already scaled to meters). Motion
        // planning and optimized output use world TCP poses in meters.
        bool worldCoordinates{ false };
        QString producer{ QStringLiteral("Freeform") };

        bool hasModel() const { return !modelObjectId.isEmpty() && !modelPath.isEmpty(); }
        bool hasTrajectory() const { return !trajectory.empty(); }
    };

    bool planningDataToWorld(const PlanningDataSnapshot& input,
        const simulation_project::ProjectDocument& document,
        PlanningDataSnapshot& output, QString* error = nullptr);

    // Keep the original and analysis channels separate: optimized results must
    // never re-import themselves into Motion Planning or calibration.
    class PlanningDataSource final : public QObject
    {
        Q_OBJECT
    public:
        explicit PlanningDataSource(QObject* parent = nullptr);

        const PlanningDataSnapshot& snapshot() const;
        const PlanningDataSnapshot& analysisSnapshot() const;
        void publish(PlanningDataSnapshot snapshot);
        bool publishForAnalysis(PlanningDataSnapshot snapshot);
        void clear();
        void rebaseUserFrame(const Eigen::Isometry3d& delta);

    signals:
        void snapshotChanged(const robot_qt_viewer::PlanningDataSnapshot& snapshot);
        void analysisSnapshotChanged(const robot_qt_viewer::PlanningDataSnapshot& snapshot);

    private:
        PlanningDataSnapshot m_snapshot;
        PlanningDataSnapshot m_analysisSnapshot;
    };
}
