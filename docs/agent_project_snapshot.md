## 2026-10-04 Top-1 平滑避障最终交付（以此段为准）

- 本轮已完成：ProjectMotionPlanning原始Cartesian连续IK可选种子、多尺度128→2节点平滑、实际FK边界投影、局部角度恶化保护、真实碰撞/有序100mm走廊门控、单轮QP、最终0.00025rad发布门槛。GUI新增允许等价端点构型勾选项；两端完整TCP位姿保持，中间姿态不锁。
- 最终交付外层results/11111_Top1_Smooth_100mm，源数据build/apf-smooth-top1-delivery。原Top-1的B1→B2切换改为授权的B1→B1连续实际IK，保留J6圈数。11个本轮源文件，旧调用兼容，无新依赖/SDK/项目格式修改；保留所有此前未提交成果。
- 最新独立验收使用导出TXT原始关节线性插值0.00025rad：APF162098次、最终153115次，均0碰撞/0限位；最终最大有序TCP偏移99.20085362mm，端点位置0.000694158mm、姿态0.0000302533度。最终时间、离散速度/加速度检查通过。
- 旧→新：关节长度78.183→54.994rad；TCP超过45度折角1026→65处，95%分位130.523→9.874度；最大关节折角159.713→107.002度。不是处处光滑，仍有局部折角；TCP偏移RMS58.870→66.239mm，平滑与贴近原参考存在取舍。
- 重要：build/apf-smooth-top1-run1虽然几何更平滑且0.001rad通过，但0.00025rad检出1个碰撞段，已排除，禁止重新作为无碰撞结果交付。最终安全候选两阶段在原始顺序下均通过更密验收，未通过改阈值或仅查节点制造成功。
- 最小节点间距0.253040mm，10mm安全余量仍未达到，partial/successIncludingMargin=0保留；当前仿真有限采样验收不等于严格连续碰撞证明或连续动态认证。生成1435.74秒，一轮主QP；保守执行时间5111.849秒，不宣称提速/全局或时间最优。
- 最终Release/Debug构建及各10/10相关回归通过，两配置主程序启动退出0；UTF-8/CRLF和根/两个子仓diff检查通过。日志apf-smooth-dense-gate-*及apf-smooth-{apf,final}-dense-validation.log。Release EXE：C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 D0E24A2230EB341B01D2D5C0C074C10888548F930707A5AD30990F680B8D6B02。运行中的用户程序未终止；仅停止过明确属于本轮的过渡试验进程。
- 实现及结果说明docs/apf_smooth_top1.md与交付README.md；最终验收JSON/CSV/PNG/校验和在交付目录。

## 2026-10-04 APF 回折与平滑再修复（进行中）

- 用户要求实际交付接近原之字形的无碰撞平滑轨迹。基线虽通过采样碰撞检查，最大折角159.7度，尚不能满足本轮目标；不将旧产物冒充本轮修复。
- main及根/规划/Workbench子仓全部已有变更保留。算法所有者ProjectMotionPlanning；本轮先改私有PathRefinement及CDF调用，真实回归增加已有路径检查点精化入口，必要时接入合法连续IK种子，GUI只负责输入与显示。
- 计划：按原始有序参考进度进行多尺度平滑，去除受APF弧长及0.06rad盒约束保留的回折；保留100mm硬走廊、完整起终TCP位姿、多圈、实际碰撞配置、0.001rad最终独立复验及单轮QP。先检查点对比，再完整实际流程和Release/Debug验证；预计不超过10源文件，无新依赖。
- 完成验收：可导出的新轨迹、完整采样零碰撞/限位、100mm、起终位姿及明显改善的几何折角/弯曲指标。10mm余量、离散动态限制和连续执行保证分开报告。

## 2026-10-04 已找到11111的100mm可行几何路径（最新）

- 用户最新授权保持原始末端起终完整位姿、允许其他有效逆解构型。已直接从11111原始Cartesian目标求连续实际模型逆解，B1起点→B1终点、保留J6圈数；原Top-1为B1→B2。本实例已完整通过APF和单轮QP后的碰撞/走廊验收，不再是仅有失败诊断。
- 当前URDF六轴均为continuous。有限±180度多逆解搜索窗口不是该模型机械限位；读取limit标签±3不能替代continuous语义。只对模型连续轴做连续提升，有限轴保持真实范围。运行符号J1/J4/J5/J6不改。
- 交付外层results/11111_APF_100mm：initial_continuous_ik.txt、apf_collision_free.txt、final_collision_free.txt、原始Cartesian参考与实际FK比较图、质量和独立验收JSON/CSV/校验和。可导入CDF播放；复算用749点新初始解，100mm、一轮QP。没有在原Top-K按钮后添加自动换种流程，原UI选中结果语义保留。
- 真实GUI完整流程864.230秒，输出4528点；APF和最终碰撞段0。最终对导出TXT独立原始关节线性插值复验60941采样（0.001rad）、APF130568采样，全部0碰撞/0限位。最大有序TCP偏差分别99.99986064/98.10077549mm，起终最大位置0.000691112mm、姿态0.0000303233度，最终时间/速度/离散加速度通过。
- 重要限制：默认10mm安全余量没有达到（最小节点间距0.254626mm）；原repair.success=0且发布_partial几何轨迹，不能报告10mm安全达标或完整执行级认证。中间完整姿态未锁，约40.4度最大偏移；最大关节折角约159.7度，未宣称完全平滑或全局最优。仅密集采样验收，非严格连续碰撞证明。
- 本轮实际源变更仅真实回归main.cpp（候选生成/独立导出复验入口）及该回归CMake（MSVC /bigobj解决新增Eigen诊断的Debug C1128）；领域/UI前轮改动全部保留，无新公共API/依赖/SDK修改。试验性位置投影种子无效，辅助函数已移除。Release/Debug构建及各10/10相关CTest通过，两配置主程序启动退出0，Release新增/bigobj后4项实际UI回归复验通过；最终证据见docs/apf_11111_feasible_path.md。

## 2026-10-04 放开起终构型并求解当前可行路径

- 最新用户授权：保持原始11111的末端起终完整位姿，允许其他有效逆解构型。此前“不更换Top-1起终构型/等待授权”已失效；100mm有序位置走廊和真实0.001rad碰撞验收不变。
- 保留根及规划/Workbench子仓全部未提交变更。先在实际FK、实际有限关节范围下比较连续构型种子和起终逆解；领域拥有构型替换、APF和验收，GUI只传递原始Cartesian/已验证IK快照，不改符号/SDK/依赖。默认一轮QP。
- 实施范围预计不超过8个本轮源文件：私有APF连续参考、CDF候选输入及选择、Workbench接入、真实回归。先独立候选筛查避免重复15分钟固定分支失败；成功标准为正式路径非空、起终完整位姿匹配、100mm有序走廊和全程碰撞/限位复验、最终QP复验。不能将预筛或失败拒绝当成完成。

