# ThreeDOF Robot Obstacle Scene

Obstacle positions are in meters in the robot base frame. The robot maximum reach is approximately 0.530 m.

| Obstacle | Shape | Position XYZ (m) | Size | Purpose |
| --- | --- | --- | --- | --- |
| obs_01_elbow_cylinder | cylinder | (0.220, 0.110, 0.000) | radius 0.035 m, height 0.090 m | Near joint2, inside the elbow sweep region. |
| obs_02_front_box | box | (0.360, -0.090, 0.000) | 0.085 x 0.040 x 0.080 m | Rectangular block in the middle-to-outer reach area. |
| obs_03_outer_cylinder | cylinder | (0.480, 0.075, 0.000) | radius 0.028 m, height 0.085 m | Small post near the outer workspace. |
| obs_04_left_wall | box | (0.170, -0.185, 0.000) | 0.150 x 0.026 x 0.075 m | Thin wall across a lower-left sweep corridor. |
| obs_05_tool_goal_block | box | (0.420, 0.185, 0.000) | 0.070 x 0.060 x 0.070 m | Block near reachable tool positions. |

## Files

- ThreeDOF_Robot_ObstacleScene.SLDASM
- obs_01_elbow_cylinder.SLDPRT
- obs_02_front_box.SLDPRT
- obs_03_outer_cylinder.SLDPRT
- obs_04_left_wall.SLDPRT
- obs_05_tool_goal_block.SLDPRT
