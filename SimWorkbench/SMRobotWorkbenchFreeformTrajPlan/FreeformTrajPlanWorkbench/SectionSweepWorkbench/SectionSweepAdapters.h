#pragma once

#include "SlicingWorkbench/FreeformTrajPlanWorkbenchExport.h"
#include "FreeformTrajPlanRenderData.h"
#include <SectionSweepTrajPlan/SingleSideScanPlanner.h>
#include <SlicingTrajPlan/PModelData.h>
#include <SlicingTrajPlan/PTrajData.h>
#include "PlanningDataSource.h"

namespace smrobot::workbench::freeform
{
    FREEFORM_TRAJ_PLAN_WORKBENCH_API std::vector<section_sweep::Triangle> sectionMeshInMeters(
        const ModelTrajPlan::PrivateModel& model,double nativeToMeters);
    FREEFORM_TRAJ_PLAN_WORKBENCH_API std::vector<ModelTrajPlan::TrajectoryPoint> sectionExportPoints(
        const section_sweep::Result& result,double interpolationInterval = 0.);
    FREEFORM_TRAJ_PLAN_WORKBENCH_API void publishSectionTrajectory(
        const section_sweep::Result& result,robot_qt_viewer::PlanningDataSnapshot& snapshot);
    FREEFORM_TRAJ_PLAN_WORKBENCH_API FreeformTrajPlanLinePreview sectionTrajectoryLines(
        const section_sweep::Result& result,bool spraying);
}
