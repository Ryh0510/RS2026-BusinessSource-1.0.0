# 工具属性与 TCP

场景树对象、已安装设备、设备定义提供“设为工具／设为工件”。默认绑定产生工具；显式设为工件可表达被机器人夹持的工件。属性独立于安装关系、可见性和碰撞启用状态。

持久化使用已有字段：源对象 `objectType`；安装定义 `assetKind` 与 `assetType`。`SceneEntitySemantics` 统一计算有效角色；兼容历史 `assetKind=tool / assetType=workpiece` 的绑定枪。插件使用有效 `role` 或 SDK `workpieces()`，不按 STL 文件名推断。

已安装设备右键提供 TCP 对话框，FANUC（mm/WPR deg）、ABB（mm/四元数 wxyz）、通用（mm/RPY deg）均转换为同一刚体变换。所选参考连杆到资产安装原点的变换，由当前机器人 FK、mount 和安装偏移共同求得。输入 TCP 为参考连杆局部位姿，不是 WORLD 位姿。切换下拉选项保留 TCP。

最终写入资产 `functionalFrames` 中 `frameType=tcp, primary=true` 的 asset-local 变换，并通过现有 active attachment 接口使正运动学、喷枪预览采用该 TCP。隐藏工具只影响显示，不改变有效工具 TCP；workpiece/sensor 不能冒充喷枪 TCP。独立修改一个实例时复制共享资产，避免其他实例的 TCP 被连带修改。

界面表示偏好存入 `extensions` 的 `rws.tcp.<attachmentId>`。数值实际精度以内部米／弧度变换为准；厂商下拉选项不隐含负载、质量、惯量或控制器编号。

回归覆盖厂商变换等价性、非零安装偏移下的下拉切换、零四元数拒绝、属性变更、共享定义隔离、保存重开；Python 独立进程测试覆盖旧喷枪分类、工件筛选、机器人所有权、真实 FK 下 TCP 跟随及禁止移动隐藏源对象。
