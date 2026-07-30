# OMPL 运动规划与现有 Project 适配修订方案

更新时间：2026-07-21

## 1. 结论

平台只保留一套项目事实：现有 `simulation_project::ProjectDocument`、`ProjectSession` 和 `ProjectSimulationRuntime`。不新增所谓“OMPL Project”，也不让 OMPL 持有或保存另一份项目文档。

OMPL 需要的不是新的项目格式，而是一次规划任务所需的以下能力：

- 目标机器人和稳定关节顺序。
- 每个规划关节的状态空间类型和上下限。
- 起点、终点和规划约束。
- 给定关节状态时判断状态是否合法。
- 判断两个关节状态之间的运动是否合法。
- 超时、取消、随机种子和规划参数。

这些能力应由现有项目构建出的只读 `PlanningSceneSnapshot` 提供。它是一次规划任务的运行时快照，不是可持久化的第二套 project。

## 2. 当前代码事实

- 已有 `SMRobotMotionPlanning/MotionPlanningCore`，当前实现是无碰撞检查的关节直线插值。
- 已有 `RobotTrajectoryCore::JointTrajectory`，可以承载带时间的关节轨迹。
- 已有 `MotionPlanningProjectStore`，通过 `motion_planning.trajectories` project extension 保存轨迹。
- 已有 `ProjectSimulationRuntime` 和 `ProjectCollisionRuntime`，可以脱离 Qt 加载项目、更新机器人状态和执行 project collision detector。
- 已有 `SMRobotWorkbenchMotionPlanning`，但 Qt controller 当前直接构造 `LinearJointMotionPlanner`。
- `RobotRunWorkbench` 已能读取已保存轨迹并加载到 `RobotTrajectoryExecutionSession`。
- 当前 `RobotModel::RobotJoint` 没有正式的关节位置、速度和加速度限制；URDF joint limit 读取仍被注释。
- 当前 `Kinematics` 只有正运动学，第一阶段不能把 TCP pose 直接当作规划终点。
- `config/projects/420-red4600-tool.sys.json` 已提供适合第一轮集成验证的真实场景：`Red4600` 的 `Link6` 通过 `ToolMount` 挂载 `420_tool_attachment`，转台机器人为 `ATPPZ350`。
- 该项目的 `default_collision` detector 已启用，明确检查工具附件和 `Red4600 Link6` 与 `ATPPZ350 base_link/Link1/Link2` 的碰撞关系。
- 工具附件、`Red4600 Link6` 和 `ATPPZ350` 的关键 link 已配置 Box/AABB proxy，且在 detector 中以 `geometryRole: Exact` 使用，计算量较小但仍经过正式 project collision runtime，适合先做确定性的规划闭环。
- 独立场景对象 `420_tool` 的 `collisionEnabled` 为 false；本验证必须使用挂载后的 `420_tool_attachment`，不能误把隐藏的源对象当作实际工具碰撞体。
- 已有 `Collision-CoacdDecompositionViewer` 可参考 GLFW/GLRuntime 的 feature probe 启动、CMake target、DLL 复制和交互写法；已有 `RobotRenderBridge/feature_probes/ProjectGlfwViewer/CMakeLists.txt` 说明平台原本也预留了 project GLFW viewer，但当前目录缺少 `main.cpp`，不能把它当作已经可运行的验证程序。

## 3. 唯一 Project 与规划快照

### 3.1 所有权

```text
ProjectDocument / ProjectSession
    唯一持久项目事实
    robot、object、attachment、collision detector、filter、保存加载

ProjectSimulationRuntime
    当前仿真执行状态

PlanningSceneSnapshot
    从 ProjectDocument + projectBasePath 构建
    一次规划任务私有
    只读场景事实 + 可变候选机器人状态
    不保存为新项目，不反向修改 ProjectDocument

OMPL
    只消费状态空间和 validity 接口
    不知道 ProjectDocument、Qt、FCL 或项目 schema
```