## 2026-10-04 继续求解实际 Top-1 APF（用户要求完整通过）

- 保留全部未提交成果和100mm上限，不更换初始Top-1、不减弱碰撞或TCP验收。此前仅得到失败诊断，不能算用户目标完成。
- 根因待复现：逐站贪心只在原直连失败后启动势场、不能退回先前站点提前绕行；有限关节盒和最终固定锚点也可能导致人为不可达。先增加独立实际FK/碰撞窗口复现及失败位置观测，避免每次重复整条扫描。
- ProjectMotionPlanning拥有改进后的APF前瞻/回退/逃逸及路径验收；Viewer regression仅用于实际模型证据，不将算法置入Widget。必要时增加按参考序号的势场路径变形，所有接受路径保留原始对应、100mm及实际0.001rad复验。默认一轮QP不变。
- 完成条件：当前11111对应749点Top-1完整APF无碰撞、100mm高密度TCP验收，后续QP最终路径复验和Release/Debug相关回归；不能把增大迭代次数/返回成功标志替代真实通过。若存在真实不可满足条件，用具体几何/限位证据说明，不作无依据的普适APF保证。预计不超过8个本轮源文件，无依赖/SDK修改。

- 本轮已实施：实际 FK 位置投影参考；120mm 前方距离势场（当前位置20mm内优先当前梯度）；早期关节/腕部偏移、位置零空间关节吸引；局部回退分配曲线至之前站点并逐段验收；60/120/240mm锚点扩展，最后使用实际有限关节范围；终点500次积分/关节差停滞指标、走廊投影和间距恢复。无SDK/依赖/public API修改。探索性的穿透弹性带未纳入最终实现。
- 本轮窗口基线 field11 可走至右锚点附近但腕部属另一个构型；增加提前避障后的若干独立窗口尝试仍失败，不能报告修复完成。最新全流程测试会话67345，绝对路径输出外层build/apf-anticipatory-real-results-absolute；性能日志 C:/b/rs105-merge/Release/bin/log/cdf/cdf_20261004_124511_927_38184.log。曾因开发工具传入相对导入路径停在对话框，已仅结束自有测试并用绝对路径重启。
- Release/Debug最新主程序、APF/质量/Headless/Smoke已构建，两配置主程序--smoke-exit-ms1500退出0。新增提前偏移和非线性腕部投影回归。最新全套测试日志apf-anticipatory-ctest-{release,debug}-delivery.log。完整Top-1仍待真实验收；不要将失败诊断或GUI回归退出0记为完整成功。

- 全流程已结束：906.6s，在当前1648→1657修复区间失败；初始1602碰撞段、部分修复后1140，QP=0/正式输出=0。新实现未解决用户要求，且本次失败比此前322.4s更慢，禁止称完整通过/提速。失败诊断仅用于调试。两配置最新各10/10 CTest通过，SHA256 5D28D1A8BAC524545A7105CBEC5B5D2AF2FC5D2CDF01C59F8442F65BE60BD39E。
- 已用request_user_input_async询问：固定当前Top-1起终关节角，还是保持相同TCP起终位姿、允许其他逆解构型。答复前不改变选定初始解。下一步重点是构型约束/多候选，而不是把更长APF搜索伪称保证成功。

## 2026-10-04 APF 保持之字形与 TCP 走廊（最新默认 100 mm）

- 保留上一轮未提交的阶段诊断/二阶QP/定时修改。当前APF只朝区间终点吸引、原路径吸引很弱；全局捷径/弧长重采样可跨越扫描线并改变点对应。关节直线插值的TCP本身也可能产生弯弧，需使用实际TCP核对。
- 用户先指定50mm，后明确改为100mm；最新默认100mm。ProjectMotionPlanning拥有按顺序的参考路径引导、实际FK位置走廊、局部场/步长/振荡控制和完整运动复验；Workbench仅提供参数和实际FK快照，不把算法放进UI。
- 实施：先用有序参考目标替换仅终点吸引，避免任意跨段捷径；保留对应节点/转角和多圈。实际Cartesian参考由原始关节控制点TCP位置之间的折线定义，加密节点继承原始段/比例，不从加密关节插值TCP拟合参考。所设100mm是硬上限，无法连接则明确失败，不扩大走廊或发布越界结果。
- 默认一轮QP、既有碰撞模型/0.001rad验收/符号/播放保持。后续平滑/QP候选也需保留走廊，避免APF合格后又被QP拉离参考。
- 验证：有序之字形/折返/碰撞绕行/硬上限/连续段中点超界/turn与确定性回归，真实11111 Top1的APF与最终碰撞/所设TCP偏移验收，Release/Debug构建和相关测试。预期不超过15个本轮源文件，无新依赖；若所设走廊内无法找到连接，报告失败及区间而不是承诺任意初始构型都可绕开。

- 本轮实现：按有序站点推进、实际 TCP 位置 Jacobian 引导、转角前停止前瞻、参考平面截断、偏移衰减；删除整段捷径/弧长重采样。所有 APF/QP/平滑/最终候选按原始 TCP 折线及对应编号复验所设上限，CDF 增加参数/绿色参考/失败阶段诊断标识。
- 本轮验证：Release/Debug 构建、各 10/10 相关 CTest 与主程序启动通过。真实 Top-1 关节插值本身最大位置偏差 942.902 mm；首次50mm验证的新 APF 在 1360→1403 加密节点（原始控制点约222→229）未找到50mm连接，正确拒绝、不运行QP、不发布优化结果。APF失败不证明不可行，需换起点构型或继续研究受限避障。详见 docs/cdf_apf_shape_corridor.md；不能用上一轮无走廊结果证明本轮完整成功。

- 用户改为100mm后，领域/Widget默认同步为0.10m/100mm，保留可调控件；新Release EXE哈希BDF0F08CA013554BB384619F96954965E29576F77CB1C01ECC89D3C4EDE73599。100mm实际Top1通过原失败区间，随后1660→1813未找到连接，322.4秒后QP=0/输出=0；GUI验证退出0，最新两配置各10/10与主程序启动通过。不能宣称100mm已得到完整可行轨迹。

## 2026-10-03 APF/CDF阶段可观测性与平滑修复

