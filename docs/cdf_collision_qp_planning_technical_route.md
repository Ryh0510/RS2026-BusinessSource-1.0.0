# 基于当前碰撞检测器的 CDF/QP 无碰撞轨迹规划技术路线

本文档总结前期关于 `CDF` 论文、`cdf-main` 官方代码、当前 `RS2026-BusinessSource-1.0.0` CMake 项目，以及 `ABB4600_urdf + burnner + 11111.txt` 轨迹输入的讨论结果。目标是形成一份后续可以直接继续实施的技术路线。

## 1. 结论先行

当前项目可以实现基于 CDF 思想的无碰撞 QP 轨迹规划，但必须区分两件事：

1. 论文严格意义的 CDF：
   - 函数形式是 `d = f(p, q)`。
   - `p` 是空间点，`q` 是关节构型。
   - 输出 `d` 是让机器人接触空间点 `p` 所需的最小关节空间运动量，单位更接近 `rad` 或关节空间范数。
   - 对障碍物模型，需要先将障碍物表面采样成点集 `{p_i}`，再计算 `min_i CDF(p_i, q)`。

2. 当前项目更容易落地的工程方案：
   - 使用项目已有碰撞检测器，把 `robot(q)` 和 `burnner mesh` 的最近距离或穿透深度封装成一个关于 `q` 的 signed clearance field。
   - 记为 `phi(q)`，通过有限差分估计 `grad_phi(q)`。
   - 用 `phi(q)` 和 `grad_phi(q)` 构造 QP 避障约束。

因此，建议对外表述为：

```text
本文采用 CDF-inspired collision-distance field / collision signed-clearance surrogate。
它不是论文严格 CDF 的完整复现，而是基于当前项目碰撞检测器构造的可微近似安全场，
用于 QP 轨迹局部修复。
```

如果后续论文或报告需要严谨，可以将当前方案称为：

```text
Project collision signed-distance surrogate for CDF-style QP planning
```

## 2. 当前项目中的已有关键信息

### 2.1 模型与输入

本任务使用：

```text
机器人:
data/Spray420/ABB4600_urdf/urdf/ABB4600_urdf.urdf

障碍物:
data/drake_models/burnner
```

`burnner` 的 URDF 只有一个 `base_link`，碰撞几何是：

```text
data/drake_models/burnner/meshes/base_link.STL
```

所以在当前项目里应将 `burnner` 作为静态 `SceneObjectDesc` 障碍物加载，而不是作为第二个运动机器人加载。

初始轨迹：

```text
C:/Users/14390/Desktop/11111.txt
```

该文件是 `4x4 TCP 位姿矩阵 + 时间` 的笛卡尔轨迹，不是关节轨迹。需要先导入为 Cartesian control points，再用 ABB4600 IK 转成关节轨迹。

### 2.2 现有代码接口

轨迹导入：

```text
SMRobotMotionPlanning/ProjectMotionPlanning/include/ProjectMotionPlanning/TrajectoryImport.h
SMRobotMotionPlanning/ProjectMotionPlanning/src/TrajectoryImport.cpp
```

要点：

- `TrajectoryImportOptions::cartesianPositionScale = 0.001`，默认可将毫米转米。
- `parseGenericMatrixText` 可以读取纯矩阵文本。
- `11111.txt` 是矩阵后跟时间行，当前 parser 对时间绑定可能需要再确认。如果只做几何避障，时间错位影响较小；如果要保持原始节拍，应新增专用 parser。

ABB4600 IK：

```text
SMRobotMotionPlanning/ProjectMotionPlanning/include/ProjectMotionPlanning/TrajectoryInverseKinematics.h
SMRobotMotionPlanning/ProjectMotionPlanning/src/TrajectoryInverseKinematics.cpp
```

要点：

- `ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames()` 返回：

```text
Joint1, Joint2, Joint3, Joint4, Joint5, Joint6
```

- `solveCartesianControlPoints(...)` 可将 TCP 位姿序列转成 `StoredMotionPlan.trajectory`。
- 如果 `11111.txt` 中 TCP 是喷枪 TCP，则用 `CartesianIkToolMode::FixedTool`；如果是法兰位姿，则用 `Flange`。

项目场景与碰撞检测：

```text
SMRobotMotionPlanning/ProjectMotionPlanning/include/ProjectMotionPlanning/ProjectMotionPlanning.h
SMRobotMotionPlanning/ProjectMotionPlanning/src/ProjectMotionPlanning.cpp
PrebuiltPackages/SMRobotCore/include/Collision/CollisionResult.h
PrebuiltPackages/SMRobotPlatform/include/SimulationRuntime/ProjectCollisionRuntime.h
```

