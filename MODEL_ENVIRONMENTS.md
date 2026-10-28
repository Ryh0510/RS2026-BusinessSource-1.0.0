# RobotQtViewer 模型环境

加载项目后，在原有环境菜单选择“精细工厂（Blender）”。旧 Studio、Factory、Workshop、Home、Grid Only 均保留。未加载项目、视口场景尚未初始化时，模型加载项暂不可用。

工厂采用固定米制，包含地坪、四面墙、屋顶和内部设施。默认开启“观察剖切”：主视口隐藏屋顶，并在相机移到墙外时隐藏面对相机的墙面；切换选项可显示完整封闭厂房。挂载相机始终渲染完整背景。实体设施仍有真实遮挡，从厂房底部观察时会看到地坪底面。

“加载环境模型…”接受外链贴图 `.gltf` 或 `.environment.json`。单个 glTF 默认作为完整模型，不推断地面、墙面或屋顶。模型必须为不透明材质；首版不接受内嵌图片，Blender 请导出 **glTF Separate**，并一起保留 `.gltf`、`.bin` 和 PNG。解析或上传失败会保留原环境，并在操作状态中显示原因；取消文件选择不改变环境。加载成功后保存本机偏好，重启或重新加载项目会恢复；环境不写入仿真项目。

需要地面与观察剖切时，使用描述文件。完整示例见 `config/environments/factory-detailed.environment.json`：

```json
{
  "schema": "smrobot.presentation-environment",
  "version": 1,
  "units": "meters",
  "scale": 1,
  "hasGround": true,
  "groundHeight": 0,
  "showGrid": false,
  "parts": [
    {"id": "floor", "source": "runtime/floor.gltf", "role": "floor"},
    {"id": "interior", "source": "runtime/interior.gltf", "role": "solid"},
    {"id": "north-wall", "source": "runtime/wall-n.gltf", "role": "wall", "normal": [0, 1, 0], "offset": 7.9},
    {"id": "roof", "source": "runtime/roof.gltf", "role": "roof"}
  ]
}
```

`source` 为描述文件相对路径、绝对本地路径，或已注册 data 根下的 `data://...`。ID 必须唯一；scale 为有限正数（最大 1000）；wall 使用单位外法线，平面满足 `dot(position, normal)=offset`，offset 随模型 scale 缩放。`groundHeight` 是世界坐标中的地面高度，应与缩放后的实际地坪顶面一致。环境固定为视觉背景，不注册项目实体、碰撞体或选择目标，也不参与 Home/聚焦 bounds。

源文件位于 data 子模块的 `environments/factory-detailed/source/factory-detailed.blend`。生成工具为主仓 `scripts/blender/build_factory_environment.py`，实际制作/导出需要 Blender，程序运行不需要。资产 README、许可说明和导出报告与 runtime 同目录交付；生成统计为 50,192 三角形、39 绘制项，runtime 约 2.78 MiB。

构建后的可执行目录自动包含 config 和此环境的 runtime、说明及报告。迁移时保留这些目录以及应用所需 DLL/Qt 插件；不需要复制 `.blend`。可用 `SMROBOT_DATA_ROOT` 指向外部 data 目录，或 `SMROBOT_CONFIG_ROOT` 指向外部 config 目录。机器人等其他项目资产仍按原资产部署约定提供。

可复现截图命令（在仓库根目录运行，路径可换成部署目录）：

```powershell
$root = (Get-Location).Path
& "$root/build/env_model/Release/bin/RobotQtViewerrx64.exe" `
  --profile-project "$root/config/projects/panda.sys.json" `
  --profile-environment factory-detailed `
  --profile-camera isometric --profile-zoom-out-steps 70 `
  --profile-screenshot "$root/build/environment-evidence/factory.png" `
  --profile-exit-ms 1600
```

`--profile-environment` 同样接受旧 ID、glTF 或描述文件路径；`--profile-camera` 接受 home/isometric/front/back/left/right/top/bottom；`--profile-zoom-out-steps` 为 0–200；加 `--profile-complete-environment` 显示完整结构。profile 不保存环境偏好，失败退出非零。截图是实际 Viewer 输出，不是 Blender 预渲染。

此改造没有新增第三方依赖或更改 project schema。新增 presentation 描述/API 和贴图枚举末尾值；`RobotViewport` 静态库消费者应重新编译。RenderCore 仍使用历史自动导出策略，weak texture cache 引起模板导出快照差异；SDK 源码消费者验证不等于旧二进制 ABI 承诺。具体构建、回归和 ABI 差异证据见 `plans/robot_qt_viewer_model_environment_execution.md`。