- main起始根/两个规划子仓干净，保留已提交的播放帧同步、全逆解/Top-K及SDK1.0.5；用户路径11111.txt → 全局Top-1 → APF/CDF出现扭曲且无法分阶段检查。
- 源码根因：QP smoothWeight实为一阶差分长度项且所有数值回退削弱/关闭它；以APF密集采样等权追踪导致折角保留；主QP验收要求最小Phi不得下降，即使已安全也拒绝平滑；输出保留重复时间戳且qd/qdd为空，没有执行速度/加速度验收。后处理仅3次小窗口拉直，改进空间有限。
- 修复所有权：ProjectMotionPlanning领域增加阶段快照/统一质量计算/离散时间参数化，修正非均匀参数二阶弯曲QP目标与满足安全阈值下的验收；默认仍一轮QP。Workbench控制器投影视图并拥有会话诊断快照；Widget/新对比窗仅显示。涉及16个源文件/配置，不新增依赖，不迁入MainWindow语义。
- CDF页增加输入/APF/最终阶段选择、逐点应用、动态播放、导出以及质量/关节/TCP/速度加速度对比；显示安全、角度单位/原符号、计算时间与执行时间、TCP相对输入偏差及局部优化边界。原初始栏独立保留，不再被结果覆盖。
- 验收：人工折角/非均匀采样/非法时间/turn/限位/速度加速度回归，真实Top-1数据一次完整优化、高密度碰撞独立复验、UI阶段切换与导出/回放、Release/Debug构建及相关回归。沿用C:/b/rs105-merge，保留用户运行进程。无碰撞不等于严格保持原TCP；明确测量偏差，不宣称非凸全局最优或分段线性轨迹具有连续物理加速度。

- 完成：真实749点Top-1实际TCP与11111.txt复验通过；4592共同节点APF→最终关节长度181.102→73.665 rad、弯曲代价1.43944→0.0858874（约94%下降），仍有最大154.779度折角。最终速度21.4342 deg/s、离散加速度119.7604 deg/s^2、位置/速度/离散加速度/运动碰撞越限均0；独立导入0.001 rad线性插值复验0碰撞段。默认仍一轮QP。
- 实际GUI完整流程退出0，APF阶段4592原始采样点全部播放，应用/导出/停止重启保留报告；完整计算644.339秒（非独占性能基准）。两次最终TXT SHA256一致：500A619461BD20FA1A9B8F17829F3CFDC4F72A60B5734D4193979637D12E08F2。
- 明确保留限制：未达到10mm安全裕量，最终节点Phi=-9.19019mm；TCP相对输入最大偏移993.516mm、姿态151.965度。保守重新定时5463.457秒，不是时间最优；离散加速度通过不代表连续物理加速度认证。详情docs/cdf_stage_quality.md，证据外层build/cdf-quality-results-final和cdf-quality-real-gui-final.log。

- 最终Release/Debug构建和各10/10相关回归、两配置主程序启动通过；修正非法时间曲线、阶段按钮状态与旧窗口关闭问题后补充UI回归通过。最新程序C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe（SHA256 4E7DEE67D5AAD2ACA1D99FEDA9FC1F028EF873FECDA0484A22F5D77F02C40343）；源码UTF-8/CRLF和diff检查通过。

## 2026-10-02 修复播放快于画面呈现

- 保留前两轮未提交变更；当前症状为逻辑进度先于可见帧，独立16ms定时器与Qt合并重绘不构成帧同步。
- 本轮不变量：每次显示姿态必须收到QOpenGLWidget frameSwapped确认后才能推进，隐藏/暂停绘制时不积压时间，终点呈现后才完成并自动导出。由RobotViewport持有呈现票据，typed port/adapter提供窄接口，规划控制器拥有预览进度；不更改领域轨迹/符号/采样。
- 每次呈现最多推进1/60秒预览，负载下降后不追赶；原始点全部测量/碰撞检查仍保留。重点验证暂停绘制、恢复无跳跃、最后一帧完成顺序、真实4592点结果和模式切换。构建沿用C:/b/rs105-merge，Release/Debug验证；不终止用户进程。

- 验证完成：Release/Debug构建及各10/10相关回归、两配置主程序启动通过。真实4592点全部校验，444个显示姿态全部收到呈现确认，未呈现覆盖0次；5秒预览目标实测8.972秒，呈现帧间隔P95=24ms，20次模式预览清理、0重载。修复重在显示同步，过载时允许延长时长。日志见外层build/playback-present-*.log。

## 2026-10-02 继续修复 CDF 播放动作不流畅

- 用户确认卡顿指动作不流畅。已有4592点结果关节段长0.024至14.17度；上一轮仍一回调一个点，目标0.1秒实际46.451秒，GUI心跳良好不能代表动作平滑。
- 保留上一轮所有变更（未提交），本轮新增 MotionPlanningCore 预览时间轴，MotionPlanning 控制器16ms帧插值、原始点批量采样/校验、20Hz界面通知和8ms采样预算；CDF按关节弧长分配预览时间，普通轨迹保留有效源时间比例。角度不wrap，原关节折线路径/导出时间不改。
- 时间轴与采样测试覆盖点密度、turn、重复时间、单点、非法数据和稀疏点间连续运动；实际CDF回归加入SceneExplorer运行事件订阅和显示规划面板，验证采样无丢失、响应、模式切换和最终姿态。
- ProjectScene 将每段独立DebugDraw改为完整折线一次绘制，增量缓存全部TCP点，正确保留Gizmo和相机过滤。实际图像回归通过。
- 验证：Release/Debug 主程序和相关目标构建成功，各10项回归通过；实际4592点默认5秒预览在负载下7.069秒完成、309帧、95%帧间隔<=34ms，全部源点采样/碰撞校验及20次模式清理通过。极短0.1秒压力设置实际5.245秒，不宣称严格实时。最新Release EXE为C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe。
- 构建继续使用C:/b/rs105-merge；保留用户运行进程。无新增依赖或持久化字段，新增MotionPlanningCore预览时间轴接口。

## 2026-10-02 修复升级后轨迹播放与工作台切换

- 当前 main，开始时根及 MotionPlanningWorkbench 子仓干净；沿用 C:/b/rs105-merge 构建。
- 根因：工作台统一清理预览调用 clearAttachmentBindingPreview，在没有装配预览时也重载文档；播放每点解析所有 StoredMotionPlan、逐关节重复刷新场景，并强制开启与独立校验重复的视口连续碰撞查询。
- 修复归属：typed adapter 的装配预览生命周期、ProjectScene/SimulationRuntime 的整组关节更新、MotionPlanning 播放快照。保持所有播放点/符号/喷涂/轨迹线及独立逐点碰撞校验；真实文档编辑终止失效播放。
- 验证完成：Release/Debug 主程序及回归目标构建、各9项相关回归和两配置启动均通过。2001点播放及20次 clearTaskPreview 事件链回归通过；Release 实际4592点 ABB4600 优化结果全部播放/校验，0碰撞/0无效、0重载，界面心跳最大67ms。无新增依赖、无持久化字段变化，新增批量关节应用接口。