关键接口：

```cpp
ProjectPlanningSceneSnapshot::setState(q)
ProjectPlanningSceneSnapshot::validateState(q)
ProjectPlanningSceneSnapshot::validateMotion(q0, q1, options)
ProjectPlanningSceneSnapshot::collisionRuntime()
ProjectCollisionRuntime::checkDetector(detectorId)
ProjectCollisionRuntime::resultOf(detectorId)
CollisionResult::minDistance
CollisionResult::inCollision()
CollisionResult::contacts[i].penetrationDepth
```

已有 CDF demo：

```text
tests/CDF/Algorithms
tests/CDF/Gui
tests/CDF/README.md
```

其中 `tests/CDF/README.md` 已经明确建议：

```text
replace the demo CircularObstacleCdfOracle with an oracle backed by
motion_planning::ProjectPlanningSceneSnapshot / collision distance queries
```

也就是说，项目中已经有 CDF demo 层，但还没有把真实项目碰撞检测器接入 CDF oracle。

## 3. 总体算法流程

总体流程如下：

```text
1. 构建 ABB4600 + burnner 项目场景
2. 创建 RobotObject collision detector
3. 导入 11111.txt 为笛卡尔 TCP 轨迹
4. 用 ABB4600 IK 转为关节初始轨迹 Q0
5. 构建 ProjectPlanningSceneSnapshot
6. 新增 ProjectCollisionCdfOracle
7. 对每个轨迹路点计算 phi(q) 和 grad_phi(q)
8. 用 QP 求 Delta_q 修复碰撞或低 clearance 路点
9. 迭代修复并用 validateMotion 复核整段轨迹
10. 输出 StoredMotionPlan
```

## 4. 场景构建路线

### 4.1 机器人

```cpp
simulation_project::RobotDesc robot;
robot.id = "ABB4600_urdf";
robot.name = "ABB4600_urdf";
robot.sourceType = "urdf";
robot.sourcePath = "data/Spray420/ABB4600_urdf/urdf/ABB4600_urdf.urdf";
robot.visible = true;
robot.collisionEnabled = true;
document.robots.push_back(robot);
```

如果沿用 `config/projects/420.v3.scene.json`，需要注意其中机器人实例 id 是：

```text
ABB4600_urdf_1
```

而不是 `ABB4600_urdf`。实现时必须统一 robot id、jointNames、detector target 中的 id。

### 4.2 障碍物

推荐将 `burnner` 作为静态物体：

```cpp
simulation_project::SceneObjectDesc obstacle;
obstacle.id = "burnner_obstacle";
obstacle.name = "burnner";
obstacle.objectType = "fixture";
obstacle.sourcePath = "data/drake_models/burnner/meshes/base_link.STL";
obstacle.visible = true;
obstacle.collisionEnabled = true;
obstacle.visualScale = 1.0;
obstacle.collisionScale = 1.0;
document.objects.push_back(obstacle);
```

必须设置正确的 `obstacle.transform`。如果 `11111.txt` 的 TCP 位姿与项目世界坐标不一致，则碰撞检测结果没有物理意义。

### 4.3 Collision Detector

推荐使用 `RobotObject` 类型，检测 ABB 全部 link 与 burnner：

```cpp
simulation_project::CollisionDetectorTargetDesc robotTarget;
robotTarget.robotId = "ABB4600_urdf";

simulation_project::CollisionDetectorTargetDesc objectTarget;
objectTarget.objectId = "burnner_obstacle";

simulation_project::CollisionPairGeneratorDesc generator;
generator.type = "RobotObject";
generator.robotId = "ABB4600_urdf";
generator.objectId = "burnner_obstacle";

simulation_project::CollisionDetectorDesc detector;
detector.id = "abb4600_burnner_collision";
detector.name = "ABB4600 vs burnner";
detector.type = "RobotObject";
detector.enabled = true;
detector.contacts = true;
detector.nearestPoints = true;
detector.distance = true;
detector.maxContacts = 32;
detector.distanceThreshold = 0.2;
detector.targets.push_back(robotTarget);
detector.targets.push_back(objectTarget);
detector.pairGenerators.push_back(generator);

document.collision.query.enabled = true;
document.collision.detectors.push_back(detector);
```