### 3.2 构建流程

```text
ProjectMotionPlanningService::plan(...)
    -> 捕获 ProjectDocument revision 和 projectBasePath
    -> ProjectPlanningSceneBuilder::build(...)
    -> 创建任务私有 ProjectSimulationRuntime
    -> 创建任务私有 ProjectCollisionRuntime
    -> 解析 robotId、joint order、joint bounds、detectorIds
    -> 生成 PlanningSceneSnapshot
    -> 调用 IMotionPlanner / OmplMotionPlanner
```

规划期间如果 GUI 修改了项目，正在运行的 snapshot 不跟随变化。规划结果记录源 revision；保存、预览或执行前如果 revision 已变化，必须重新验证或标记为 stale。

## 4. 建议接口

### 4.1 与项目无关的核心数据

建议保留在 `SMRobotMotionPlanning::MotionPlanningCore`：

```cpp
struct JointBound
{
    double lower = 0.0;
    double upper = 0.0;
    double maxVelocity = 0.0;
    double maxAcceleration = 0.0;
    bool continuous = false;
};

struct JointPlanningProblem
{
    std::string robotId;
    std::vector<std::string> jointNames;
    std::vector<JointBound> jointBounds;
    std::vector<double> start;
    std::vector<double> goal;
    PlannerOptions planner;
    MotionValidationOptions validation;
    TrajectoryPostProcessOptions postProcess;
};

struct StateValidationResult
{
    bool valid = false;
    std::string diagnosticCode;
    std::string message;
};

class IPlanningScene
{
public:
    virtual ~IPlanningScene() = default;
    virtual StateValidationResult validateState(
        const std::vector<double>& jointValues) = 0;
    virtual StateValidationResult validateMotion(
        const std::vector<double>& from,
        const std::vector<double>& to,
        const MotionValidationOptions& options) = 0;
};

class IMotionPlanner
{
public:
    virtual ~IMotionPlanner() = default;
    virtual MotionPlanningResult plan(
        const JointPlanningProblem& problem,
        IPlanningScene& scene,
        CancellationToken* cancellation) const = 0;
};
```

公共接口不暴露 `ompl::*`、Qt、FCL、`ProjectDocument` 或具体 runtime 类型。

### 4.2 项目适配接口

建议新增真实组件 `ProjectMotionPlanning`，而不是创建空的通用目录：

```cpp
struct ProjectPlanningRequest
{
    std::string robotId;
    std::vector<std::string> jointNames;
    std::vector<double> start;
    std::vector<double> goal;
    std::vector<std::string> collisionDetectorIds;
    PlannerOptions planner;
    MotionValidationOptions validation;
    TrajectoryPostProcessOptions postProcess;
};

class ProjectMotionPlanningService
{
public:
    MotionPlanningResult plan(
        const simulation_project::ProjectDocument& document,
        const std::filesystem::path& projectBasePath,
        const ProjectPlanningRequest& request,
        CancellationToken* cancellation = nullptr) const;
};
```

`ProjectMotionPlanningService` 负责把现有项目事实转成 `JointPlanningProblem + IPlanningScene`。Qt、CLI、测试和未来 SDK consumer 都调用同一个入口。

### 4.3 碰撞 detector 的选择

Qt 不传入 widget callback 或任意函数指针。用户在界面选择现有项目中的 detector ID，request 保存 detector ID：

```text
Workbench detector selection
    -> ProjectPlanningRequest.collisionDetectorIds
    -> PlanningSceneSnapshot resolves detector IDs
    -> set candidate joint state in private runtime
    -> ProjectCollisionRuntime::update(...)
    -> checkDetector(detectorId)
    -> all selected detectors must be collision free
```

以下情况必须作为无效请求返回，不能默认视为无碰撞：

- detector 不存在。
- detector 被禁用。
- detector 没有构建目标机器人所需的 collision object。
- collision query 执行失败或没有可靠结果。

