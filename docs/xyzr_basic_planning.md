# XYZR 四轴机器人在 Basic Planning 中的逆解与播放

## 使用步骤

1. 启动重新编译后的 `RobotQtViewer`。已经打开的旧进程仍使用旧代码，需要自行重启程序后使用新功能。
2. 打开项目内的 `config/projects/xyzr-tongban-demo.sys.json`。该工程使用随项目提供的机器人演示副本和铜板模型，路径相对工程文件解析，不依赖桌面原始机器人目录。
3. 进入 `Motion Planning` → `Basic Planning`，在场景中选择 `xyzr_spray_robot_no_workpiece` 机器人。
4. 导入 `data/trajectories/xyzr_tongpian.txt`，也可导入原来的桌面 `tongpian.txt`。这两个文件内容相同。矩阵位置的单位是 mm，导入后按项目既有流程转换为 m。
5. 逆解目标选择 `with tool`，运行现有 IK 按钮。完整轨迹应得到 8735 个四轴解，顺序为 `x_joint, y_joint, z_joint, yaw_joint`。
6. 点击 `Play IK result` 播放。现有播放时长输入控制预览速度；原始轨迹的时间戳保留，不被预览时长覆盖。也可以点选关节轨迹行，将机器人置于该点。
7. 保存项目后，关节值、解析逆解标记和 TCP 模式一并保存，重新加载后仍可按相同方式播放。

不需要手动选择新的算法下拉项。控制器识别实际 URDF 的三平移加一旋转关节链，为四轴配置独立解析求解器；六轴机器人的 Basic Planning、原有模型数值逆解、名义 ABB DH 逆解和多解流程保持原路径。

此次新增范围为四轴 Basic Planning 逆解和播放，未扩展六轴专用的 Multi IK、Top-K 或 CDF/APF 优化流程，也不会将三个直线轴当作角度自动填入 CDF 初始解表。

## 坐标与单位

- 铜板位置沿用网页示例：平移 `(0, 0.140, 0.375) m`，绕 Z 轴旋转 180°。
- `tongpian.txt` 已经是该摆放对应的世界坐标轨迹，导入后不要再对它重复施加一次工件变换。
- 本机器人 URDF 的 `tool0` 位于喷嘴出口。轨迹过程坐标系的 +Z 是喷射方向，对应喷枪本地 +Y；过程 +X 对应竖直扫描方向。该内置喷枪的固定标定旋转为：

```text
0 1 0
0 0 1
1 0 0
```

- `with tool` 使用该喷嘴位置与过程姿态；`flange` 使用旋转腕连接体，不附加上述喷嘴标定。
- 如果机器人挂有项目中明确保存的工具 TCP，则使用该工具及其固定变换。工具存在但 TCP 含糊时会报告错误，不会悄悄退回内置喷嘴。
- 关节数据采用运行时 SI 单位：前三轴为 m，第四轴为 rad。Basic Planning 中的原始关节数值遵循该单位。

## 解析逆解与兼容边界

零基座下，网页模型的公式是：

```text
R = atan2(-TCP_R(0,2), TCP_R(1,2))
X = TCP_x + 0.08 sin(R)
Y = TCP_y + 0.08 - 0.08 cos(R)
Z = TCP_z - 0.47
```

C++ 实现不将这些数值写死到 GUI 中。`motion_planning::XyzrKinematics` 读取实际机器人关节拓扑，以运行模型的正解识别三条平移轴、旋转轴、喷嘴旋转偏置和零位，建立解析模型，再用多个组合关节位姿与实际 URDF 正解交叉校验。基座平移、旋转和合法固定 TCP 变换因此能进入同一求解过程。

求解器验证目标完整姿态、实际关节限位和正解残差。四轴不具备的倾斜姿态会失败，不以近似方向代替。连续旋转轴选择最接近种子的圈数，前三个平移轴不做角度归一化。任一控制点失败时不提交部分关节路径。

播放通过既有文档和控制器流程驱动真实机器人关节。两控制点之间先插值 TCP 位置及连续旋转角，再解析逆解，避免直接关节线性插值造成喷嘴偏离原 TCP 直线。四轴播放复用项目明确配置的碰撞检测器，不自动套用六轴 CDF 示例工件。

## 原始限位和演示副本