`distanceThreshold` 建议设置为 `0.1 m` 到 `0.3 m`。如果过小，远离障碍物时 `minDistance` 可能不可用或不稳定；如果过大，计算成本会上升。

## 5. 轨迹预处理路线

### 5.1 导入 `11111.txt`

```cpp
motion_planning::TrajectoryImportOptions importOptions;
importOptions.robotId = "ABB4600_urdf";
importOptions.jointNames =
    motion_planning::ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
importOptions.cartesianPositionScale = 0.001;

auto imported = motion_planning::ProjectTrajectoryImporter::importFile(
    "C:/Users/14390/Desktop/11111.txt",
    importOptions);
```

预期：

```text
imported.dataKind == TrajectoryImportDataKind::CartesianControlPoints
```

### 5.2 IK 转关节轨迹

```cpp
motion_planning::CartesianIkOptions ikOptions;
ikOptions.robotId = "ABB4600_urdf";
ikOptions.jointNames =
    motion_planning::ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
ikOptions.seedJoints = std::vector<double>(6, 0.0);
ikOptions.toolMode = motion_planning::CartesianIkToolMode::FixedTool;

auto ik = motion_planning::ProjectTrajectoryInverseKinematics::solveCartesianControlPoints(
    document,
    imported.plan,
    ikOptions);
```

得到：

```text
ik.plan.trajectory.points[i].q
```

### 5.3 转成 CDF seed path

```cpp
cdf::Path seed;
for (const auto& point : ik.plan.trajectory.points) {
    seed.push_back(point.q);
}
```

如果存在 IK 失败点：

- 先输出失败 index、目标 TCP 位姿和 final error。
- 可尝试更换 `toolMode`、初始 seed、或者降低原始 TCP 点密度。
- 不能直接进入 QP，因为 QP 是关节空间修复器。

## 6. CDF 与当前碰撞检测器的关系

### 6.1 严格 CDF

官方 CDF 是：

```text
CDF(p, q) = min ||q - q*||
subject to robot at q* contacts spatial point p
```

对障碍物模型，需要：

```text
burnner mesh -> surface points {p_i}
overall distance = min_i CDF(p_i, q)
```

这需要额外生成 `q*` 零水平集数据或训练神经 CDF 网络。

### 6.2 当前工程 surrogate

当前项目没有 ABB4600 的预训练 CDF，也没有 `burnner` 点集到关节空间边界构型库。因此第一阶段采用 collision signed-clearance surrogate：

```text
phi(q) = signed clearance between robot(q) and burnner mesh
```

未碰撞：

```text
phi(q) = minDistance(q) - safetyMargin
```

已碰撞：

```text
phi(q) = -maxPenetrationDepth(q) - safetyMargin
```

其中：

```text
minDistance(q) 来自 CollisionResult::minDistance
maxPenetrationDepth(q) 来自 CollisionResult::contacts
```

这个 `phi(q)` 的单位是米，不是 rad。它不是严格 CDF，但足以构造 QP 避障约束。

### 6.3 如果碰撞状态拿不到有效穿透深度

备用方案是 configuration-space escape search：

```text
1. 从碰撞构型 q_col 出发
2. 在关节空间采样方向 v_j
3. 沿 q(alpha)=q_col+alpha*v_j 线搜索
4. 找到最近无碰撞构型 q_free
5. 近似 phi(q_col) = -||q_free-q_col||
6. 近似 grad_phi(q_col) = normalize(q_free-q_col)
```

这比 workspace penetration 更接近严格 CDF 的关节空间距离，但计算更慢。

## 7. ProjectCollisionCdfOracle 设计

新增类建议命名：

```text
ProjectCollisionCdfOracle
```

第一阶段可以放在 headless feature probe 内部。产品化时建议放入：

```text
SMRobotMotionPlanning/ProjectMotionPlanning/include/ProjectMotionPlanning/ProjectCollisionCdfOracle.h
SMRobotMotionPlanning/ProjectMotionPlanning/src/ProjectCollisionCdfOracle.cpp
```

接口：

```cpp
class ProjectCollisionCdfOracle final : public cdf::ICdfDistanceOracle
{
public:
    ProjectCollisionCdfOracle(
        motion_planning::ProjectPlanningSceneSnapshot& scene,
        std::vector<cdf::JointBounds> bounds,
        double safetyMargin);

    std::size_t dof() const override;
    const std::vector<cdf::JointBounds>& bounds() const override;
    double signedDistance(const cdf::JointVector& q) const override;

private:
    motion_planning::ProjectPlanningSceneSnapshot& m_scene;
    std::vector<cdf::JointBounds> m_bounds;
    double m_safetyMargin = 0.01;
};
```

