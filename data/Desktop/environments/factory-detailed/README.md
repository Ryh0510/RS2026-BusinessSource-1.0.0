# 精细工厂模型环境

Blender 5.1 制作的原创工业背景，采用米制、Z-up；厂房 16 × 16 m，中部 6 × 6 m 保持净空，地坪顶面 Z=0。包含控制柜、输送机、货架、工作台、护柱、管线、四面实体墙及屋顶。窗采用不透明材质，避免依赖尚未完整支持的透明 glTF 渲染。

- `source/factory-detailed.blend`：可编辑源，七个独立部件，贴图打包在源文件。
- `runtime/`：GLTF_SEPARATE 导出的 `.gltf`、`.bin`、PNG；必须完整保留相对关系及文件名。
- `export-report.json`：Blender 版本、几何统计、字节数、源/导出文件 SHA256。模型 50,192 三角形、39 材质绘制项，runtime 2,909,199 bytes。
- 主仓 `config/environments/factory-detailed.environment.json`：部件角色和剖切平面；环境加载不创建仿真/碰撞实体。

在主仓根目录重新生成（需要已安装 Blender）：

```powershell
& '<Blender executable>' --background --python scripts/blender/build_factory_environment.py -- --output data/environments/factory-detailed
```

程序运行无需 Blender。发行只需 runtime、此说明、LICENSE.txt 和报告，不部署 source。贴图为原创生成的 BaseColor、Normal 和 packed MR；MR 的 G=roughness、B=metallic。Blender glTF 自动转换为 Y-up，由 AssetCore 恢复到 Z-up；不能再人为旋转或按机器人尺寸缩放。

观察剖切仅对主视口生效：屋顶隐藏，面对外部观察者的墙面隐藏；关闭选项显示完整封闭厂房。挂载相机始终观察完整模型。自选单个 `.gltf` 不推断墙面或地面；需要这些规则时使用描述文件。
