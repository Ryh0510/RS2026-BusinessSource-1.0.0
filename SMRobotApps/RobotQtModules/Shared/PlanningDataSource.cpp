#include "PlanningDataSource.h"

#include <SimulationProject/ProjectDocument.h>
#include <algorithm>
#include <cmath>

namespace robot_qt_viewer
{
    namespace
    {
        bool sameData(const PlanningDataSnapshot& a, const PlanningDataSnapshot& b)
        {
            if(a.modelObjectId != b.modelObjectId || a.modelPath != b.modelPath ||
                a.modelName != b.modelName || a.sceneScale != b.sceneScale ||
                a.sprayAxis != b.sprayAxis || a.feedAxis != b.feedAxis ||
                a.worldCoordinates != b.worldCoordinates || a.producer != b.producer ||
                a.trajectory.name != b.trajectory.name ||
                a.trajectory.segments.size() != b.trajectory.segments.size()) return false;
            for(std::size_t i = 0; i < a.trajectory.segments.size(); ++i) {
                const auto& x = a.trajectory.segments[i];
                const auto& y = b.trajectory.segments[i];
                if(x.passIndex != y.passIndex || x.processId != y.processId ||
                    x.sprayEnabled != y.sprayEnabled || x.points.size() != y.points.size()) return false;
                for(std::size_t j = 0; j < x.points.size(); ++j) {
                    const auto& p = x.points[j]; const auto& q = y.points[j];
                    if(p.time != q.time || p.sprayEnabled != q.sprayEnabled ||
                        p.processId != q.processId || p.pointType != q.pointType ||
                        p.velocity != q.velocity || p.acceleration != q.acceleration ||
                        p.targetDistance != q.targetDistance || p.workpieceRegionId != q.workpieceRegionId ||
                        p.jointValues != q.jointValues ||
                        (p.tcpPose.matrix().array() != q.tcpPose.matrix().array()).any() ||
                        (p.targetNormal.array() != q.targetNormal.array()).any()) return false;
                }
            }
            return true;
        }
    }

    bool planningDataToWorld(const PlanningDataSnapshot& input,
        const simulation_project::ProjectDocument& document,
        PlanningDataSnapshot& output, QString* error)
    {
        const auto object = std::find_if(document.objects.begin(), document.objects.end(),
            [&](const auto& candidate) { return candidate.id == input.modelObjectId.toStdString(); });
        if(object == document.objects.end()) {
            if(error) *error = QStringLiteral("The source workpiece is no longer in this project.");
            return false;
        }
        output = input;
        if(input.worldCoordinates) return true;
        const auto& t = object->transform;
        Eigen::Isometry3d world = Eigen::Isometry3d::Identity();
        world.translation() = Eigen::Vector3d(t.x, t.y, t.z);
        world.linear() = (Eigen::AngleAxisd(t.yaw, Eigen::Vector3d::UnitZ()) *
            Eigen::AngleAxisd(t.pitch, Eigen::Vector3d::UnitY()) *
            Eigen::AngleAxisd(t.roll, Eigen::Vector3d::UnitX())).toRotationMatrix();
        if(!world.matrix().allFinite()) {
            if(error) *error = QStringLiteral("The source workpiece transform is invalid.");
            return false;
        }
        for(auto& segment : output.trajectory.segments) {
            for(auto& point : segment.points) {
                point.tcpPose = world * point.tcpPose;
                point.targetNormal = world.linear() * point.targetNormal;
            }
        }
        for(auto& point : output.trajectory.baseCartesianTrajectory.points)
            point.tcpPose = world * point.tcpPose;
        output.worldCoordinates = true;
        return true;
    }
    PlanningDataSource::PlanningDataSource(QObject* parent)
        : QObject(parent)
    {
    }

    const PlanningDataSnapshot& PlanningDataSource::snapshot() const
    {
        return m_snapshot;
    }

    void PlanningDataSource::publish(PlanningDataSnapshot snapshot)
    {
        // UI-only refreshes must not erase edited/optimized downstream data.
        if(sameData(snapshot, m_snapshot)) return;
        snapshot.revision = m_snapshot.revision + 1;
        m_snapshot = std::move(snapshot);
        m_analysisSnapshot = m_snapshot;
        emit snapshotChanged(m_snapshot);
        emit analysisSnapshotChanged(m_analysisSnapshot);
    }

    const PlanningDataSnapshot& PlanningDataSource::analysisSnapshot() const
    {
        return m_analysisSnapshot;
    }

    bool PlanningDataSource::publishForAnalysis(PlanningDataSnapshot snapshot)
    {
        if(!snapshot.hasTrajectory() || !snapshot.worldCoordinates) return false;
        if(m_snapshot.hasModel() && (snapshot.modelObjectId != m_snapshot.modelObjectId ||
            snapshot.revision != m_snapshot.revision)) return false;
        m_analysisSnapshot = std::move(snapshot);
        emit analysisSnapshotChanged(m_analysisSnapshot);
        return true;
    }

    void PlanningDataSource::clear()
    {
        const auto revision = m_snapshot.revision + 1;
        m_snapshot = {};
        m_snapshot.revision = revision;
        m_analysisSnapshot = m_snapshot;
        emit snapshotChanged(m_snapshot);
        emit analysisSnapshotChanged(m_analysisSnapshot);
    }

    void PlanningDataSource::rebaseUserFrame(const Eigen::Isometry3d& delta)
    {
        if(!m_snapshot.hasTrajectory()) return;
        for(auto& segment:m_snapshot.trajectory.segments) for(auto& point:segment.points) {
            if(m_snapshot.worldCoordinates) {
                point.tcpPose=delta*point.tcpPose;
                point.targetNormal=delta.linear()*point.targetNormal;
            }
            point.jointValues.clear();
        }
        if(m_snapshot.worldCoordinates) for(auto& point:m_snapshot.trajectory.baseCartesianTrajectory.points)
            point.tcpPose=delta*point.tcpPose;
        ++m_snapshot.revision;
        // Optimized world paths and their joint solutions must be recomputed.
        m_analysisSnapshot=m_snapshot;
        emit snapshotChanged(m_snapshot);
        emit analysisSnapshotChanged(m_analysisSnapshot);
    }
}