核心逻辑：

```cpp
double ProjectCollisionCdfOracle::signedDistance(const cdf::JointVector& q) const
{
    std::string error;
    if (!m_scene.setState(q, &error)) {
        return -1.0;
    }

    bool colliding = false;
    double minDistance = std::numeric_limits<double>::infinity();
    double maxPenetration = 0.0;

    auto& collision = m_scene.collisionRuntime();
    for (const auto& detectorId : m_scene.collisionDetectorIds()) {
        const auto check = collision.checkDetector(detectorId);
        if (!check.success) {
            return -1.0;
        }

        const collision::CollisionResult* result = collision.resultOf(detectorId);
        if (result == nullptr) {
            return -1.0;
        }

        minDistance = std::min(minDistance, result->minDistance);
        if (result->inCollision()) {
            colliding = true;
            for (const auto& contact : result->contacts) {
                maxPenetration = std::max(maxPenetration, contact.penetrationDepth);
            }
        }
    }

    if (colliding) {
        return -std::max(maxPenetration, 1.0e-4) - m_safetyMargin;
    }
    return minDistance - m_safetyMargin;
}
```

注意：

- `signedDistance` 是 `const`，但内部会改变 `m_scene` 的 runtime state。可以让成员是非 const 引用，或在产品化时调整接口 const 语义。
- 该 oracle 不应该跨线程共享同一个 `ProjectPlanningSceneSnapshot`。有限差分并行时每个线程应有独立 scene snapshot。

## 8. 梯度计算路线

当前 CDF demo 中已有：

```text
cdf::FiniteDifferenceCdfOracle
```

中心差分：

```text
d_phi / d_q_i ~= [phi(q + eps * e_i) - phi(q - eps * e_i)] / (2 * eps)
```

ABB4600 是 6 轴，每个路点需要：

```text
2 * 6 + 1 = 13
```

次碰撞距离查询。

推荐参数：

```text
finiteDifferenceStep = 5e-4 到 1e-3 rad
safetyMargin = 0.01 到 0.03 m
targetClearance = 0.0 如果 phi 已经减去 safetyMargin
interpolationStep = 0.03 到 0.04 rad
```

如果 `norm(grad_phi)` 太小：

1. 增大 `finiteDifferenceStep`。
2. 使用一侧差分。
3. 使用 escape search 方向作为备用梯度。
4. 将该路点标记为需要 OMPL 局部重连。

## 9. QP 轨迹修复数学形式

对轨迹中间路点 `q_k`，求修正量 `Delta_q_k`。

线性化避障约束：

```text
phi(q_k) + grad_phi(q_k)^T Delta_q_k >= d_target
```

等价写成 QP 标准不等式：

```text
-grad_phi(q_k)^T Delta_q_k <= phi(q_k) - d_target
```

关节限位：

```text
q_min <= q_k + Delta_q_k <= q_max
```

信赖域：

```text
-Delta_q_max <= Delta_q_k <= Delta_q_max
```

目标函数：

```text
min 0.5 * Delta_q^T H Delta_q + f^T Delta_q
```

建议由三项组成：

```text
1. 修正量小:
   w_c * ||Delta_q_k||^2

2. 轨迹平滑:
   w_s * ||q_{k-1} - 2*(q_k + Delta_q_k) + q_{k+1}||^2

3. 保持接近原始轨迹:
   w_t * ||(q_k + Delta_q_k) - q_k_seed||^2
```

第一阶段可以复用当前 `CdfPlanningService.cpp` 中的简化 QP-like correction：

```text
tests/CDF/Algorithms/src/CdfPlanningService.cpp
solveCdfQpCorrection(...)
```

它的逻辑是：

```text
1. 先加平滑项，往邻点 midpoint 靠近
2. 如果 clearance 不够，则沿 gradient 方向补足 requiredIncrease
3. trust region 限制单步修正量
4. clamp 到关节限位
```

如果后续要写成严格 QP，可接入：

```text
OSQP / qpOASES / CasADi qpsol
```

当前 CMake 项目中不应第一步就引入新第三方依赖。建议先用已有 correction 跑通，再替换为正式 QP solver。

## 10. 初始轨迹碰撞时的处理

