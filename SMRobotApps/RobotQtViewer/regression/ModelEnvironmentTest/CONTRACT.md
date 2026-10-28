# 模型环境候选回归契约

- Owner：SMRobotApps/RobotViewerCore；Qt 描述适配、AssetCore 外链、RenderCore 材质/GPU 和 SceneCore 分类为受测边界。来源 ai，状态 candidate，尚无人审晋升。
- 前提：真实 OpenGL 4.2、Qt Widgets、data 子模块中的 Blender 精细工厂资源；不通过跳过 GL 来计成功。
- 不变量：米制七部件、完整场景 39 mesh 提交；剖切隐藏屋顶及朝向相机的墙面，只影响当前视图；嵌套完整视图退出后恢复；node visible 不变；缺失最后部件时旧场景完整保留。
- 材质：URI 空格正确解码，中文目录外部 bin/PNG 可读；Normal/MR 从 CPU 到真实 GPU；50 次加载/卸载后纹理 weak_ptr 失效、GL texture 删除、根节点清空；其他持有者仍有效。
- 输入与兼容：重复 ID、非有限尺度、非单位法线、分数版本、错误字段类型、非米单位拒绝；旧五预设与模型在 viewport 中互切；项目重建重放选择。
- 限制：实体列表断言不能替代完整 dirty/undo/collision/picking 端到端测试；像素测试测量生产 shader 的 MR/Normal helpers，不承诺完整物理光照准确性；挂载相机使用真实渲染路径及合成 RuntimeMountedCamera，不覆盖设备流和绑定编辑器。GLFW UI、SDK ABI 和异机驱动另行验收。视觉和运行部署证据见执行记录。
- 红绿方法：受控将 Environment tag 改成 Robot（或跳过 Normal）应触发分类/slot 断言失败，恢复产品源码后重跑；最终证据记录于计划，不把故障注入开关留在产品。

- 已补：生产 fragment 保留 helpers、仅替换 main 输出，GPU 采样 B/G × factor，Normal 导数 TBN，移除贴图后 flags 复位，旧独立灰度 R 通道；米制 16m 地坪/Z-up；内嵌图、透明材质、空 glTF 拒绝并保留旧根。
- 已补：真实 renderMountedCamera 在 includeGrid=false 下提交 39 mesh 并读取室内背景像素；初始化前 pending 不发 committed，实际初始化后才提交；controller 成功/失败偏好、旧偏好、中文/英文、取消文件选择、所有操作唯一终态和重启恢复。
- 红绿证据：把生产 shader 的 packed metallic B 改为 R，Release 测试退出 1，`real shader samples B metallic and G roughness with factors` 断言失败；finally 恢复原字节后退出 0。仍为 ai/candidate，未自行晋升。

- 可选截图诊断：在标准 descriptor/runtime 两参数后另传 project/output，会加载真实项目，通过现有鼠标和滚轮事件输出一周 49 帧；不增加 CTest 个案，不替代标准断言分支。四个斜角的截图保存在计划视觉证据中。