不能默认使用 `checkAllDetectors()`，因为与规划机器人无关的 object-object detector 也可能阻止所有状态。第一阶段应明确选择一个或多个与目标机器人相关的 detector。

## 5. 模块结构与依赖方向

```text
SMRobotMotionPlanning/
    MotionPlanningCore/
        request、result、planner/scene interface、diagnostic

    MotionPlanningOmpl/
        OMPL state space、planner registry、sampler、termination
        OMPL 类型保持 private

    ProjectMotionPlanning/
        ProjectDocument -> PlanningSceneSnapshot
        ProjectSimulationRuntime / ProjectCollisionRuntime adapter
        MotionPlanRepository 和 project extension 兼容

SMRobotWorkbenchMotionPlanning/
    Qt widget/controller
    只构造 request、发起异步任务、显示结果和提交保存命令

SMRobotMotionPlanning/ProjectMotionPlanning/feature_probes/
    ProjectOmplPlanningViewer/
        无 Qt 的项目规划集成消费者
        GLFW/GLRuntime 窗口 + platform scene/render bridge
        不拥有规划、碰撞、项目加载或轨迹保存业务逻辑
```

依赖方向：

```text
SMRobotWorkbenchMotionPlanning
    -> SMRobotMotionPlanning::ProjectMotionPlanning

ProjectMotionPlanning
    -> MotionPlanningCore
    -> MotionPlanningOmpl
    -> SMRobotPlatform::SimulationProject
    -> SMRobotPlatform::SimulationRuntime

MotionPlanningOmpl
    -> MotionPlanningCore
    -> ompl::ompl (PRIVATE)

MotionPlanningCore
    -> SMRobotCore::RobotTrajectoryCore

ProjectOmplPlanningViewer executable
    -> ProjectMotionPlanning
    -> SMRobotPlatform::SimulationRuntime
    -> SMRobotPlatform::RobotRenderBridge / VisualizationSDK
    -> glfw::glfw / Common::GLRuntime
```

`SMRobotCore`、`SMRobotPlatform` 和 OMPL 均不得依赖 Qt Workbench。

`ProjectOmplPlanningViewer` 应放在 motion planning 的 feature probe 下，而不是放进 `Collision`。它可以参考 `CoacdDecompositionViewer` 的可执行程序组织方式，但不得让 `Collision` 反向依赖 project、OMPL 或 renderer。探针 target 的依赖全部使用 `PRIVATE`，不进入任何正式库的公共依赖。

## 6. OMPL 安装检查与 CMake 接入

已核对并按实际版本修正的安装目录：

```text
D:/PreBuild/vs2019/ompl-2.0.1
```

实际检查结果：

- 安装目录名为 `ompl-2.0.1`，与 package metadata 和头文件版本一致。
- `omplConfig.cmake`、`omplConfigVersion.cmake` 和 `ompl/config.h` 报告实际版本为 `2.0.1`。
- include 目录为 `include/ompl-2.0`。
- 导出 target 为 `ompl::ompl`。
- Debug 静态库为 `lib/ompl-d.lib`。
- Release 静态库为 `lib/ompl.lib`。
- 没有 OMPL DLL。
- 导出 target 传递依赖 `Boost::serialization`、`Eigen3::Eigen` 和 `Threads::Threads`。
- config 中遗留了一个生成机器上的 `E:/ws_prebuild/.../eigen3` deprecated include 变量；项目必须链接 imported target `ompl::ompl`，不要消费旧的 `OMPL_INCLUDE_DIRS`。

本轮 CMake 接入：