如果初始关节轨迹与 `burnner` 碰撞，不代表不能计算 `phi` 和梯度。分三种情况：

### 10.1 轻微碰撞

`CollisionResult::contacts` 能返回 penetration depth。

使用：

```text
phi(q) = -maxPenetrationDepth(q) - safetyMargin
```

然后正常有限差分。

### 10.2 中等或深度碰撞

穿透深度不稳定，或梯度方向接近 0。

使用 escape search：

```text
grad_phi(q) = normalize(q_free - q_col)
phi(q) = -||q_free - q_col||
```

这时单位会从米变成关节空间范数。实现上需要记录 `DistanceSource`，避免混用尺度。

### 10.3 长段深度碰撞

如果连续大段轨迹穿过障碍物，局部 QP 很可能失败。建议：

```text
碰撞段提取 -> OMPL/RRTConnect 局部重连 -> 得到粗略无碰撞 seed -> CDF/QP 平滑修复
```

因此最终规划器应支持 fallback：

```text
IK seed
  -> CDF/QP repair
  -> if failed: OMPL global/local seed
  -> CDF/QP repair again
```

## 11. 推荐实现阶段

### 阶段 A：headless 实验程序

新增目录：

```text
SMRobotMotionPlanning/ProjectMotionPlanning/feature_probes/ABB4600BurnnerCdfRepair
```

并在：

```text
SMRobotMotionPlanning/ProjectMotionPlanning/feature_probes/CMakeLists.txt
```

添加：

```cmake
add_subdirectory(ABB4600BurnnerCdfRepair)
```

实验程序功能：

```text
1. 加载或构造 ABB4600 + burnner ProjectDocument
2. 导入 C:/Users/14390/Desktop/11111.txt
3. 调 IK 生成 seed path
4. 构造 ProjectPlanningSceneSnapshot
5. 创建 ProjectCollisionCdfOracle
6. 调用 cdf::CdfPlanningService::plan
7. validateMotion 复核结果
8. 输出 CSV / StoredMotionPlan / 诊断报告
```

建议链接目标参考：

```text
tests/CDF/Gui/smoke/CdfProjectCollisionSmoke.cpp
SMRobotMotionPlanning/ProjectMotionPlanning/regression/ProjectMotionPlanningHeadless/main.cpp
```

需要链接的典型目标：

```cmake
target_link_libraries(ABB4600BurnnerCdfRepair
    PRIVATE
        CDF::Algorithms
        SMRobotMotionPlanning::ProjectMotionPlanning
        SMRobotPlatform::SimulationProject
        SMRobotPlatform::SimulationRuntime
        SMRobotCore::Collision
        SMRobotCore::RobotCore
        SMRobotCore::RobotIO
        SMRobotCore::RobotRuntime
)
```

实际 target 名以当前 CMake 包变量和已有 probe 写法为准。

### 阶段 B：抽出 reusable adapter

将 `ProjectCollisionCdfOracle` 从 feature probe 移到 `ProjectMotionPlanning` 模块。

新增公开接口：

```text
ProjectCollisionCdfOracle
ProjectCdfQpPlanningOptions
ProjectCdfQpPlanningResult
ProjectCdfQpPlanningService
```

服务接口可设计为：

```cpp
class ProjectCdfQpPlanningService
{
public:
    ProjectCdfQpPlanningResult repair(
        ProjectPlanningSceneSnapshot& scene,
        const robottrajectory::JointTrajectory& seed,
        const ProjectCdfQpPlanningOptions& options) const;
};
```

### 阶段 C：正式 QP solver

在已有 QP-like correction 跑通后，再接入真正 QP 求解器。

推荐先做路点级 QP：

```text
每个中间 waypoint 单独解一个小 QP
```

之后再扩展为 banded trajectory QP：

```text
一次性优化整段 Delta_Q = [Delta_q_1, ..., Delta_q_N]
```

路点级 QP 更易调试；整段 QP 平滑性更好但实现复杂。

### 阶段 D：严格 CDF 可选增强

如果后续要更贴近论文 CDF：

```text
1. burnner mesh 表面采样为点集 P
2. 对 ABB4600 生成接触这些点的 q* 数据
3. 训练 CDF(p, q) 网络或建立近邻库
4. QP 中使用 min_i CDF(p_i, q)
```

这是第二条技术路线，不建议和第一阶段工程 surrogate 混在一起。

## 12. 验证指标

每次规划输出必须记录：

