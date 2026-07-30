#include <MotionPlanningCore/MotionPlanning.h>
#include <RobotRuntime/RobotTrajectoryExecutionSession.h>
#include <SimulationProject/ProjectDocument.h>

#include <cmath>
#include <iostream>

int main()
{
    motion_planning::MotionPlanningRequest request;
    request.robotId = "consumer_robot";
    request.startJoints = { 0.0, 0.5 };
    request.goalJoints = { 1.0, -0.5 };
    request.constraint.duration = 2.0;
    request.constraint.sampleCount = 5;

    const motion_planning::LinearJointMotionPlanner planner;
    const motion_planning::MotionPlanningResult result = planner.plan(request);
    if(!result.succeeded() || result.trajectory.points.size() != 5) {
        return 1;
    }
    if(std::abs(result.trajectory.points.back().q.front() - 1.0) > 1e-9) {
        return 2;
    }

    motion_planning::MotionPlanningRequest invalidRequest = request;
    invalidRequest.goalJoints.pop_back();
    if(planner.plan(invalidRequest).succeeded()) {
        return 3;
    }

    robotruntime::RobotTrajectoryExecutionSession execution;
    if(!execution.load(request.robotId, "consumer_trajectory", result.trajectory).success) {
        return 4;
    }
    if(!execution.start().success || !execution.step(0.5).success) {
        return 5;
    }

    const robotruntime::RobotRunExecutionSnapshot running = execution.snapshot();
    if(running.state != robotruntime::RobotRunExecutionState::Running ||
        running.time <= 0.0 || running.jointValues.size() != request.startJoints.size()) {
        return 6;
    }
    if(!execution.stop().success || execution.snapshot().time != 0.0) {
        return 7;
    }

    simulation_project::ProjectDocument document;
    motion_planning::StoredMotionPlan storedPlan;
    storedPlan.id = "consumer_trajectory";
    storedPlan.name = "Consumer trajectory";
    storedPlan.robotId = request.robotId;
    storedPlan.jointNames = { "joint_1", "joint_2" };
    storedPlan.trajectory = result.trajectory;
    std::string storeError;
    if(!motion_planning::MotionPlanningProjectStore::upsertPlan(
           document, storedPlan, &storeError)) {
        return 8;
    }
    const std::vector<motion_planning::StoredMotionPlan> storedPlans =
        motion_planning::MotionPlanningProjectStore::plans(document);
    if(storedPlans.size() != 1 ||
        storedPlans.front().id != storedPlan.id ||
        storedPlans.front().jointNames != storedPlan.jointNames ||
        storedPlans.front().trajectory.points.size() != result.trajectory.points.size()) {
        return 9;
    }

    std::cout << "Motion planning, project store, and trajectory execution consumer passed." << std::endl;
    return 0;
}