- 在 `D:/PreBuild/vs2019/cmake/CMake_FindOMPL.cmake` 增加查找脚本。
- 默认从相邻 `../ompl-2.0.1/share/ompl/cmake` 查找 config。
- 使用 `find_package(ompl 2.0.1 EXACT CONFIG REQUIRED)` 校验准确版本。
- 支持通过 `SMROBOT_OMPL_INSTALL_DIR` 覆盖安装 prefix。
- 使用 `NO_DEFAULT_PATH`，避免误命中系统里的另一套 OMPL。
- 验证 `ompl::ompl` target 存在。
- 为 `RelWithDebInfo` 和 `MinSizeRel` 映射 Release 库。
- 在 `cmake/UserConfigs/TANGTANG_4090_Windows.cmake` 中，在 Boost/Eigen 可用后 include 查找脚本。
- 当前 `MotionPlanningCore` 尚未使用 OMPL，因此本轮只注册 target，不给现有线性规划 target 增加无用链接依赖。后续由 `MotionPlanningOmpl` 私有链接 `ompl::ompl`。

外部安装目录已经从原来的错误名称修正为 `ompl-2.0.1`，查找脚本、项目配置和本计划统一使用该名称。OMPL 包内部由安装过程生成的版本文件保持不变。

## 7. 分阶段实施

### 阶段 A：基础模型和 runtime 接口

- 在 RobotCore 的正确所有权层补充 joint bounds、velocity 和 acceleration 信息。
- 恢复 RobotIO 的 URDF joint limit 解析。
- 为 `ProjectSimulationRuntime` 增加一次 FK/attachment 更新的批量 `setRobotJointValues(...)`。
- 明确第一阶段只支持有界 revolute/prismatic joint；continuous、loop 或缺少 bounds 时返回诊断。

### 阶段 B：Project planning snapshot

- 实现 `ProjectPlanningSceneBuilder`。
- 为每个规划任务创建私有 simulation/collision runtime。
- 实现 joint vector 到 runtime joint state 的稳定映射。
- 实现 detector ID 校验、state validity 和离散 motion validity。
- 记录 project revision、robot model identity 和 collision configuration identity。

### 阶段 C：OMPL backend

- 新增 `MotionPlanningOmpl` target，私有链接 `ompl::ompl`。
- 第一版支持 `RRTConnect + Uniform sampler`。
- 支持 timeout、atomic cancellation、固定 seed 和统计信息。
- 路径简化和插值后再次逐段验证。
- OMPL 输出先形成无时间的 joint path，再进行时间参数化生成 `JointTrajectory`。

### 阶段 D：可重复的 headless 闭环

- 加载真实项目。
- 从项目构建 planning snapshot。
- 指定 robot、start、goal 和 detector。
- 规划、复验、时间参数化、保存、重新加载和执行。
- 补齐当前缺失的 `SimRuntime-HeadlessCollision` regression `main.cpp`，再增加 motion planning regression。

### 阶段 E：无 Qt 的 GLFW 可视化集成探针