`data/robots/xyzr_spray_robot_demo/urdf/xyzr_spray_robot_demo.urdf` 是与桌面网页相同思路的演示副本：直线轴位置范围为 ±1 m，旋转轴为 continuous，原速度及力矩参数保留。`xyzr_spray_robot_original_limits.urdf` 用于原始限位对照。用户提供的原始机器人包未修改。

原轨迹需要 X 约 ±183.49 mm、Y 约 0–160.83 mm、Z 约 −105–115 mm、R 约 ±75°。原始 URDF 的 Y 轴 ±60 mm 和旋转轴 ±60° 无法覆盖整条轨迹；加载原始限位模型时将如实拒绝越界点。只有加载演示副本才放宽这些位置限制。

网页示例曾为满足速度上限重排时序；本次 Basic Planning 兼容保持输入文件原始时间，并遵循现有预览时长机制，未加入新的速度、加速度规划。软件放宽位置限位不代表硬件行程增加。本功能不向实机发送指令；逆解和动画通过也不等于全机器人无碰撞认证。

## 代码位置与验证入口

- 算法：`SMRobotMotionPlanning/ProjectMotionPlanning/include/ProjectMotionPlanning/XyzrKinematics.h` 及 `src/XyzrKinematics.cpp`。
- 原逆解接口与实现 `TrajectoryInverseKinematics.h/.cpp` 保持本轮修改前的内容，避免改变旧调用方的参数、结构布局或求解流程。
- UI 适配：`SimWorkbench/SMRobotWorkbenchMotionPlanning/MotionPlanningWorkbench/Editor/MotionPlanningModuleController.*`。根据实际模型选择四轴或原有六轴逆解，复用现有保存、事件刷新和播放流程。
- 全量域回归：`ProjectMotionPlanningXyzr`。检查原轨迹 8735 点、26202 个段内位置、独立实际模型正解、与网页公式的一致性、保存重载、原限位、非法姿态、连续圈数、变换基座和 flange 模式。
- 实际 Qt 回归：`RobotQtViewerTrajectoryTransfer-xyzr`。通过 Basic Planning 的真实逆解入口运行 8735 点和动态播放，校验末点，同时执行原六轴实际模型逆解。

在已有 VS2019 构建目录中重新运行 CMake 后，构建 `RobotQtViewer` 即可纳入新增源文件，不需要手动向 `.vcxproj` 添加文件。Release 和 Debug 的回归目标分别运行相应配置。完整构建／测试日志和截图位于本工作区外层 `output/xyzr_basic_planning_20261105`。


## 发那科普通单解与全逆解的限位规则（2026-11-05）

按用户最新要求，Basic Planning 的六轴 `Solve IK and apply` 忽略关节位置限位，包括对整条输入轨迹逐点求单个解；正解残差检查和原数值逆解算法保留。`全逆解` 保留搜索范围与实际 URDF 限位的交集；GA/APF、碰撞检查与其他规划服务继续使用各自原有约束。原 URDF 无修改。XYZR 仍通过独立解析服务及加载的演示模型工作。

问题可复现于 `ABB4600-Aburnner.sys.json` 的实际 Fanuc M-710iC/50 和桌面 `freeform_trajectory_paq(3).txt`（2400 点）。加入位置限位的普通数值搜索会在部分可达分支卡住；另外 12 点在该工程保存的 TCP 与 URDF 原限位下没有合法解。独立球腕分支枚举并对解析 URDF 正解逐分支核对，确认原限位内为 2388 点，剩余点是 1630、1635、1654、1659、1678、1683、1702、1707、1726、1731、1750、1755。例如第 1630 点一条分支需 J3=-80.62844385°，低于模型下限 -80.5°。普通单解按用户要求忽略位置限位后可接受这些几何解；全逆解仍如实显示原限位内无解。

`ProjectMotionPlanningFanuc` 覆盖真实 Fanuc URDF、工程保存 TCP、普通单解（不限制位置）、原限位全逆解与不可达目标拒绝。`RobotQtViewerTrajectoryTransfer-fanuc` 检查实际 Basic Planning 控件、关节应用、全逆解界面及动画。完整用户输入另有独立诊断记录，不把单解成功解释成碰撞检查成功。

为改善 Debug 长轨迹动画，轨迹折线使用已有 `TrajectoryPass` 缓存完整顶点，只在轨迹或高亮范围变化时重建，避免每帧为每段分配 DebugDraw 线段；没有删除或降采样运动点。