## 2026-10-02 合并 Business Source 1.0.5

- 用户授权将新版所有新增内容整合到当前 main，保留全逆解、构型分类、Top-K/APF/CDF/QP 和 UI 卡顿修复；此次完整版本升级适用多模块范围。
- 原根 HEAD d7c1594，开始时工作树干净。已有 SDK 实际为 1.0.3，部分应用曾局部迁入 1.0.4；不能把所有版本差异当成本地定制。
- 业务源码按文件三方比较/手工合并，SDK 整体更新为 1.0.5。原 740 个业务文件和完整原 SDK 保存在外层 build/upgrade-105/backup；data、thirdparty 和本地依赖配置保留。
- 新版采用 typed viewport ports 与拆分场景系统；本地规划通过独立窄接口接入，保持领域算法归属。喷嘴标定、实际模型 FK 和运行时对象移动/复制需要重点验证。
- 使用独立 C:/b/rs105-merge 进行 CMake、Release/Debug 主程序与新旧回归验证，不覆盖原 build 中运行的程序。Release/Debug 构建与主程序启动通过，两配置各31项回归最终通过（Debug查询首轮180秒超时，独立延长预算复验195.78秒通过）。
- 不改项目持久化格式，不引入额外第三方依赖。实际749点5986逆解候选/双页Top-K及CDF传递通过，最大实际FK误差约0.001mm。完整说明见 docs/upgrade_1_0_5_report.md，文件级清单见 docs/upgrade_1_0_5_file_changes.csv。

# 当前项目快照

## 2026-09-23 Basic Planning 关节导出和轨迹点开关

- 新增“导出关节轨迹”，直接导出当前选中计划的全部关节数据；与 CDF 导出复用 QSaveFile 原子写入。格式匹配 ik_joint_angles.txt：秒、度、六位小数、制表符，导出存储的 IK 角度，不执行回放符号映射。
- 新增默认关闭的“显示轨迹点”；控制器保存会话显示状态，通过原控制点 overlay 的 showPoints 参数控制球形标记，轨迹连线与末端运行轨迹独立。
- 已验证 Debug/Release RobotQtViewer 与回归目标构建，两种配置各 2 项回归通过；2001 行导出完整、角度符号与单位正确、参考表头一致，球标开关经过实际帧缓冲图像比较；Release 主程序启动退出码 0。


## 2026-09-23 Basic Planning 逆解与实际 TCP 对齐

- 任务：修复导入末端轨迹逆解后，关节回放的喷枪顶点偏离目标；保留 Joint1/4/5/6 的既有符号映射。
- 根因：原求解器的固定理想 IRB4600 DH 与旧固定工具矩阵，不等于当前 URDF 安装位姿及已标定喷枪 TCP；原求解器仅验证自身 DH 残差。
- 所有权：ProjectScene 从当前模型、基座、工具创建独立 RobotInstance 快照；ProjectMotionPlanning 通过可选世界坐标 FK 回调求解；MotionPlanning 控制器连接模型快照并转换输入种子的符号约定。快照计算不修改视口机器人。
- 算法：实际 TCP 有限差分 Jacobian、阻尼最小二乘、步长限制和回溯；对输入矩阵的小幅非正交误差做 SO(3) 投影；非法位姿拒绝。
- 兼容：旧 DH API 在未传回调时保留；无新增项目依赖、无项目 schema 变更，旧关节数据需要重新逆解。
- 验证：11111.txt 全部 749 点成功，实际回放位置最大误差由 176.197 mm 降为 0.000994557 mm，RMS 0.000213805 mm，最大姿态误差 0.000045337 度。Debug/Release 主程序编译、IK/喷涂/导入回归各 3 项通过；Release 主程序启动退出码 0。
- 产物：上一级 build/ik-11111-round-trip.csv、ik-11111-round-trip.png、ik-11111-comparison.png；测试入口 RobotQtViewerIkPlaybackSmoke，固定六点样例位于 SprayMeasurementSmokeTest/ik_tcp_poses.txt。


## 2026-09-17 喷涂距离与角度扩展

- 当前工作区为桌面 `RS2026-BusinessSource-1.0.0/RS2026-BusinessSource-1.0.0`，分支 `main`；下方 2026-07 的路径和分支是历史记录。
- 开始时 `ProjectScene.cpp` 有用户已有修改；保留已校准的 ABB4600 枪口坐标及法向，不重做装配标定。
- 不变量：圆锥与测量共用 FK 枪口位姿；距离取沿工具 +Z 的最近正向 STL 交点；角度为法向偏角，正负按工具局部 X 轴右手规则。
- 所有权：ViewerCore 提供非 Qt 几何测量，viewport services 转发，MotionPlanning 控制器持有本次播放采样，Widget 只显示结果和曲线。
- 安全扩展点：已有 `intersectRayTriangle`、AssetManager CPU 网格、`applyJointValuesToRobotRuntime`、QSaveFile。无新第三方依赖，无项目格式变化。
- 核对风险：burnner 是静态机器人链接；不能使用简化碰撞代理代替 STL；播放采样必须在整组关节应用后；无命中使用无效标记而非 0。
- 已验证：上一级 `build` 的 Release 目标 `RobotViewerCore`、`MotionPlanningEditor`、`RobotQtViewer` 编译通过；默认工程隐藏启动退出码 0；新增 spray smoke 覆盖最近交点、角度符号、STL 绕序、FK/目标移动、无命中、播放采样、自动 TXT 导出和双图像素检查并通过。

更新时间：2026-07-20

## 当前基线

- 工作区：`D:/program/src/RS2026_CodexDev`
- 分支：`branch-codex-dev`
- 基线提交：`5f4ca0fda7fed65fc49690b6c0da1860c12fe87a`
- `thirdparty` 与 `data` 子模块处于主仓库记录的提交。
- 当前工作树包含本轮明确执行的 vcpkg 移除、路径修订和历史文档清理。

## 模块现状