- 新增 `ProjectOmplPlanningViewer`，CMake 组织参考 `SMRobotCore/Collision/feature_probes/CoacdDecompositionViewer/CMakeLists.txt`，项目场景渲染参考 `RobotRenderBridge/feature_probes/ProjectGlfwViewer` 预留的依赖方向。
- 默认加载 `config/projects/420-red4600-tool.sys.json`，但同时提供 `--project` 参数，避免把业务逻辑硬编码到示例。
- 第一轮固定规划机器人为 `Red4600`，保持 `ATPPZ350` 为项目初始静态状态，使用 detector `default_collision`，并确认 `420_tool_attachment` 随 `Link6` 正确更新。
- 在转台左、右两侧各准备一个无碰撞的命名关节构型。通过 FK 检查工具参考点/TCP 的世界坐标确实位于转台两侧，并把最终关节向量作为 probe fixture 或命令行参数保存，不写回源 project。
- 第一轮所称“起始位姿、目标位姿”是上述两个关节构型及其 FK 位姿，不接受裸 TCP pose 作为规划输入；等独立 IK 接口完成后再增加 TCP pose 输入。
- 选择测试构型时，除要求起点和终点各自无碰撞外，还要求起终点的直接关节插值会与转台相撞，而场景中存在绕行通道。若直接插值本身无碰撞，该组构型只能用于 smoke test，不能作为碰撞绕行验收用例。
- 调用正式 `ProjectMotionPlanningService` 和 `RRTConnect` 生成路径；viewer 不直接构造 OMPL state space、不直接调用 FCL，也不复制 project collision 规则。
- 规划成功后对完整路径逐段复验，再转换为 `JointTrajectory`；窗口按仿真时间回放 `Red4600` 和已挂载工具的运动。
- 视图至少显示项目机器人/工具/转台、起点、终点、规划路径和当前回放状态；可切换 collision proxy、接触点和轨迹显示。颜色建议固定为起点绿色、终点蓝色、有效路径青色、碰撞采样或失败段红色。
- 支持 `--seed`、`--timeout`、`--start`、`--goal`、`--detector`、`--no-window` 和显式的 `--save-trajectory`。默认不修改源 project，`--no-window` 必须复用同一规划调用并可用于自动回归。
- 探针只负责启动、参数解析、键盘/鼠标控制、调用服务和渲染 view model；规划结果和诊断必须来自非 Qt 模块。

### 阶段 F：Qt Workbench 与运行预览

- Workbench 捕获当前关节状态作为 start/goal。
- 从项目列出适用 collision detector 和 planner descriptor。
- 后台执行规划，支持 progress/cancel。
- 通过 document command 保存结果并发布 typed event。
- 通过 Platform visualization service 显示 TCP path、起止状态和轨迹 preview。
- Robot Run 从项目轨迹实体加载并通过 runtime 仿真时钟执行，不让 Qt adapter 长期拥有轨迹执行真相。

### 后续阶段

- TCP pose goal 与 IK candidate 生成。
- continuous joint compound state space。
- RRTstar、BITstar、PRM 和 sampler registry。
- clearance objective、路径长度和关节权重优化。
- 连续碰撞检测、动态障碍物和多机器人协同规划。

## 8. 验证标准

### CMake

- configure 输出实际 OMPL 版本 `2.0.1`。
- `ompl::ompl` 的 Debug/Release imported location 分别指向 `ompl-d.lib` 和 `ompl.lib`。
- 不依赖硬编码 `E:/ws_prebuild` include。
- 不使用全局 include/link directory。

### Headless

- 起点碰撞和终点碰撞被拒绝。
- detector 不存在、禁用或查询失败被拒绝。
- 直线相撞但存在绕行空间时，OMPL 能返回复验通过的路径。
- 不存在路径时返回 `NoSolution`，不返回空的 Success。
- 固定 seed 的 regression 可复现。
- timeout 和 cancel 能终止规划。
- 每个结果路径段按配置的最大关节步长重新碰撞验证。

### GLFW 中期集成

- `ProjectOmplPlanningViewer` 的可执行依赖中不包含 Qt，关闭窗口后无后台线程、runtime 或 OpenGL 资源泄漏。
- 默认项目成功加载 `Red4600`、`ATPPZ350`、`420_tool_attachment` 和 `default_collision`，并能显示配置的 Box/AABB collision proxy。
- 转台左右两个命名关节构型分别通过起点/终点合法性验证，FK 结果也符合左右空间关系。
- 直接插值的碰撞复验必须失败，`RRTConnect` 返回的绕行路径必须通过相同 detector 的逐段复验。
- 固定 seed 和 fixture 时，命令行 `--no-window` 得到可重复的成功/失败结论；GUI 显示与该 headless 结果使用同一服务入口。
- 回放期间 `Red4600`、`Link6` 和 `420_tool_attachment` 的变换保持一致，不能出现工具留在起点或只移动可视模型而碰撞模型未更新。
- 保存功能只有在显式指定输出时生效，输出轨迹可被重新加载；默认运行不改变 `config/projects/420-red4600-tool.sys.json`。

