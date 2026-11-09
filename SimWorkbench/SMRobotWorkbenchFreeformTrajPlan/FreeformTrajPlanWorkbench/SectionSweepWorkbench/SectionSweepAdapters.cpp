#include "SectionSweepAdapters.h"
#include "SlicingWorkbench/TrajectoryExportSampling.h"

#include <cmath>
#include <stdexcept>

namespace smrobot::workbench::freeform
{
    std::vector<section_sweep::Triangle> sectionMeshInMeters(
        const ModelTrajPlan::PrivateModel& model,double scale)
    {
        if(!std::isfinite(scale)||scale<=0. || model.m_indices.size()%3)
            throw std::runtime_error("Invalid model unit scale or triangle indices.");
        std::vector<section_sweep::Triangle> result;
        for(std::size_t i=0;i<model.m_indices.size();i+=3) {
            section_sweep::Triangle triangle;
            for(int k=0;k<3;++k) {
                const std::size_t j=static_cast<std::size_t>(model.m_indices[i+k])*3;
                if(j+2>=model.m_vertices.size()) throw std::runtime_error("Invalid mesh vertex index.");
                triangle[k]=scale*Eigen::Vector3d(model.m_vertices[j],model.m_vertices[j+1],model.m_vertices[j+2]);
            }
            result.push_back(triangle);
        }
        return result;
    }

    std::vector<ModelTrajPlan::TrajectoryPoint> sectionExportPoints(const section_sweep::Result& result,double interpolationInterval)
    {
        std::vector<ModelTrajPlan::TrajectoryPoint> points;
        if(!result.success) return points;
        for(std::size_t i=0;i<result.points.size();++i) {
            const auto& source=result.points[i];
            ModelTrajPlan::TrajectoryPoint point;
            point.pose=source.pose.matrix();point.pose.block<3,1>(0,3)*=1000.;
            point.timestamp=source.time;point.velocity=source.speed*1000.;
            point.spray_distance=source.spray?result.sprayDistance*1000.:0.;
            point.trajectory_index=source.pass;point.point_type=source.spray?0:2;
            if(source.spray && (i+1==result.points.size() || !result.points[i+1].spray ||
                i==0 || !result.points[i-1].spray)) point.point_type=1;
            points.push_back(point);
        }
        if(interpolationInterval == 0.) return points;
        if(!std::isfinite(interpolationInterval) || interpolationInterval < 0. || points.size()<2 ||
            (points.back().timestamp-points.front().timestamp)/interpolationInterval + points.size() > 10000000.)
            throw std::runtime_error("Invalid interpolation interval or excessive output size.");
        std::vector<ModelTrajPlan::TrajectoryPoint> sampled;
        sampled.push_back(points.front());
        for(std::size_t i=1;i<points.size();++i) {
            auto start=points[i-1];
            // Process values describe the incoming move, not a ramp from the previous move.
            start.point_type=points[i].point_type;start.velocity=points[i].velocity;
            start.spray_distance=points[i].spray_distance;
            auto edge=resampleExportTrajectory({start,points[i]},interpolationInterval);
            sampled.insert(sampled.end(),edge.begin()+1,edge.end());
        }
        return sampled;
    }

    void publishSectionTrajectory(const section_sweep::Result& result,robot_qt_viewer::PlanningDataSnapshot& snapshot)
    {
        // Match the coverage/webpage Gaussian frame: +Z sprays, +Y is
        // the transverse major axis, +X stays fixed along the scan axis.
        snapshot.sprayAxis=4;snapshot.feedAxis=2;
        if(!result.success) return;
        for(std::size_t i=0;i<result.points.size();++i) {
            const auto& source=result.points[i];
            const bool newSegment=i==0 || source.spray!=result.points[i-1].spray || source.pass!=result.points[i-1].pass;
            if(newSegment) {
                spraytrajectory::SpraySegment segment;
                segment.passIndex=source.pass;segment.sprayEnabled=source.spray;segment.processId="freeform";
                // Retain the exact incoming segment endpoint and its timestamp.
                if(i>0) {
                    spraytrajectory::SprayPathPoint start;
                    start.tcpPose=result.points[i-1].pose;start.time=result.points[i-1].time;
                    start.velocity=source.speed;start.sprayEnabled=source.spray;
                    start.targetDistance=source.spray?result.sprayDistance:0.;
                    start.targetNormal=-start.tcpPose.linear().col(2);
                    start.workpieceRegionId=source.pass;
                    start.pointType=source.spray?spraytrajectory::SprayPathPointType::Regular:spraytrajectory::SprayPathPointType::AirMove;
                    segment.points.push_back(std::move(start));
                }
                snapshot.trajectory.segments.push_back(std::move(segment));
            }
            spraytrajectory::SprayPathPoint point;
            point.tcpPose=source.pose;point.time=source.time;point.velocity=source.speed;
            point.sprayEnabled=source.spray;point.targetDistance=source.spray?result.sprayDistance:0.;
            point.targetNormal=-point.tcpPose.linear().col(2);point.workpieceRegionId=source.pass;
            point.pointType=source.spray?spraytrajectory::SprayPathPointType::Regular:spraytrajectory::SprayPathPointType::AirMove;
            snapshot.trajectory.segments.back().points.push_back(std::move(point));
        }
    }

    FreeformTrajPlanLinePreview sectionTrajectoryLines(const section_sweep::Result& result,bool spraying)
    {
        FreeformTrajPlanLinePreview lines;
        for(std::size_t i=1;i<result.points.size();++i) {
            if(result.points[i].spray!=spraying) continue;
            const auto& a=result.points[i-1].pose.translation();const auto& b=result.points[i].pose.translation();
            lines.push_back({a.x(),a.y(),a.z(),b.x(),b.y(),b.z(),spraying?.1:1.,spraying?.9:.65,spraying?.8:.1,1.});
        }
        return lines;
    }
}