- `SMRobotCore`：机器人模型、IO、运动学、运行时、轨迹、碰撞和核心 SDK。
- `SMRobotPlatform`：资产、渲染、场景、相机、项目文档、仿真运行时、传感器和平台 SDK。
- `SMRobotApps`：`RobotViewerCore`、`RobotQtViewer`、QuickStart 和教程。
- `SimWorkbench`：装配、碰撞、运行、运动规划、喷涂、分析和数字孪生工作台。

## 当前架构依据

- GUI/document-view：`docs/architecture/document_view_gui_contract.md`
- Workbench：`docs/architecture/simulation_platform_document_workbench_contract.md`
- SDK：`docs/architecture/modular_simulation_platform_sdk_contract.md`
- DLL component：`docs/architecture/sdk_dll_component_policy.md`

## 构建状态

- 已在 `build/codex_cleanup_check` 使用 CMake 3.31.3 与 Visual Studio 2019 generator 完成 configure/generate。
- configure 使用 `USER_TANGTANG_4090_Windows=ON`，未启用 example、regression 或 tests。
- 本轮尚未执行目标编译和 CTest。
- 当前 Windows 主开发配置依赖 `D:/PreBuild`，使用 `USER_TANGTANG_4090_Windows=ON`。
- 根目录忽略的 `PrebuiltPackages` 早于当前源码，不应作为当前源码的验证产物。

## 当前风险与下一步

- 下一步构建 `RobotQtViewer` 和 `RenderCoreShaderResources`。
- 测试时同时启用 `BuildTests=ON` 与 `BuildRegression=ON`。
- 不复用旧 `PrebuiltPackages` 证明当前源码可构建；SDK 应使用新的 install prefix 生成。
- 仍有效的后续计划见 `plans/README.md`，执行前必须重新核对源码。

## 2026-07-21 Coating Analysis 厚度预测框架实施快照

- 当前分支：`branch-codex-dev`；开始实施前工作树干净。
- 当前任务：执行 `plans/coating_analysis_thickness_prediction_framework_revision_plan.md`，形成模型导入、演示厚度预测、彩色 surface-scalar overlay、厚度色标与 hover 数值探测闭环。
- 领域所有权：`SMRobotSpray/SprayThicknessPrediction` 只生成厚度标量；通用色图进入 `SMRobotPlatform/VisualizationSDK`；`RobotViewerCore` 负责 scene overlay 与精确 probe；`SMRobotWorkbenchPaintingAnalysis` 负责 Qt workbench 编排。
- 当前可复用点：`SceneEntityWorkflowController`、`ViewportReloadWorkflowController`、`RobotQtViewerDocumentViewRegistry`、`RobotQtViewerSelectionModel`、`GeometryDesc::colors`、`ModelNode::setModel(...)`。
- 当前待退役路径：`ProjectRuntimeBuilder` 依据 `burner.stl` 文件名就地修改缓存 `ModelDesc` 的演示着色逻辑，以及 `RobotViewerCore` 对 `SprayThicknessPrediction` 的条件依赖。
- 构建顺序风险：根 `SeqPackages` 当前在 `SMRobotApps` 之后才加入 `SMRobotWorkbenchPaintingAnalysis`；若 `RobotQtViewer` 链接 Painting Analysis 包，需要将该 workbench 包前移到 `SMRobotApps` 之前，但不得改变 Core/Platform 包顺序。
- 预期验证：分别构建 `SprayThicknessPrediction`、`VisualizationSDK`、`RobotViewerCore`、Painting Analysis 实现 target 与 `RobotQtViewer`，运行新增 headless tests，并执行隐藏启动 GUI smoke。

## 2026-07-21 OMPL 阶段 A-E 实施快照

- 当前分支：`branch-codex-dev`；任务目标是执行 `plans/ompl_motion_planning_project_adapter_integration_plan.md` 到阶段 E，形成无 Qt 的 project/collision/OMPL/headless/GLFW 闭环。
- 当前工作树包含另一项 Coating Analysis 实施中的 `RobotViewerCore`、Qt viewport、`VisualizationSDK`、`SprayThicknessPrediction` 和 Painting Analysis 改动；本任务不修改或回退这些文件。
- 主要影响模块：`SMRobotCore/RobotCore`、`SMRobotCore/RobotIO`、`SMRobotPlatform/SimulationRuntime`、`SMRobotMotionPlanning`，以及 motion planning 自己的 regression/feature probe。
- 所有权：`RobotCore` 保存关节限制；`RobotIO` 解析 URDF limit；`SimulationRuntime` 负责批量关节状态及 attachment 更新；`ProjectMotionPlanning` 构建任务私有 runtime/collision snapshot；`MotionPlanningOmpl` 私有封装 OMPL；GLFW probe 只消费正式服务。
- 当前安全扩展点：`RobotJoint` 是普通数据结构；`URDFLoader` 的 limit 解析当前被注释；`ProjectSimulationRuntime` 已有单关节设置、统一 `update()` 和 attachment propagation；`ProjectCollisionRuntime` 已有 detector ID、update/check/result 接口。
- 当前缺口：`MotionPlanningCore` 只有线性插值且公共 request 直接服务旧调用；`SMRobotMotionPlanning` 只有一个 component；`HeadlessProjectCollisionExample` 与 `ProjectGlfwViewer` 目录均缺少 `main.cpp`。
- 目标项目：`config/projects/420-red4600-tool.sys.json`；规划机器人 `Red4600`，静态环境机器人 `ATPPZ350`，工具 `420_tool_attachment`，detector `default_collision`。
- 风险：新增 `RobotJoint` 字段会改变源码级结构布局但不改变虚函数表；必须同步所有 loader 并构建依赖 target。默认场景未必能稳定找到“直线碰撞但可绕行”的构型，若失败应保留 smoke test 并报告，不得削弱 detector。
- 验证顺序：独立 CMake configure；构建 `MotionPlanningCore`、`MotionPlanningOmpl`、`ProjectMotionPlanning`、headless regression 和 `ProjectOmplPlanningViewer`；运行无窗口固定 seed 场景；最后检查可执行依赖不含 Qt。

### 实施完成状态

- 阶段 A-E 已完成，阶段 F 未开始。
- 新增 `MotionPlanningOmpl`、`ProjectMotionPlanning`、`ProjectMotionPlanning-Headless` 和 `ProjectOmplPlanningViewer`；所有规划业务入口位于非 Qt 模块。
- 固定真实项目 fixture 已验证“起终点合法、直线插值碰撞、RRTConnect 可绕行”，并完成最终路径逐段复验、保存/重载和 runtime 回放。
- `build/codex_ompl_stage_e` Release 构建 `ProjectMotionPlanning-Headless`、`ProjectOmplPlanningViewer`、`RobotQtViewer` 通过；CTest 1/1 通过；隐藏 GLFW/OpenGL 运行通过；PE 依赖不含 Qt。
- 下一步若进入阶段 F，应直接复用 `ProjectMotionPlanningService`，Qt 层不得重新构建 OMPL state space 或直接调用 collision backend。

