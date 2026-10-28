# InterprocessCommunication

RWS 进程内的桥接代码。`RwsPythonBridge.h` 仅声明接口，`RwsPythonBridge.cpp` 实现 JSON 文件请求处理，在 GUI 主线程调用 RobotViewport/ProjectScene。支持机器人枚举及关节读写、场景工件世界位姿读写、检测对配置、全局检测开关、检测器和距离开关、碰撞及最近距离查询。

由 `SMRobotApps/RobotQtViewer/main.cpp` 引入，通过 `--rws-sdk-dir <通信目录>` 启用。本目录 CMake 定义 `RobotQtViewer::InterprocessCommunication` INTERFACE target，传递头文件搜索路径和 `.cpp` 源文件；该 `.cpp` 在 RobotQtViewer 目标中单独编译一次。这样避免桥接库与 RobotViewerCore 的依赖环，不生成独立二进制库。使用者引用 `<InterprocessCommunication/RwsPythonBridge.h>`。

Python 客户端和控制界面位于 外层 `Agent/rwssdk/`。C++ 桥接代码不再依赖该外部目录中的头文件。

通信提供两种通道：原 JSON 文件轮询（20 ms）及 localhost TCP 持久连接。TCP 使用 Qt5::Network 的 readyRead 事件，在 server.json 发布动态端口；业务消息无需文件读写，使用换行分隔 JSON、会话和到期时间校验。不接受流水线请求，每连接一次只发一个请求并等待响应，避免积压控制命令。Qt 主线程繁忙时仍会延迟。Python FastClient 支持 20/10 ms 目标周期发送和超期跳槽；不承诺硬实时。

工件接口操作当前已加载的 SceneObject（包含 STL），不是导入新 STL。世界位姿采用 x/y/z 和 roll/pitch/yaw，旋转为弧度，旋转组合沿用场景 Z-Y-X 约定，长度沿用场景模型单位。角度读回可能为等价欧拉角表示。

规划接口 `planning_submit / planning_status / planning_result / planning_cancel / planning_forget` 可处理外部 STL 和轨迹。提交时在 GUI 线程捕获当前运行场景，包括未保存关节、机器人基座、工件位姿与碰撞配置；后台线程只持有独立文档和规划场景，不访问 viewport。可用内嵌 `planning_request` 或绝对 `request_file`，大文件后台读取，最多 16 MiB。

每次允许一个活动任务，最多保留 8 个任务记录；完成后可释放。提交返回快照 ID 和机器人/工件摘要，结果不会自动应用或保存；后续 GUI 修改也不会更新已提交的任务。取消是协作式停止，可通过状态确认完成。详细请求格式、UF2 标定和约束搜索见 `SMRobotMotionPlanning/EXTERNAL_PLANNING.md`。Python 的 `Client`、`FastClient` 都支持这些方法。

检测器采用 BetweenSets，端点支持 robot_id + link_name 或 object_id；配置时启用所选机器人和工件的碰撞标志，不自动生成缺失几何。全局关闭时尚未构建几何，配置结果中的检测器有效性可能待开启后重新求值；开启后查询并检查 valid、effective_pair_count、nearest.state。碰撞状态可抑制最近点，穿透见 contacts 的 penetration_depth。

所有变更针对当前运行场景，不自动保存磁盘项目，也不承诺同步主窗口文档编辑器/撤销栈。重新加载项目可能覆盖桥接变更。配置失败可能已更新部分运行时配置，应读取状态核验后再重试。

此迁移版将 Qt 桥接放入应用层 Integration，避免算法层依赖 Qt 与界面类。设置 RWS_SDK_TRACE=1 可记录请求类型与耗时，不输出会话令牌或请求内容。任务完成状态等待工作线程退出后才发布。
