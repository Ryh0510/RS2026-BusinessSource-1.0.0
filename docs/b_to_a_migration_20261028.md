# B 功能更新到 A：迁移与基本功能测试

日期：2026-10-28。A 为 `RS2026-BusinessSource-1.0.0`，B 为 `RSW/RSW`，均位于本工作区。B 作为读取来源，所有业务修改落在 A。没有提交或推送 Git。

## 按用户要求顺序完成的内容

1. **曲面规划依赖、虚拟挡板、标定领域模块**：引入 SprayFreeformTrajPlan（切片与 UV/OptCuts）、CalibrationInstructionTranslation，接入依赖兼容脚本和虚拟挡板补充代码。保留 A 的本地工具链配置、预编译 SDK 和许可证。
2. **默认 FANUC 场景**：沿用文件名 `config/projects/ABB4600-Aburnner.sys.json`，实际机器人为 `m710ic_50_drake`。保留 B 的挂载、喷枪、TCP、用户坐标系和虚拟挡板参数，删除初始规划和轨迹扩展。机器人网格、工件及喷枪随 RuntimeConfig 目标部署，不依赖原桌面绝对路径，也不要求重新链接才能刷新配置。
3. **工作台与轨迹传递**：注册曲面轨迹、标定转换工作台，扩展涂层分析与 PlanningDataSource/PlanningTrajectoryTransfer，保持场景/世界坐标转换、工艺开关、速度、加速度和点类型。保留 A 独有的 16 个碰撞工作台文件，并合并 B 对最近距离结果显示和显式碰撞配对的 7 个文件改进。
4. **规划、IK 和后处理**：引入 B 的独立 IK、PostModel 多品牌后处理、外部工件规划、虚拟挡板、GPU 搜索与之字形分段。APF/CDF 的界面、领域接口和调用链采用 B。A 原有直接 Cartesian RAPID/FANUC 导出与 FANUC WPR 连续性修复保留。
5. **算法与轨迹属性**：引入曲面切片、UV 映射/轨迹、标定转换、OpenGL GPU 膜厚预测和论文算法复现；轨迹数据包含速度、加速度、喷涂开关及点类型。部署 NVRTC 12.8 到程序的 `cuda` 目录，搜索/IK 不需要安装 NVCC。

## 测试发现与处理

- 保留 A 导出按钮时的两处信号连接闭合错误：已修复，主程序与导出回归通过。
- 部分新测试的中间目录超过 Windows MAX_PATH：改为短测试构建目录。
- Debug 外部规划回归的 Eigen 模板对象节数超限：仅该目标添加 `/bigobj`。
- B 新增最近距离界面与 A 保留的碰撞工作台之间存在接口缺口：合并 B 的结果标签、运行时刷新和 SceneAll→显式配对处理，同时保留 A 独有实现。
- 场景树菜单的旧断言将所有动作都视作碰撞动作，与 B 的显示/隐藏功能冲突：保留显示/隐藏，并明确检查挂载坐标系不能配置碰撞模型或加入检测集合。
- ABB 专用回归此前直接读取默认场景，切换 FANUC 后失败：为这些测试保存独立 ABB 场景，默认用户场景继续使用 FANUC。
- 轨迹分段在快速原子替换 manifest 时可重复收到 Windows 错误 5，原测试未捕获异常而退出：对访问/共享/锁冲突做最多约 190 ms 的有限重试，仍失败时保留原文件并给出错误码与路径。修复后连续 5 次完整分段及取消保留测试通过。
- Debug 的 GA 次数预算测试被 15 秒时限提前打断，密集 CDF 校验超过 240 秒：次数预算用例与已有超时失败用例分开，重计算回归单独延长时限；不修改生产默认参数或安全门限。
- B 带有未参与构建的旧 Qt UI/旧优化器文件，会违反 A 的领域层边界：6 份未迁移的历史文件保存在 `excluded_b_legacy/`，活动算法保持领域层检查。另有 10 个被 B 接口替代的 A 私有旧文件退休，原件在 `before/`。

## 验证结果

- CMake 配置成功；Release/Debug 均完成主程序及所有配置目标构建。
- Release 全量 68/68 项回归通过。后续仅调整测试预算后，外部规划回归再次通过。
- Debug 68/68 项回归最终通过（常规 66 项 + 两项独立复测）。密集 CDF 校验用时 268.75 秒，完整 GA 外部规划用时约 355 秒；保留首轮失败日志与最终复测日志，汇总见 `test_results_summary.json`。
- 真实 FANUC 无参数默认启动、显式加载、保存、重新打开通过；Release/Debug 主程序退出码均为 0。保存重开后挂载/TCP/虚拟挡板与 B 一致，无轨迹扩展。截图 `fanuc_startup.png`。
- RTX 5050 GPU 搜索：13,266 个 CPU/GPU 几何对照，最大误差 `2.220446049250313e-16`；96 个可达目标 IK，通过独立 CPU FK 复验。Release/Debug 均通过。
- GPU 膜厚：完整与空间过滤结果最大差异 0，中心厚度约 `0.000148814 m`；有限性、对称性、曝光时间缩放通过。
- 标定：倾斜圆柱、六点平面圆、工件坐标系、刚体变换往返和退化输入检查通过。
- UV：真实网格展开、生成 238 个轨迹点、PAQ 导出与清理后的状态失效检查通过。
- 工作台轨迹传递、CDF 接收、喷涂测量、碰撞模型工作流、主题/高 DPI、后处理、切片与轨迹导出等均在回归覆盖内。
- 领域层边界检查通过；业务源码无遗留冲突标记，C/C++/CMake/Qt 源码保持 CRLF；根仓与两个子仓 `git diff --check` 通过。

## 保留、备份及范围说明

- `before/` 保存覆盖或退休前的 A 文件；根仓和两个子仓的原始工作区/暂存区补丁另存本目录。未重置已有工作区。
- `final_change_checklist.md` 按相对路径排序，`final_change_manifest.csv` 包含全部文件状态及 SHA256，`final_change_summary.json` 记录数量及保留核对。
- 原 1,875 份共有预编译 SDK 文件与两份许可证均保持原始 SHA256。A 独有碰撞实现名单见 `preserved_a_collision_only_files.json`。
- 新增文件中多数属于 B 自带的 OptCuts/libigl/CGAL 等第三方源码；没有替换 SDK 二进制。新增 NVRTC 二进制及许可放在工作区外层 `prebuild/cuda-nvrtc`，由缓存变量 `SMROBOT_CUDA_RUNTIME_DIR` 指定。
- UV 网格投影当前采用 CPU 路径，未启用需要 NVCC 的可选 `.cu` 实现；GPU 搜索/IK 和 OpenGL 膜厚已真实执行验证。
- 当前工件的虚拟挡板拟合最大残差约 4.65 mm，小于 B 的 5 mm 容差，但大于 1 mm 板厚；保留其原始诊断与配置，没有把拟合误差视为已消除。

## 运行位置

- Release：`C:/b/rs-bmerge-20261028/Release/bin/RobotQtViewerrx64.exe`
- Debug：`C:/b/rs-bmerge-20261028/Debug/bin/RobotQtViewerdx64.exe`
- 程序哈希：`verified_executables.json`。
- 源码中的本报告：`docs/b_to_a_migration_20261028.md`。