## 2026-09-23 APF 碰撞段预修复

- 分支 main；根仓库和两个相关子模块工作树在任务开始时干净。
- 在 ProjectMotionPlanning 领域服务替换导入轨迹的 OMPL 预修复；Qt workbench 只更新算法文案。通用 OMPL 规划入口保留。
- 不变量：原始关节符号映射、时间戳、后续 QP/CDF 流程保持；两侧安全锚点固定，替换段及完整路径验证通过后才允许优化，不发布碰撞残留路径。
- 采用实际检测器的显式距离查询生成斥力；受限步长、确定性切向脱困和有限迭代，不增加依赖。
- 验证：私有 APF 算法回归、指定 ik_joint_angles.txt 真实网格场景、Release/Debug 规划测试和 RobotQtViewer 构建。离散运动检查不等于连续碰撞证明。

### APF 任务完成状态

- 已实现 APF 安全锚点预修复、0.001 rad 全流程碰撞验收、连续关节解缠输出及真实回放复验，原 QP/CDF 目标与参数保留。
- Release/Debug 各 4 项回归通过；真实输入 749 → 8641 点，修复与导出重新导入后的线性回放均 0 碰撞段。真实文件最终回归采用 1 轮 QP；间距约 0.00675 mm，未达默认 10 mm，按 partial 如实报告。
- 当前用户运行中的 Release EXE 无法覆盖，最新可执行文件另存为同目录 `RobotQtViewer_APFrx64.exe`；Debug 正常构建。
- 细节、哈希及验证命令见 `docs/agent_change_audit.md` 本日章节和同级 build 目录的 `APF-validation-report.md`。

## 2026-09-23 QP 默认单轮调整

- 用户要求当前代码改为一轮 QP；GUI 控件初值、GUI settings、领域 options 和 headless 默认值统一为 1，保留显式轮数设置能力。
- 保留已有 APF、QP 求解器内部迭代和碰撞验收；当前已开始的旧进程任务不会动态改变参数。
- 本轮只调整默认值，不声称解决计算性能；上一轮真实数据单轮完整流程约 1192 秒。

## 2026-09-23 轨迹减点与平滑

- 保留之前未提交的 APF/单轮 QP 工作。本轮算法归属 ProjectMotionPlanning；不调整关节符号、碰撞检测器或场景几何。
- 优化节点间隔独立为 0.04 rad，运动验收仍为最多 0.001 rad；保留原始输入时间点和首尾配置。
- APF 局部段采用有界碰撞验证捷径；QP 前后采用包含接缝的时间加权弯曲代价下降平滑。后平滑不得降低已达到的最小点间距（达到目标后允许保持目标）。
- QP 连续关节统一使用解缠坐标，避免逐点折回正负 pi 引入虚假折角。
- 验证目标：几何与周期关节回归、749 点输入且匹配 GUI 默认参数的单轮全量验证及平滑度对比，Release/Debug 编译和测试。

- 完成：4529 点、0.001 rad 独立回放与再导入均无碰撞段，QP 一轮耗时 870.532 秒，点间距仍未收敛到 10 mm。Release/Debug 各 5 项回归通过。末端曲线对比与限制见同级 build/APF-smoothing-report.md。

## 2026-09-24 多逆解与调试

- 当前相关工作树干净。本轮只新增直接读取末端控制点的多逆解与 Basic Planning 调试入口，保留单逆解和符号映射。
- ProjectMotionPlanning 拥有有限范围多初值搜索、实际 FK 复验、turn 枚举、去重和单解序列组装；Workbench 拥有后台任务与主从表投影，应用仍走 document/runtime 正式入口。
- 数值搜索不保证枚举全部离散根；无机械限位的 continuous 关节采用显式可配置搜索窗口，不把窗口宣称为物理限位。
- 后台单线程独占 FK 快照，支持取消/进度；源项目、轨迹、机器人改变时丢弃过期结果。
- 验证：领域边界/多圈回归、真实 ABB/TCP 多解复验、GUI 单点/选解播放、Release/Debug 构建。

- 完成：多逆解接口、后台任务、可配置搜索窗口、候选主从表、单点应用和选解序列播放均已实现。真实 749 点得到 5986 组有效候选，搜索约 11.45 秒；Release 5 项、Debug 3 项相关回归通过。可执行文件为同级 build/Release/bin/RobotQtViewerrx64.exe。详见本日 audit 和 multi_ik_debug_usage.md。

## 2026-09-24 多解单点应用后结果消失修复

- 保留上一轮未提交的多逆解实现。根因是应用关节角的文档通知触发 ToolSetup::refresh → syncPinnedRobotMountFrames → ViewportPreviewChanged，旧处理无条件清空多解。
- MotionPlanningModuleController 按 preview payload 区分几何变换和显示/焦点状态；只有几何变化使多解失效。复用正式 preview state，不屏蔽消息、不绕过 document mutation。
- 增补跨面板嵌套通知回归，先确认旧实现失败，修复后验证连续应用、候选切换、选解播放及真正基座变换的失效行为。
- 当前用户原 EXE 在运行，Release 修复版另名为同级 build/Release/bin/RobotQtViewer_MultiIKFixrx64.exe。

## 2026-09-24 分层图 Top-M 筛选与 CDF 初始解

- 开始时根仓库及相关子仓工作树干净，基线已包含多逆解和应用后结果保留修复。
- 领域 ProjectMotionPlanning 新增分层图 API：仅相邻层全连接、无节点代价/碰撞检测，以实际未折回关节差和可配置权重精确求 Top-M；回溯存储受显式预算约束，超限报错而非静默裁边。
- Workbench 增加 M/权重、筛选/取消、结果排名与逐点明细，后台计算，源逆解失效时取消旧任务。选中结果以原 IK 符号、度数和时间戳进入现有 CDF 初始栏。
- 本轮不自动执行 APF 或 CDF，不进行碰撞后的最终 Top-K 排名。用户本轮的前 K 组输出按可配置 M 条运动学候选实现。
- 验证：小图穷举对比、turn/同价/无解/取消/预算回归、GUI 到 CDF 符号时间对照、真实 749 点、Release/Debug 构建测试。

- 完成：精确 Top-M API、后台筛选/取消、结果及完整明细、选中候选进入 CDF。Release 4 项/Debug 2 项回归通过；真实 749 点 Top-30 图计算约 0.0095 秒。当前可执行文件恢复为同级 build/Release/bin/RobotQtViewerrx64.exe。