### GUI

- 规划不阻塞 Qt 主线程。
- Project 修改后旧结果被标记 stale。
- Apply 后由 document event 刷新 Motion Planning、Robot Run 和 viewport。
- Cancel/退出任务后清理 preview，不修改持久 project。

## 9. 风险和停止条件

- 如果添加 joint limit 必须改变对外稳定 ABI，需要先确定版本升级或兼容接口，不直接扩展现有虚函数表。
- 如果 OMPL state checker 需要与渲染线程共享可变 runtime，停止该方案；必须改为任务私有 snapshot/runtime。
- 如果离散碰撞验证无法给出可靠的最大步长，应先补 motion validation 契约，不能仅调小一个全局百分比后宣称安全。
- 如果规划期间要支持动态障碍物或多机器人同时运动，应作为独立阶段设计，不能混入第一版静态场景规划。
- 如果用户输入的是 TCP pose 而非 joint state，必须先建立独立 IK 接口和多解筛选，不把 IK 临时写进 Qt controller 或 OMPL adapter。
- 如果无法为默认项目找到“起终点均合法、直接插值碰撞、OMPL 可绕行”的稳定构型，不得放宽碰撞规则或关闭工具附件来制造成功；应把该场景降级为 smoke test，并另外建立最小确定性障碍场景作为绕行 regression。
- 如果 GLFW probe 为了显示而开始复制 project loader、FK、attachment propagation 或 collision geometry 构建逻辑，应停止并补齐 Platform bridge/view model 接口；探针不能成为第二套仿真平台。

## 10. 里程碑与进入 Qt 的门槛

### 10.1 第一里程碑：无窗口算法闭环

第一里程碑定义为：

```text
不启动 Qt
-> 加载现有 ProjectDocument
-> 构建一次性 PlanningSceneSnapshot
-> 选择 robot 和 project collision detector
-> 以 joint state 为 start/goal
-> 使用 RRTConnect 规划
-> 对完整路径逐段碰撞复验
-> 生成并保存 JointTrajectory
```

这一里程碑通过自动化 regression 证明算法、项目、碰撞和轨迹契约独立于任何窗口系统。

### 10.2 中期里程碑：GLFW 项目规划可视化

```text
不链接、不启动 Qt
-> ProjectOmplPlanningViewer 加载 420-red4600-tool.sys.json
-> 构建与 headless regression 相同的 PlanningSceneSnapshot
-> 固定 Red4600 + default_collision，保持 ATPPZ350 静态
-> 加载转台左右两个命名关节构型并用 FK 确认空间位置
-> 证明直接插值碰撞
-> 调用 ProjectMotionPlanningService / RRTConnect 找到绕行路径
-> 使用同一 detector 逐段复验
-> 在 GLFW 中显示起点、终点、AABB proxy、轨迹并回放机器人和工具
-> 可选导出 JointTrajectory，默认不修改源 project
```

该中期里程碑非常有价值，因为它同时暴露纯 headless 测试不容易发现的问题：project scene 与 collision scene 是否一致、attachment 是否跟随、轨迹关节顺序是否正确、路径是否能被 runtime 连续回放，以及算法接口是否真的没有 Qt 依赖。

### 10.3 进入 Qt 集成的门槛

只有以下条件同时满足后，才开始接入 `SMRobotWorkbenchMotionPlanning` 和 `Robot Run`：

- headless regression 稳定通过。
- GLFW 探针能用同一请求和同一 collision detector 规划、复验、回放。
- `ProjectMotionPlanningService`、`MotionPlanningOmpl` 和轨迹结果类型的公共接口中没有 Qt、GLFW、OpenGL、FCL 或 `ompl::*` 类型。
- 已确认 start/goal、joint order、attachment、collision proxy 和路径回放使用同一 runtime 语义。
- 探针中的业务调用可以原样被 Qt controller 使用；Qt 集成仅增加异步调度、控件输入、document command/event 和 viewport 表现，不重写规划算法。