```text
1. imported control point count
2. IK success count / failed point indices
3. seed waypoint count
4. seed min phi
5. repaired waypoint count
6. repaired min phi
7. repair iteration count
8. max correction norm
9. validateMotion segment failure count
10. final stored trajectory path
```

通过标准：

```text
1. 起点和终点 validateState 通过
2. 所有相邻段 validateMotion 通过
3. min phi >= targetClearance
4. 单步 Delta_q 不超过 trust region
5. 轨迹没有明显抖动或关节突变
```

## 13. 参数建议

初始参数：

```text
safetyMargin = 0.01 m
targetClearance = 0.0
finiteDifferenceStep = 5e-4 rad
interpolationStep = 0.03 rad
maxRepairIterations = 100
repairGain = 0.5 到 0.8
smoothGain = 0.05 到 0.12
trustRegion = 0.015 到 0.03 rad
validation.maxJointStep = 0.03 到 0.04 rad
```

如果修复失败：

```text
1. 降低 interpolationStep
2. 增大 maxRepairIterations
3. 检查 burnner transform 和 TCP 坐标系
4. 检查 11111.txt 导入时间与单位
5. 对碰撞段用 OMPL 重连
6. 再做 CDF/QP 修复
```

## 14. 下一步实施清单

建议下一步直接做：

```text
1. 新建 feature probe: ABB4600BurnnerCdfRepair
2. 在 probe 内实现 makeAbb4600BurnnerDocument()
3. 实现 import11111AndSolveIk()
4. 实现 toCdfPath()
5. 实现 ProjectCollisionCdfOracle
6. 调用 CdfPlanningService::plan()
7. 对 repairedPath 全段 validateMotion
8. 输出 result CSV 和诊断日志
```

最小可运行命令目标：

```text
ABB4600BurnnerCdfRepair
  --trajectory C:/Users/14390/Desktop/11111.txt
  --robot data/Spray420/ABB4600_urdf/urdf/ABB4600_urdf.urdf
  --obstacle data/drake_models/burnner/meshes/base_link.STL
  --detector abb4600_burnner_collision
  --output C:/Users/14390/Desktop/abb4600_burnner_cdf_repaired.csv
```

第一版可以先不做命令行参数，路径写死，跑通后再抽象。

## 15. 当前最重要的实现风险

1. 坐标系风险：
   - `11111.txt` 的 TCP 位姿、ABB 基座、burnner 位姿必须在同一世界坐标系下。

2. 时间戳风险：
   - `11111.txt` 是矩阵后接时间，现有 parser 可能把单独时间行用于下一帧。

3. 严格 CDF 命名风险：
   - 当前方案不是论文严格 CDF。论文/报告中应写为 CDF-inspired 或 collision-distance surrogate。

4. 梯度噪声风险：
   - 有限差分需要多次碰撞查询，mesh 最近特征切换时梯度可能不连续。

5. 深度碰撞风险：
   - 大段碰撞时局部 QP 无法绕过拓扑障碍，需要 OMPL seed。

## 16. 后续继续实施时的入口文件

优先参考这些文件：

```text
tests/CDF/README.md
tests/CDF/Algorithms/include/CDFAlgorithms/CdfTypes.h
tests/CDF/Algorithms/include/CDFAlgorithms/CdfCollisionOracle.h
tests/CDF/Algorithms/src/CdfCollisionOracle.cpp
tests/CDF/Algorithms/src/CdfPlanningService.cpp
tests/CDF/Gui/smoke/CdfProjectCollisionSmoke.cpp
SMRobotMotionPlanning/ProjectMotionPlanning/include/ProjectMotionPlanning/ProjectMotionPlanning.h
SMRobotMotionPlanning/ProjectMotionPlanning/src/ProjectMotionPlanning.cpp
SMRobotMotionPlanning/ProjectMotionPlanning/include/ProjectMotionPlanning/TrajectoryImport.h
SMRobotMotionPlanning/ProjectMotionPlanning/src/TrajectoryImport.cpp
SMRobotMotionPlanning/ProjectMotionPlanning/include/ProjectMotionPlanning/TrajectoryInverseKinematics.h
SMRobotMotionPlanning/ProjectMotionPlanning/src/TrajectoryInverseKinematics.cpp
SMRobotMotionPlanning/ProjectMotionPlanning/regression/ProjectMotionPlanningHeadless/main.cpp
```

如果继续编码，优先完成 headless feature probe。只有 headless 能稳定输出无碰撞 repaired path 后，再考虑 UI 集成或正式 API。
