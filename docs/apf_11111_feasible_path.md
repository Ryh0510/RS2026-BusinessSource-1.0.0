# 11111 实际可行几何轨迹（100 mm）

输入：C:/Users/14390/Desktop/11111.txt，749个原始Cartesian控制点。工程：ABB4600-burnner.sys.json，当前实际世界TCP、原关节符号、当前CDF检测器。

## 使用

- final_collision_free.txt：最终4528点关节轨迹。在Motion Planning → CDF导入后，可应用/动态运行关节角；不必再次执行QP。
- apf_collision_free.txt：QP前APF结果，4528点；用于单独观察避障效果。该阶段保留源时间，含重复时间戳，只用于几何预览。
- initial_continuous_ik.txt：749点、从原始Cartesian完整位姿直接连续求解的B1种子，保留当前模型连续轴的合法圈数。导入CDF后设TCP走廊100 mm、默认一轮QP，可复算本次流程。这个初始解本身有碰撞，不能当成避障结果。
- final_validation.json / apf_validation.json：对实际导出TXT重新导入、独立碰撞场景、原始关节线性插值的复验结果。
- tcp_comparison.png / quality.png：末端轨迹和质量对比；quality.csv为数值报告。

TXT的列为time_s、J1_deg至J6_deg，沿用存储IK角度；原有J1/J4/J5/J6符号映射由应用程序执行，不在文件中重复取反。

## 可行性证据

- APF：130568个最大单轴0.001 rad的原始关节线性插值采样，0碰撞/0限位违规，最大有序TCP位置偏差98.10077549 mm。
- 最终：60941个同样标准的独立采样，0碰撞/0限位违规，最大有序TCP位置偏差99.99986064 mm。
- 起终完整位姿相对于原始11111：最大位置误差0.000691112 mm、最大姿态误差0.0000303233度。
- 峰值关节速度23.190707度/秒、峰值离散加速度119.759975度/秒²，时间/速度/离散加速度检查通过。
- 最终记录执行时长5488.708056秒，为保守重新定时结果；GUI预览速度独立于该时长。不能把约864.230秒计算耗时当成执行时长。
- 仅一轮QP（外层QP轮数=1；数值迭代和线搜索不是重复完整轨迹优化）。

## 保留限制

本次满足当前CDF机器人与burnner检测配置下的无碰撞采样验收、100 mm有序位置走廊和起终完整位姿约束。中间点没有完整姿态等式约束，最大姿态相对输入偏移约40.401度。没有证明全局最优或严格连续碰撞安全。

默认10 mm安全余量未达到：最终最小节点Phi=-9.7453744 mm，对应节点最小物理间距约0.2546256 mm。因此原服务仍报告success=0（安全余量未达标），同时已输出经过碰撞/走廊验收的_partial几何轨迹。不得将本结果描述为达到10 mm安全余量或实机执行认证。没有减少安全余量参数以改变成功标志。

二阶弯曲代价由APF的3.0091349降至最终0.50414245（约83.25%）；最大关节折角仍约159.71度，不能描述为完全光滑。离散加速度通过不等于分段线性曲线具有连续物理加速度。

## 本次选种变化

原Top-1起点为B1，终点为B2。本次从相同原始Cartesian轨迹直接进行实际模型连续逆解，使用B1起点、B1终点，J6保留绕行圈数（终点约326.267度，而非强制归回±180度）。这满足用户允许使用其他有效逆解构型的授权，解决了此前固定回接B2腕部锚点造成的失败。

本次提供可直接导入的结果及复算种子。Basic/Top-K当前选中项的自动规划逻辑并未增加自动替换：复算需要导入上述连续逆解种子。全部原有单逆解/多逆解/Top-K/播放功能保留。

## 复现和实现边界

源变更：SMRobotApps/RobotQtViewer/regression/SprayMeasurementSmokeTest/main.cpp及CMakeLists.txt；诊断功能不进入Widget或修改领域规划语义。候选直接导入Cartesian targets，不由当前关节轨迹先做FK再求全逆解。试验候选文件不是无碰撞结果，必须继续APF及完整验收。

构建目标：RobotQtViewer、RobotQtViewer-SprayMeasurementSmoke、ProjectMotionPlanning-ApfTests、ProjectMotionPlanning-TrajectoryQualityTests、ProjectMotionPlanning-Headless，C:/b/rs105-merge，Release/Debug。

回归CLI（参数均绝对路径）：

```text
RobotQtViewer-SprayMeasurementSmokerx64.exe --apf-candidates <project> <11111.txt> <candidate-folder>
RobotQtViewer-SprayMeasurementSmokerx64.exe --cdf-shape <project> <fullpose-B1.txt> <result-folder> <11111.txt>
RobotQtViewer-SprayMeasurementSmokerx64.exe --validate-cdf-result <project> <11111.txt> <fullpose-B1.txt> <stage-2.txt> <report.json>
RobotQtViewer-SprayMeasurementSmokerx64.exe --validate-cdf-result <project> <11111.txt> <fullpose-B1.txt> <stage-1.txt> <report.json> --geometry-only
```

--cdf-alternative仅用于允许中间姿态改变的试验种子检查；交付的fullpose-B1.txt初始种子已对全部749点位置和姿态验证，实际APF/QP仅约束中间位置与起终关节构型。

证据日志：外层build/apf-alternative-B1-gui.log、apf-alternative-B1-final-validation.log、apf-alternative-B1-apf-validation-format-final.log；性能C:/b/rs105-merge/Release/bin/log/cdf/cdf_20261004_155657_525_22360.log。单轮QP=1但OSQP内部迭代及线搜索可多次，不重复整轮几何优化。

输出最终TXT SHA256：0BA9B4A813FCE8248B7E8433600B47578630D8C6C5DA4AAAEBA8F7E1008E9420。

候选调查：仅投影TCP位置的端点插值种子存在大量碰撞，未采用；平行传输姿态试验产生另一些种子但未作为交付路径。原始完整位姿B1连续种子已得到本次完整结果，因此不扩大偏移、不恢复OMPL，也不继续同一固定B2终锚点失败搜索。

最终验证：Release/Debug主程序和相关目标构建成功，各10/10相关CTest通过；Release /bigobj后4项实际UI回归复验通过。主程序两配置--smoke-exit-ms1500退出0。日志外层build/apf-feasible-delivery-*。
