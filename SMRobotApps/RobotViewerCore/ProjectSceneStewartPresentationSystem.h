#pragma once

#include "ProjectRuntimeTypes.h"

#include <vector>

namespace robot {
struct RobotModel;
}

namespace simulation_project {
struct RobotDesc;
}

class ProjectSceneStewartPresentationSystem
{
public:
    static void configureParallelControlIfNeeded(
        RuntimeRobot& runtime,
        const simulation_project::RobotDesc& robotDesc,
        const robot::RobotModel& model);
    static void configureParallelFollowerIfNeeded(
        RuntimeRobot& runtime,
        const simulation_project::RobotDesc& robotDesc,
        const robot::RobotModel& model);
    static void solveInternalLegControls(RuntimeRobot& platform);
    static bool sameSource(const RuntimeRobot& a, const RuntimeRobot& b);

    void applyInternalVisualOverrides(RuntimeRobot& platform);
    void applyFollowerVisualOverrides(
        const RuntimeRobot& platform,
        std::vector<RuntimeRobot>& robots);
    void applyAllVisualOverrides(std::vector<RuntimeRobot>& robots);
};