## 11. 阶段 A-E 实施结果（2026-07-21）

阶段 A-E 已按本方案完成，未进入阶段 F 的 Qt Workbench 接线。

### 11.1 已完成内容

- 阶段 A：`RobotJoint` 已承载 bounded/continuous、位置、速度和加速度限制；`URDFLoader` 已恢复 URDF position/velocity limit 解析；`ProjectSimulationRuntime` 已提供一次 FK 和 attachment propagation 的批量关节更新。
- 阶段 B：新增 `ProjectMotionPlanning`，由现有 `ProjectDocument + projectBasePath` 构建任务私有的 `ProjectSimulationRuntime + ProjectCollisionRuntime`。它不是第二套 project，也不修改源 project。
- 阶段 C：新增 `MotionPlanningOmpl`，OMPL 仅作为私有实现依赖；公共接口不暴露 `ompl::*`、Qt、GLFW、OpenGL 或 FCL 类型。第一版实现 `RRTConnect`、固定 seed、timeout/cancel、离散 motion validation、确定性 rope shortcut、插值和时间化 `JointTrajectory`。
- 阶段 D：新增 `ProjectMotionPlanning-Headless` regression，真实加载 `420-red4600-tool.sys.json`，验证 detector、碰撞起点、左右构型、直线碰撞、OMPL 绕行、逐段复验、固定 seed 可复现、取消、project extension 保存/重载和 runtime 执行。
- 阶段 E：新增 `ProjectOmplPlanningViewer`。`--no-window` 与 GLFW 显示复用同一个 `ProjectMotionPlanningService`；窗口显示 project scene、collision proxy、起终点、轨迹和机器人/工具回放，并支持显式 CSV 导出。
- OMPL 安装目录已按头文件版本统一为 `D:/PreBuild/vs2019/ompl-2.0.1`；`D:/PreBuild/vs2019/cmake/CMake_FindOMPL.cmake` 使用 `find_package(ompl 2.0.1 EXACT CONFIG REQUIRED)` 和 imported target `ompl::ompl`，工程通过用户配置脚本引入。

### 11.2 固定验收场景

- 规划机器人：`Red4600`；静态转台：`ATPPZ350`；工具：`420_tool_attachment`；detector：`default_collision`。
- 起点工具位置：`(1.52547, -1.01344, 1.1423)`。
- 终点工具位置：`(0.507403, 0.614208, 0.175197)`。
- 起终点均合法，直接关节插值发生碰撞；固定 seed 的 `RRTConnect` 生成 80 个时间化 waypoint，最终每段均通过同一 detector 复验。

### 11.3 验证证据

- CMake/Release 构建通过：`ProjectMotionPlanning-Headless`、`ProjectOmplPlanningViewer`、`RobotQtViewer`。
- CTest：`ProjectMotionPlanningHeadless` 1/1 通过。
- `ProjectOmplPlanningViewer --no-window`：通过，并成功导出 CSV。
- `ProjectOmplPlanningViewer --hidden --max-frames 5`：通过；OpenGL 3.3、NVIDIA RTX 4090 初始化和实际渲染循环正常退出。
- `dumpbin /dependents`：GLFW 探针依赖中没有 Qt DLL。

### 11.4 当前边界

- 第一版输入仍是 joint state；TCP pose 目标需要后续独立 IK 阶段。
- 当前 motion validation 是按 `maxJointStep` 的离散检查，不宣称连续碰撞检测。
- `NoSolution` 与 `Timeout` 状态均已定义；对采样式 `RRTConnect`，无法证明无解时通常由时间终止条件返回 `Timeout`，不会以空轨迹伪装 Success。
- 阶段 F 尚未执行；Qt 端下一步只应增加异步任务、参数面板、document command/event、preview 和 Robot Run 接线，不应复制规划或碰撞逻辑。