## 2026-09-24 构型选择对比窗口

- 保留当前未提交的 Top-M 实现。新增 Basic Planning 按钮及 Qt 绘图窗口，由 controller 从完整排名结果投影一基逆解编号，不读采样表格，不改变筛选或优化算法。
- 支持任意多选、叠加/分行和控制点区间；源结果失效时关闭旧窗口。编号仅代表当前点内的候选。
- 验证完整序列投影、多选/区间、生命周期、Release/Debug 构建和 GUI 回归。

- 完成：对比窗口及全量只读投影已接入，Release 三项/Debug 一项 GUI 回归通过，749 点单点差异绘图已视觉核对，主程序启动检查退出 0。可执行文件为同级 build/Release/bin/RobotQtViewerrx64.exe。

## 2026-09-28 固定起点构型的 Top-K

- 开始时根仓及相关子仓工作树干净。保留全局 Top-M，额外按首层每个实际逆解（含 turn）独立求 Top-K，K 默认 1、可配置；不硬编码八组，不改变边代价或增加碰撞检查。
- ProjectMotionPlanning 复用精确 DP，固定首层有效前缀；Workbench/controller 持两套结果，结果栏和绘图对话框各两页，两页都可传 CDF，明确起点编号和组内排名。
- 仅新增领域查询接口，保留原 filter；Qt 内部信号携带结果类别，避免跨页取错轨迹。输入/参数失效同时清两套结果和窗口。
- 验证小图按起点穷举、K=1/多条/组合不足/单层/turn/取消/预算，GUI 双页数据和 CDF 逐点对照，Release/Debug 构建测试及真实文件统计。

- 完成：双页结果/双页绘图、每起点独立 Top-K 与 CDF 应用已实现，并显示精确全局榜内名次或 >M。Release 领域及三项 GUI、Debug 两项测试通过；749 点真实文件得到 8 组×3 条，组阶段约 0.0215 秒。原程序在运行，新版交付 build/Release/bin/RobotQtViewer_StartTopKrx64.exe。

## 2026-09-28 CDF/QP 等价提速

- 当前根仓及子仓工作树干净。保留 QP 轮数、矩阵/容差、APF、原始点、平滑参数和 0.001 rad 碰撞验收精度；不依赖修改预编译 Collision/SimulationRuntime SDK。
- 在 ProjectMotionPlanning 引入单次 repair 私有批量查询上下文：独立场景并行评估互不依赖的点/边，精确 double 键缓存重复距离和运动验证，不共享可变碰撞场景、不量化角度。每轮规划重新建立/销毁缓存。
- 保留单工作场景模式用于回归比较；默认按硬件限制最多四个。Qt/CDF 数据和优化约束不变，底层同步接口返回前等待批量工作结束。
- 验证：保留原 baseline EXE 和同输入完整输出；领域小场景单/多工作场景对照、缓存与无缓存对照、原有回归、749 点 GUI 默认参数全程基准以及独立高密度回放、Release/Debug 编译。

- 完成：Release/Debug 各 6 项通过；同一输入 861.754 → 316.912 秒，输出 4529 点 TXT SHA256 完全相同，独立 0.001 rad 复验均 0 碰撞段。原安全间距未达标状态保留。主程序已更新为同级 build/Release/bin/RobotQtViewerrx64.exe，报告见 cdf_qp_performance.md。

## 2026-09-28 Top-K 输入耗时复核

- 用户确认慢输入来自 Top-K，并非上一轮 ik_joint_angles.txt。当前保留此前提速的所有未提交修改。
- GUI 旧入口同步占用 UI 线程且未接 progress；改为 controller 私有快照后台执行和模态阶段窗口，记录 EXE、配置、输入来源、阶段时间。期间文档变化则拒绝提交过期结果。
- APF 梯度探针、碰撞区间扫描和局部 QP 查询复用领域层独立场景批量查询/精确缓存；保持 APF 搜索、有限差分、QP 参数及最终 0.001 rad 新鲜复验。
- 用现有全局 Top-M 第 1 条 749 点候选进行顺序对照；实际用户所选编号尚未明确，不能把该复测当成其具体运行。验证包括 APF 批量等价回归、查询回归、Release/Debug 主程序构建。

- 本轮完成：全局 Top-M 第 1 条实测 1012.320 → 439.964 秒；难段 1660→1813 为 726.794 → 277.199 秒。两版 4592 点导出 TXT 逐字节一致，SHA-256 为 c73a1ea0998ec31a7b4fd5ae2e5bfb579fe0bc5d1cbdc3e8cdfc811b2a4381ce；独立 0.001 rad 复验均 0 碰撞段，安全间距未达标的返回 1 状态保持。
- CMake configure、Release/Debug 主程序与相关目标构建通过；两配置各 6 项领域回归及新 GUI 阶段/线程/过期结果回归通过。阶段窗口已视觉核对，主程序 smoke 退出 0，源码 UTF-8/CRLF 与 diff 检查通过。
- 当前 GUI：同级 build/Release/bin/RobotQtViewerrx64.exe；每次运行日志位于 EXE 同目录 log/cdf，结束摘要给出路径。完整证据/口径见 docs/cdf_qp_performance.md 第二轮；未把第一轮原始文件基准推广为任意 Top-K 的耗时保证。未新增公共 API、第三方依赖或项目持久化字段。

## 2026-09-29 GPU 碰撞查询接入核对

