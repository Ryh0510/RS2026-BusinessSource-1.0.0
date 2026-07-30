# CDF planning demos

This folder is intentionally split like the project document/view architecture:

- `Algorithms` owns the CDF data model, distance oracle interfaces, finite-difference gradients, initial-path handling, repair loop, and collision-free validation.
- `Gui` owns Qt widgets, controllers, and view-model builders. Widgets receive a view model and emit user intent only; all data changes go through the algorithm document/controller boundary.

The included GUI uses a small 2D configuration-space scene so the algorithms can be built and inspected without loading a robot project. In the full RS2026 platform, replace the demo `CircularObstacleCdfOracle` with an oracle backed by `motion_planning::ProjectPlanningSceneSnapshot` / collision distance queries, and provide the OMPL trajectory as `PlanningRequest::omplSeedPath`.

Suggested platform integration:

1. Use the existing project motion-planning service to generate the first OMPL trajectory in joint space.
2. Convert that trajectory into `cdf::Path` and assign it to `PlanningRequest::omplSeedPath`.
3. Implement `cdf::ICdfDistanceOracle` by setting the robot state on the project planning snapshot, updating collision runtime state, and returning the signed minimum distance from the collision result.
4. Call `cdf::CdfPlanningService::plan(request, platformOracle)` to run finite-difference CDF gradient repair and path validation.
5. Keep Qt widgets bound to a view model; mutate data only through `cdf::CdfDocument` or a platform document service.
