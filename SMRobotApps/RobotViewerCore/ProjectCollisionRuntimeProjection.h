#pragma once

#include "ProjectRuntimeTypes.h"

#include <SimulationRuntime/ProjectCollisionDetectorRuntime.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ProjectCollisionDetectorViewRuntime
    : simulation_runtime::ProjectCollisionDetectorRuntime
{
    bool visible = true;
    std::size_t effectiveIncludePairCount = 0;
    double lastCheckMs = 0.0;
    double lastDistanceMs = 0.0;
    double lastQueryMs = 0.0;
    std::uint64_t lastNearestQueryFrame = 0;
    std::string nearestState = "NotComputed";
    std::string nearestReason;
};

std::vector<ProjectCollisionDetectorViewRuntime> buildProjectCollisionDetectorViewRuntimes(
    const simulation_project::ProjectDocument& document,
    const std::vector<RuntimeRobot>& robots,
    const std::vector<RuntimeSceneObject>& objects);