- 当前根仓、MotionPlanning 与 Workbench 保留上轮未提交提速改动；本轮尚未修改算法、编译配置或 EXE。任务是核对并推进 GPU 碰撞/距离查询，保持现有几何与过滤语义。
- NVIDIA 驱动查询及 CUDA Driver API 的 cuInit/cuDeviceGet 成功：RTX 5050 Laptop，8151 MiB 可见显存，compute capability 12.0；驱动 591.91。nvidia-smi 显示的 CUDA 13.1 是驱动支持版本，不是已安装 Toolkit 的证明。
- PATH 未找到 nvcc，标准 NVIDIA GPU Computing Toolkit 目录与本工程内也未找到 CUDA 编译工具/内核。暂未安装任何依赖或声称已执行 GPU 碰撞内核。
- 本工程依赖 PrebuiltPackages 下 Collision/SimulationRuntime；工作区未找到 CollisionWorld.cpp 或 ICollisionBackend.h 的实现源码。CollisionBackendType 只有 Default/Fcl/Coal，CollisionWorld 的 backend_ 为私有，没有公开后端注入接口；ProjectCollisionRuntime 也不公开完整对象/有效碰撞对快照。
- 正确所有者是 Collision：需在后端层接入 GPU 批量碰撞/最小距离/最近点查询，继承 includePairs/excludePairs、ACM、geometryRole/source/group/mask 和对象标识；规划域 CdfQueryBatch 消费批量接口。不能通过显示网格或自行复制筛选规则冒充原检测器。
- 推荐先保持原网格的 GPU 批量后端与 CPU 回退/最终高密度复验；SDF 是另外一种近似路线，不在未经说明时替换。必须将距离误差、碰撞误判、最近点/梯度一致性、CPU/GPU 传输与端到端耗时分开验证，不预先承诺倍数。
- 用户已确认只有当前工程和预编译 SDK。现有接口可读部分几何缓存、显示描述及 ACM 单对查询，但不提供可替换后端或带完整过滤语义的批量场景快照；不能据此宣称可直接替换原 FCL。没有修改预编译 SDK 或在 UI 中伪装 GPU 开关。
- 完整精确后端迁移需要可扩展的 Collision SDK。当前工程内可考虑独立 GPU 距离场辅助 APF/CDF，由 CPU 保留精确运动检查及最终验收，但这是近似搜索路线，会影响梯度和结果轨迹，不能按此前“保持现有功能/查询语义”要求擅自替换。后续须明确是否接受这一范围变化；目前仅完成环境与接口核对，未安装 CUDA Toolkit、未新增 GPU 内核或更改程序。

## 2026-09-30 ABB 构型分类与图表

- 分支 main；开始时根仓和两个相关子仓干净。修改 ProjectMotionPlanning 与 MotionPlanningEditor 及已有 smoke 回归。
- 旧实现按关节数值排序，绘图只投影点内行号。新增实际模型 FK 的肩/肘/腕几何标签，固定 B1～B8；候选身份和 turn 保留，分支边界/未分类不硬塞八类。不是 ABB RAPID confdata。
- 分类归领域结果，controller 统一投影，两页结果与绘图消费同一快照。原始候选选择、播放、Top-M/固定起点 DP 与 CDF 传递不改语义。
- 验证：Release/Debug 目标、真实 11111.txt 全轨迹 FK/构型与周期不变性、GUI 两页/CDF/应用/播放及分层图回归。当前旧 Release 主程序正在运行，必要时另名交付，不终止用户进程。

- 完成：固定 B1～B8 分类、两页结果/对比图和 turn 调试已接入；真实 749 点 5986 候选分类/FK/分层图验证通过。Release/Debug 构建、各四项回归通过，界面视觉核对及新版启动通过。
- 当前新版入口为同级 build/Release/bin/RobotQtViewer_Branchrx64.exe（原 EXE 运行中，未覆盖）。使用前重新执行全逆解和分层图筛选；定义及限制见 docs/ik_configuration_branches.md。

## 2026-09-30 按起点结果表卡顿

- 用户截图明确故障在 Basic Planning 主面板结果分页，K=1；问题为长轨迹明细逐格替换时 ResizeToContents 重复测量，归 UI 临时显示层。
- 三个分层图表统一批量更新期间禁用自动列宽，结束后测量一次；保留全部原始行、固定构型标签、原始候选身份与 CDF 传递。旧版长明细替换 30 秒超时，修复版 749 行约 27–31 ms。
- 回归入口 RobotQtViewerConfigurationTabsSmoke / --configuration-tabs；使用主程序主题、8 起点 K=1，并保留现有真实 IK、播放、分层图与 CDF 传递测试。源码已有构型分类等未提交修改均保留。

- 验收完成：Release/Debug 主程序与 smoke 目标构建通过，两配置各 5/5 回归通过；新版 Release 启动检查退出 0，截图视觉核对及 UTF-8/CRLF、git diff --check 通过。运行中的原 EXE 未覆盖、未终止；交付同级 build/Release/bin/RobotQtViewer_TopKFixrx64.exe，SHA256 4B84F3C0013B718EA8C4B0902AC093E00D253C086A44ECCF429A310B728EFAB0。测试日志 topk-fix-tests-release.log / topk-fix-tests-debug.log。无新增公共 API、第三方依赖或持久化字段。


## 2026-10-05 TCP 扫描线交叉与分图对照

- 分支 main，基线 6489cc4，根仓与规划子仓初始干净；上轮安全结果 results/11111_Top1_Smooth_100mm 只读保留。
- 根因核查：关节 chord 多尺度平滑仅受逐站 100 mm 球形走廊约束，缺乏 TCP 参考形状目标；允许相邻扫描线越线。所有几何改进归 ProjectMotionPlanning，回归程序仅注入真实场景与记录证据。
- 计划先量化参考/旧结果的投影交叉，再试验实际 FK 的形状恢复与碰撞约束精化，修复后接入正式 APF/CDF；默认一轮 QP、端点完整位姿、合法 turn 和 100 mm 不变。最终原始关节线性插值 0.00025 rad 独立复验，不把节点无碰撞等同连续认证。
- 交付独立原始/旧结果/新结果图，统一坐标与指标；Release/Debug 构建、相关回归、真实 Top-1 验收。不得复用已知密采样失败的 run1 候选。

- 实施中：新增 restoreApfTcpShape/refineApfTcpShape，先有序 TCP 圆角参考与碰撞限制位移恢复，再固定该 TCP 曲线投影关节平滑；已接入 APF 快照前和最终 QP 输出前。无 GUI 或公共 API 变更。
- 检查点试验 shape-restored-run2 独立 0.00025 rad 共149563样本、0碰撞/0限位，偏移77.5512mm；TCP >45度折角65→18、最大145.61→94.33度、RMS偏移66.24→16.80mm。只是检查点证据，完整正式流程待最终验收。
- Release/Debug 各10/10初轮回归通过；最终健壮性小修后 Debug3/3通过且主程序启动退出0。完整真实运行写入外层build/apf-shape-top1-delivery及同名log，报告脚本build/report_tcp_shape_delivery.py，拟交付results/11111_Top1_Shape_100mm；结束后补最终结果、Release重编译和audit。

- 本轮最终完成：完整Top-1结果已交付results/11111_Top1_Shape_100mm，4528点/最终138362次0.00025rad独立采样0碰撞，最大偏移87.664mm。新增投影交叉55→7、>45度TCP折角65→11；原始/旧/新已分图。仍未达到10mm余量、保留局部离散折角，完整计算2025.22秒；详见本日audit与交付README。Release/Debug构建/回归/启动均通过，当前EXE为C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe。
