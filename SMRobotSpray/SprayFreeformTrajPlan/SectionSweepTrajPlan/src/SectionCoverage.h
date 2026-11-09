#pragma once
#include <SectionSweepTrajPlan/SingleSideScanPlanner.h>

namespace section_sweep::detail
{
    struct ScanPass
    {
        Eigen::Vector3d target;
        Eigen::Vector3d normal; // Outward nozzle direction, not necessarily the face normal.
    };

    // Retains the original pass budget; fits aim points/angles, never film thickness.
    bool fitExteriorCoverage(const std::vector<SectionSegment>& section,
        const Eigen::Vector3d& scan, const Eigen::Vector3d& side,
        const Parameters& parameters, std::vector<ScanPass>& passes, Result& result);
}
