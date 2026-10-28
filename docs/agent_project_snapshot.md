## 2026-10-28 B 功能更新到 A（已完成）

- 按用户五项要求迁入曲面切片/UV、标定、涂层与论文算法、独立 IK、PostModel、外部规划、虚拟挡板、GPU 搜索及轨迹分段。APF/CDF 采用 B；A 的直接导出/WPR 连续性和 16 份独有碰撞工作台文件保留，7 份共有碰撞文件合并 B 的最近距离及配对改进。
- 默认 `ABB4600-Aburnner.sys.json` 实为 FANUC `m710ic_50_drake`；挂载/TCP/用户坐标系/虚拟挡板保留，无初始轨迹。资源与配置随 RuntimeConfig 增量部署，NVRTC 私有运行库由 ProjectMotionPlanning 的运行时目标部署。
- 原有 main/两个子仓的未提交修改与暂存补丁、覆盖前文件均备份于外层 `output/b_to_a_20261028`。未更改预编译 SDK/许可证、未提交 Git。领域边界检查通过，未迁移的 6 份旧 Qt/优化器文件另存备份。
- 已修复集成导出连接、Windows 测试路径、Debug 模板节数、碰撞结果接口缺口；分段 manifest 原子替换增加有限重试及错误诊断，连续 5 次分段/取消回归通过。ABB 专用测试改用独立场景。
- 独立目录 `C:/b/rs-bmerge-20261028`：Release 68/68，Debug 68/68（66 项 + 2 项延长时限独立复测），均通过。FANUC 两配置实际启动/保存/重开通过；RTX 5050 搜索/IK、GPU 膜厚、标定、真实 UV→238 点→PAQ、跨工作台轨迹传递已通过。最终证据和限制见 `docs/b_to_a_migration_20261028.md`。

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


## 2026-10-08 动态轨迹播放卡顿复查

- 基线 main/bf1c0ea；根仓、MotionPlanning 与 Workbench 子仓初始干净。目标：定位 Motion Planning 动态播放的主视图卡顿与进度超前，保留完整轨迹与校验语义。
- 检查 RobotViewport 呈现票据、controller 采样/测量/通知，以及主程序真实渲染路径；所有者为既有领域服务、viewport 与 typed adapter/controller，遵守两份 GUI 架构契约。
- 使用现成 4528 点结果 results/11111_Top1_Shape_100mm/final_joint_trajectory.txt 复现，不重跑 APF/QP。验证真实呈现、UI 响应、完整采样、跨模式和末帧行为，构建 Release/Debug 并运行相关回归。当前未定位根因，尚未改源码。

- 后续原主窗口定位：LayoutRequest 触发全局语言服务递归重翻译，反复遍历数千行轨迹数据并执行模板正则；单个布局事件约85～99ms，多个事件累积使主视图几百毫秒才更新。原简化测试没有覆盖完整应用语言过滤器，不能代表用户窗口性能。
- 已在 Shared 语言服务加入8MiB近似字节计费的文本命中/未命中缓存、按model数据变化失效的表格翻译缓存；语言/词库变化清缓存。Workbench将播放通知与喷涂读数刷新移到上一姿态frameSwapped确认之后。原布局、轨迹数据、逐点校验与APF/QP保持。
- 原主窗口Release验收：同一4528点最终轨迹，主视口连续碰撞关闭，喷涂测量和末端线显示开启。基线120013ms超时、帧间隔P95=706ms；修复版18181ms/P95=45ms，最终交付版19637ms/P95=46ms、最大49ms；全部4528点完成、0碰撞/0无效。5秒是预览目标，实际随负载延长；不以超时基线计算完整耗时倍数，不宣称固定60FPS。
- 新增仅显式参数启用的原窗口诊断入口 --profile-playback / --profile-playback-report，走原MainWindow、导入按钮和播放按钮；输出帧呈现、UI心跳、源点总数、完整结果和截图。正常启动无新增界面。Release/Debug主程序和Localization目标已构建，两个配置相关5/5回归通过；最终源码UTF-8/CRLF和diff检查通过。交付入口C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe。

- 最后补充验证：两配置主程序正常启动 --smoke-exit-ms 1500 均退出0（playback-main-smoke-{release,debug}.log）。Debug原主窗口全轨迹在120142ms触发诊断超时，615帧、P95=233ms、finished=false，未取得完整源点验收；见playback-main-delivery-debug.{log,json}。Debug性能仍较慢，不能把编译/常规回归通过表述为Debug完整播放通过，本轮完整播放与提速结论仅适用于Release。测试进程均已自行退出。

## 2026-10-08 CDF RAPID MOD 导出

- 基线main/1ad4f8a，根仓及Workbench子仓干净。用户要求CDF页新增附件MainModule.mod格式导出；已确认使用当前喷枪TCP并附工具定义。
- 附件格式为MODULE/CONST robtarget/PROC main/MoveL v50 z10 tool0，robconf全零、外轴9E+09。改为当前喷枪工具；位置采用当前实际世界TCP（m转mm），四元数wxyz。未知ABB confdata和负载不猜测，文件与UI标明模板占位和MoveL圆滑路径未复验。
- 所有者为ProjectMotionPlanning导出模块（模型快照/验证/序列化），Workbench只提供CDF最终计划、实际TCP FK快照、按钮和原子保存；保留既有TXT导出。验证全部点而非表格采样、符号/turn、工具相对法兰刚性、四元数精度和格式往返，Release/Debug构建与原主窗口按钮检查。

- 完成：CDF页新按钮、完整TCP MOD与工具定义已接入；Release/Debug构建和各2/2回归通过，两配置原主窗口全部4528点导出/FK核对通过。文件results/CDF_RAPID_Export/MainModule.mod，交付EXE仍为C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe；ABB confdata和负载占位以及插补限制见docs/cdf_rapid_export.md。用户旧EXE未终止。


## 2026-10-08 tool0 导出、12 列末端轨迹导入与等价提速

- 基线 main/1ad4f8a，保留上一轮 RAPID 导出及原主窗口诊断未提交修改。用户要求按桌面 MainModule.mod 固定 tool0，保留已有目标坐标语义；12 列导入使用 mm→m、wxyz、显式秒时间和 mm/s→m/s。
- 根因：通用矩阵解析接受任意 >=4 列，每四行拼矩阵，抢先吞掉12列点表；零时间错误回退行号。修复归 ProjectMotionPlanning，Workbench仅默认值和提示，预编译SDK不修改。
- 安全余量领域/UI统一0.0005m。提速不改QP轮数、搜索顺序、随机数、采样与验收条件；优先复用查询工作线程，保留独立场景与同步批次屏障。
- 验证：附件全部行和格式兼容/无效输入、tool0格式及实际FK往返、查询及相同0.5mm参数优化对照；Release/Debug构建和原主窗口导入/导出检查。改余量前后不宣称轨迹不变，等价性只针对同参数提速。

- 本轮实现已完成并构建Release/Debug：tool0模板、234点12列导入、0.5mm默认（含APF隐藏下限移除）、查询线程复用及失败窗口距离提前拒绝。原窗口两配置导入/导出通过；600点同参数对照1199点输出逐字节一致，但64.37→63.71秒改善很小，未证明完整Top-1大幅提速。Release/Debug最终均9/9通过，所有测试已结束；详情见本日audit与cdf_import_tool0_performance.md。GPU追加核对发现底层后端可注入，上层运行时未开放，未更换GPU近似算法。

## 2026-10-08 失败复核、恢复10mm与统一密采样验收

- 最新用户要求恢复安全距离10mm。main/1ad4f8a，保留已有MOD、导入及查询优化未提交变更。
- 失败日志cdf_20261008_175327_123_30612为234点trajectory的Top-1，0.5mm、28个APF区间；0.001rad候选验收后，最终0.00025rad检出7段碰撞并拒绝输出。旧10mm同输入返回2050点但未达到所请求余量，不能声称原结果满足10mm。
- 领域ProjectMotionPlanning统一APF/QP/平滑候选与最终碰撞验收上限0.00025rad；保留最终不走缓存的验证。领域/UI默认恢复0.01m，有限差分步长等无关0.0005值不改。Workbench仅同步默认与日志。
- 原MainWindow诊断增加导入-全逆解-Top1-优化及阶段导出，使用真实234点和同项目配置验证；不使用简化测试窗口替代。Release/Debug构建与相关回归、原窗口完整运行核对输出/碰撞/余量状态。

- 用户纠正本轮复现输入为桌面11111.txt，终止本轮自行启动的234点诊断PID3088，改用11111原完整流程；失败日志仍是234点，但不以其结果冒充用户指定749点复现。

- 完成：11111原主窗口749点Top-1以10mm/一轮QP运行1998.876秒，输出4528点；最终/APF独立0.00025rad分别137279/148061样本，0碰撞/0限位，最终速度/离散加速度通过。最小最终节点间距0.723814mm，10mm未达，partial保留。Release8/8、Debug6/6；两配置构建/启动通过。外层build/Release/bin EXE已备份同步，SHA256 AF3BCDDE3A56E7229B0938AC9024DBA7DE11350AB606659CE918161B6CEF67D3；成果results/11111_Top1_10mm_20261008，详见audit。

## 2026-10-11 播放中的碰撞距离线丢失

- main/33fac4a，开始时工作树干净。用户确认丢失的是主视图最近点连线/数字。
- 播放独立快照仍逐源点做碰撞检查；主视图 ProjectScene 仅使用 SDK Continuous，SDK 另有 ManualNearest / PendingManualRefresh。先以原 MainWindow 验证启用检测后的最近点状态，再在公共场景呈现层按显式可见/距离请求调用 SDK 最近点入口。
- 不修改 SDK、APF/CDF、关节符号、turn、轨迹或播放逐点验收；不自动开启用户关闭的碰撞检测，不恢复每个源点重复主视图查询。
- 验证：原窗口现有4528点CDF产物，启用/关闭检测的基线与修复对照，距离线实际提交及数值变化、Release/Debug构建、相关回归、两入口部署核对。

- 完成：主视图按显式可见最近点请求调用SDK ManualNearest；原MainWindow 4528点验证全部476帧都有距离线，较原Continuous的0帧恢复；Release/Debug构建、各4/4相关回归通过，用户EXE同步并备份。用户2050点额外原窗口播放出现呈现等待，未冒充完整通过。
- 新增核验发现：用户更正的桌面ABB4600_urdf_apf_cdf_qp_trajectory.txt在0.00025rad通过、0.0001rad检出1178→1179段（516.366195秒）机器人—工件碰撞，独立见证姿态复现。结果见外层results/trajectory_check_20261011；仅修显示与诊断输出，未改规划/过滤或原轨迹。交付EXE SHA=B81B637944F7B5044D30D92DAFB0785EAEF51D72ED45E16DF72C72DA2DD21C44。


## 2026-10-12 轨迹变换与逐点最近距离图

- main/33fac4a；保留上轮ProjectScene、原窗口诊断、Headless及文档未提交改动。新增Trajectory Transform第三页和Basic最近距离图，不改变APF/CDF、检测过滤和播放呈现节奏。
- 领域：ProjectMotionPlanning复用导入，整体世界系左乘刚体变换（Rz Ry Rx，先旋转后平移），矩阵mm+秒导出；MotionPlanningCore增加按id删除。变换保留时间/速度并失效旧关节解，经mutateProject通知视图。
- 最近距离以最近一次播放的独立碰撞快照和逐源点runtime角度为准；播放结束后后台查询，不堵塞播放，不以渲染帧替代源点。碰撞为零几何间距、查询错误NaN、注明检测器及完整/部分记录；不当作段间连续碰撞认证。
- 验证领域旋转顺序/单位/元数据/矩阵往返/删除、真实场景距离；Release/Debug构建与原MainWindow导入变换导出删除/短轨迹播放曲线。部署前备份、核对SDK DLL，不终止用户进程。

- 完成：新增第三页和距离图/导出，修复矩阵后置时间戳错位；Release/Debug构建及各4/4回归通过，原MainWindow两格式变换和20点距离图验证通过。真实碰撞返回0mm/collision，不伪造负穿透值。详情和证据见本日audit；交付SHA=034CABF644137E43A86CA01107924AE471FBA77199CFA78333ACA194980773D9，两个Release入口已同步，旧EXE已备份。

## 2026-10-12 CDF FANUC LS 导出

- 在 main/33fac4a 上保留既有轨迹变换、距离图、场景最近点和诊断改动。新增领域 FanucTrajectoryExport 和 CDF 按钮，按桌面 JHA.LS 序列化完整最终 TCP。
- 固定 UF2/UT1、L/400mm/sec/CNT100、模板 N U T 及零 turn；不把 ABB 构型冒充 FANUC 构型，不改变 APF/QP。实际独立 FK、m→mm、RzRyRx WPR、原子保存归相应领域与适配层。
- 验证计划：Release/Debug 构建与导出回归、原 MainWindow 点击导出全部点/FK 对比、两入口部署，未进行目标控制器编译。

- 完成：CDF 新增“导出发那科机器人程序（.LS）...”按钮，全部最终关节点经独立实际 TCP FK 转为 XYZ/WPR；模板 UF2/UT1、400mm/sec、CNT100、N U T/零圈数，ASCII/CRLF/原子保存。新增领域 API，未增加依赖或修改原优化/导出语义。
- Release/Debug 主程序和新回归目标构建通过，两配置各 3/3 导出/导入回归通过。正常启动原 MainWindow 的 CDF 可见按钮导出两配置均通过：4528/4528 点，位置最大误差 8.35237e-7 m、姿态 1.81050e-5 rad（三位小数序列化）；截图与 JSON 为外层 build/fanuc-main-visible-{release,debug}.*。
- 原诊断脚本可能点击隐藏页面；本轮诊断增加真实工作台 action 和按钮可见性核验。显式 --profile-project 路径下工作台动作禁用，最终改用正常启动加载已有 ABB 项目的原窗口验证，没有改变产品工作台切换逻辑。
- 发布至 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe 与外层 build/Release/bin/RobotQtViewerrx64.exe，旧 EXE 保存在 build/backups/fanuc-export-20261012；两入口 SHA256=8C783D19E6630D737689F74AA1285E34D4CA774559315BF18137AEED39AAB42B。桌面 JHA.LS 原模板未修改。
- 使用限制：当前为笛卡尔目标格式导出，FANUC CONFIG/turn 沿用模板而非机器人逆解；UF2/UT1 需匹配实际场景，控制器 L/CNT 路径与原 CDF 关节插值不同。未宣称 FANUC 编译或实机无碰撞已验证，详见 docs/cdf_fanuc_export.md。


## 2026-10-12 Project Assembly / Move Base 无响应修复

- 根因：装配工作台的 panel resolver 随选择在 statusPanel/taskWidget 间切换；初始化只注册 statusPanel，后来 setCurrentWidget(taskWidget) 报 not contained in stack。未纳入布局的子控件仍可能 isVisible=true，不能据此判定可操作。此外 MoveRobotBase/MoveSceneObject 未进入显式打开/置前任务 dock 的菜单分支。
- MainWindow::updateTaskPanel 在选择前按需注册 resolver 返回的新页面；Move Base/Move Object 复用已有任务面板显示入口。SceneExplorerWorkbenchLifecycle 创建备用任务页后先隐藏，避免未注册时覆盖状态页。变换草稿、Apply 的 SceneEntityWorkflow、文档 dirty、预览和 Cancel 机制不变；无依赖或领域 API 改动。
- 新增显式 --profile-move-base <report.json> 原 MainWindow 诊断（沿用 PlaybackWindowProfile 文件），直接触发真实树右键菜单，不使用简化窗口。断言必须是 QStackedWidget 已注册且当前页面，并检查几何尺寸和可见性；覆盖未选中、右键、隐藏 dock 恢复、100mm 预览/取消/Apply/恢复、Motion Planning 往返、burnner 和 ABB 切换。测试不保存项目文件。
- Release/Debug 主程序构建通过；两配置原窗口各 17/17 检查通过，截图确认编辑器完整布局，日志无 not contained in stack。两配置架构边界、场景树投影、工作台生命周期、文档事务、插件回归各 5/5 通过。
- 证据：外层 build/move-base-{baseline,release,debug}.json、.json.png、.log、.err.log；复现脚本 build/test_move_base.ps1；构建及 ctest 日志在 C:/b/rs105-merge/move-base-*.log。
- 已同步 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe 与外层 build/Release/bin/RobotQtViewerrx64.exe；SHA256=8FA8DA16A2981930C8278062263D20C1E2983549A5F3DB78041935A38BD5C9B3。旧 EXE 保存在外层 build/backups/move-base-before-20261012，未终止用户进程。原有 FANUC/CDF/轨迹等未提交修改均保留。


## 2026-10-12 外部 Freeform APF/QP 脚本（已完成）

- 用户要求在项目外创建脚本，输入 freeform_trajectory_paq.txt 与毫米制 A.STL，复用当前 ABB4600/实际喷枪 TCP、全逆解、Top-1、APF/一轮QP，输出原主窗口动画、距离图、FANUC和关节TXT。最新姿态要求是工具局部 Z 轴正/负90度择可行方向，撤销此前Rx/Ry取反。
- 独立目录 C:/Users/14390/Desktop/Freeform_APF_QP，未修改本轮主项目业务源代码。通过CMAKE_PROJECT_INCLUDE外部钩子添加FreeformPlanner目标，Release/Debug构建通过，不部署或替换SDK DLL。原主程序只通过独立SMROBOT_CONFIG_ROOT载入场景。
- 确认 −90° 的2400/2400点均有8个有效解（19200），+90°为0/2400；全局Top-30及Top-1已生成。实际FK复验Top-1最大位置9.9278e-7m、角度7.4012e-7rad，Release/Debug一致。数值多起点沿用GUI搜索范围，不宣称数学穷尽。
- 工件0.001缩放；刚体对齐约平移(1.2,.6,.6)m、RPY(-90,0,-90)度。2000个表面点间距0.9855至1.0023mm；400个引入/引出点约100mm，全部保留。
- 完成13770点、868个APF区间、一轮QP。原0.00025rad验收遗漏7个加密碰撞样本；外部DenseRepair利用URDF运动位移界和实际距离查询，找到13个需要补修的区间，全部以幅度0.0001rad的正弦平方扰动修复（91个邻近点），原业务源码未修改。最终整段自适应验收通过，0.0001rad密采样2556864状态、0碰撞，项目限位/速度/离散加速度违规均0。
- 用户确认无碰撞即可，10mm为可选软目标。节点最小间距0.596690mm；整段几何位移界保守下界0.0100098mm。起终完整TCP位置/角度误差1.57e-9m/1.01e-9rad。中间位姿未锁定，最大位置偏移99.3348mm、法向偏差68.3798度，不能称为严格保法向/喷距的工艺轨迹。验收范围是ABB4600含喷枪对A.STL及关节直线插值，不含FANUC控制器L/CNT插补。
- 最终TXT/LS/距离图/分阶段对比/原MainWindow动画均在外部output/final。LS全部13770点回读FK通过；原窗口播放13563ms，13770/13770点完整、碰撞/无效0，P95帧间隔49ms，录像截图核对通过。最新Release/Debug入口构建及Top-1实际FK一致通过；完成缓存复用测试，默认run.cmd不重复优化。CMAKE_PROJECT_INCLUDE已清除，原主GUI哈希保持8FA8DA16…C9B3，保留此前所有未提交修改。
- GUI兼容副本仅将工件/检测器id映射为默认burnner/cdf_abb4600_burnner_detector，源仍为A.STL，几何/变换/过滤保持不变；已在原MainWindow启动检查2机器人/1检测器和工件显示。

## 2026-10-13 Freeform 内表面喷射方向修正（进行中）

- 用户指出上一版喷枪从外侧喷射。实际drawSprayRange/sprayMeasurement使用TCP +Z，而输入+Z与A.STL内侧面法向一致、背离最近表面；此前绕Z的切向修正未翻转Z。保留已校准工件刚体位置，改用R_original*Rz(±90)*Rx(180)，24点诊断+90全部可解、−90无法完整覆盖。
- 所有者ProjectMotionPlanning增加可选TCP工艺可行性回调，空回调保留当前UI行为；APF、平滑、QP的统一有序路径验收执行回调。外部脚本加入实际网格内侧/喷射朝向条件，并在密采样及最终验收重查。主场景/动画仍用原MainWindow。
- 先保留上一版完整输出及输入快照，重跑全逆解/Top1/APF/一轮QP；无碰撞是硬条件，10mm可选。最终还需确认扫描区喷枪位于内侧且+Z朝向内表面，输出TXT/LS/距离图及原窗口动画。本轮尚未取得新结果。


## 2026-10-13 轨迹点姿态预览（进行中）

- 用户优先要求 Basic Planning 新增“显示轨迹点姿态”。沿用会话控制器和 typed viewport port：widget发意图、controller保存会话开关、ProjectScene接收完整位姿、私有overlay pass批量绘制RGB局部XYZ细线。默认关闭，独立于球状点显示；轴长按范围限定15至40mm。
- 不改原轨迹/逆解/优化，不持久化临时预览；追加showOrientations默认false兼容现有调用。基于SDK RenderCore Geometry(GL_LINES)缓存一次上传/一次draw，避免数千debug.drawLine的分配和draw开销。
- 外部Freeform规划按用户要求保留机器人基座，暂不继续失败规划；最新同名轨迹为世界坐标矩阵格式，需要另行输出不可达点报告。本轮先完成主项目姿态显示。
- 验证：Release/Debug主程序构建，原MainWindow导入矩阵和四元数轨迹、显示/隐藏/球点独立性及大量点预览。旧EXE/SDK已备份于外层build/backups/trajectory-orientations-20261013。


- 本轮姿态显示已完成：Basic Planning 的“显示轨迹点姿态”默认关闭，RGB对应完整位姿的局部XYZ；15~40mm自适应轴长，1像素细线，独立于球状点开关。开关仅更新覆盖层，不重建轨迹表；轨迹替换/变换/删除走现有refresh通知，ProjectScene统一管理预览并缓存单个GL_LINES Geometry。私有pass未加入安装头文件，SDK/DLL未变；应用层typed viewport port追加默认false参数，旧源码调用保持默认行为。
- Release/Debug主程序构建通过，Release兼容回归RobotQtViewerConfigurationTabsSmoke 1/1通过；原MainWindow实测最新矩阵7056点（两配置）及12列四元数7056点（Release），三项均RGB显示通过、关球点姿态独立显示通过、取消后framebuffer逐像素恢复通过。测试禁用诊断窗口鼠标输入避免用户操作干扰，不使用简化窗口。证据在外层build/orientations-{matrix-release,quaternion-release,matrix-debug}.json及截图/日志。
- 编译部署覆盖过的Release lib_coacd.dll/tbb12.dll已从本轮备份恢复，Debug未发现部署差异。CMAKE_PROJECT_INCLUDE外部钩子已清除。用户正在运行外层build/Release/bin/RobotQtViewerrx64.exe（旧版），保留不终止/覆盖；新版在C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe及外层build/Release/bin/RobotQtViewer_TrajectoryAxesrx64.exe，SHA256=3898485754431EF1380822AC0F23F01D57CD1E24D0D81F20F9DDA03CCA701465。交付清单为外层build/trajectory-orientations-delivery.json。


## 2026-10-13 轨迹点局部轴姿态旋转（已完成）

- 在现有未提交姿态轴预览改动上继续，保留原世界系整体变换。在Trajectory Transform的“变换轨迹”对话框增加模式选择及局部XYZ轴、角度字段。位置保持不变，使用R_i右乘所选轴旋转；时间/工艺属性不变，旧IK及图结果按原领域规则失效。
- 所有者为ProjectMotionPlanning/TrajectoryControlPointEditing，新增typed轴枚举及旋转API；Widget只收集草稿，controller通过commitMotionPlanUpdate提交并通知原轨迹视图/姿态覆盖层。无SDK、依赖或项目格式变更。
- 验证领域局部/世界旋转区别、局部轴不变、元数据和错误原子性；Release/Debug构建及原MainWindow导入-局部旋转-导出回读（XYZ、正负角及累计旋转），保留旧整体变换回归。备份在外层build/backups/trajectory-local-rotation-20261013。

- 本轮局部轴旋转已完成：Trajectory Transform → 变换轨迹 → 只旋转姿态（各点局部坐标），可选红/绿/蓝对应的X/Y/Z及正负角度。使用R_i * R_axis(theta)，各点位置保持原值；时间及已存速度元数据不变，成功后清除旧关节逆解、全逆解/分层图及关联CDF初始解。原世界坐标整体变换接口和默认模式保持。
- Release/Debug 主程序和领域回归构建通过，两配置各通过ProcessPoseImport、Architecture Boundary、ConfigurationTabs三项检查。领域覆盖三个不同姿态的XYZ±90/450度、反向恢复、轴/位置/元数据不变及错误原子性。原MainWindow实测7056点矩阵Z+90（Release/Debug），四元数样例X+90、Y−90及旧整体世界变换（Release），五项导入-真实弹窗-变换-导出回读-删除检查全部通过；位置最大回读误差4.62e-16m、姿态矩阵误差2.73e-15、时间误差0。截图核对真实轴下拉/角度/帮助文字和更新后的RGB姿态。
- 初次诊断删除断言错误地要求项目所有轨迹清空，现改为仅断言所选导入轨迹被删除（私有测试项目已有其他轨迹）；第一次重跑被文件覆盖确认阻塞，仅关闭本轮带profile参数的测试进程，后续使用独立输出名通过。没有修改生产删除行为或关闭用户程序。
- 原SDK DLL部署覆盖已恢复本轮初始备份，Release/Debug全部DLL哈希与初始相同；外层Release已备份两项不一致依赖并同步已验证的Release运行库。C++七文件CRLF及主仓/两个子仓diff --check通过。原有未提交姿态显示与其他业务改动保留。
- 新版入口：C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe及外层build/Release/bin/RobotQtViewerrx64.exe、RobotQtViewer_LocalRotationrx64.exe；SHA256=AA1E0C97D78C470FE8D476FFB112C81F7B0DBAE0173FD1EB8ACF8AF07B2C2143。外层旧主EXE已备份，本轮交付清单build/trajectory-local-rotation-delivery.json；证据为build/local-rotation-{z-release-verified,x-release,y-release,world-release,z-debug}.json及截图/日志。未改用户桌面轨迹输入。

## 2026-10-13 STL 导入单位选择（已完成）

- 用户要求选完STL文件后选择米/毫米。UI对话框属于ProjectAssemblyDialogService，MainWindow只编排选择；SceneEntityWorkflowController在同一项目mutation内设置SceneObjectDesc.visualScale/collisionScale为1或0.001，SDK和项目格式不变。取消不修改项目；非STL保持原行为，旧接口调用默认米。
- 修改前备份于外层build/backups/stl-import-units-20261013，保留现有姿态/轨迹变换等未提交修改。验证米/毫米、保存回读、取消和失败回滚、原MainWindow实际导入；Release/Debug编译及相关回归，恢复部署覆盖的原SDK DLL。

- STL单位功能已完成：Import Object选完.stl/.STL文件后弹出“选择STL模型单位”，可选米(m)或毫米(mm)，默认米。毫米统一按0.001换算，取消不新增对象。非STL仍直接导入；旧workflow调用保留默认米，应用静态库增加typed SceneObjectImportUnit参数，未修改预编译SDK、依赖或项目格式。
- 单位转换由SceneEntityWorkflowController同一原子项目mutation写入已有SceneObjectDesc.visualScale/collisionScale，显示与碰撞采用同一比例；之后沿用viewport reload、失败snapshot restore、dirty和视图通知。没有改写STL顶点源文件。
- Release/Debug主程序及RobotQtViewer-RibbonPcdSmoke构建通过，两配置各通过Architecture Boundary和导入回归（米/毫米双scale、项目保存读取、恢复导入快照、非法单位不修改项目、默认调用）。原MainWindow两配置均完整验证真实Import Object文件选择→取消/米/毫米对话框→视口重载→Save Project As并用SDK回读：两个单位scale正确、取消不增加对象、只新增2对象全部通过；截图核对两项选择和解释文字。使用私有配置副本保留完整ABB/A.STL场景，只移除无关轨迹表，未使用简化窗口或改用户项目。
- 证据：外层build/stl-units-{release,debug}.json、.unit0.png/.unit1.png、.png、.{before,cancelled,after}.sys.json、构建/运行日志。九个源码文件CRLF及git diff --check通过；构建部署覆盖的SDK DLL已恢复，Release/Debug全部DLL与本轮初始备份哈希一致。外层Release两项不一致依赖（lib_coacd.dll/tbb12.dll）先备份再同步已验证C:/b运行库；旧程序备份于build/backups/stl-import-units-20261013，未终止用户运行程序。
- 更新C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe及外层build/Release/bin/RobotQtViewerrx64.exe、RobotQtViewer_StlUnitsrx64.exe；SHA256=D1D96AF925A9A4D9F8CEF6DD9A299DA9B6F5D917FC2A6A2C51499A7BBA66587E。保留此前姿态轴/局部姿态变换等所有未提交改动；交付清单build/stl-import-units-delivery.json。


## 2026-10-14 内表面喷涂姿态引导接入 CDF 按钮（已完成）

- 当前 main；保留根 PlaybackWindowProfile、规划子仓姿态变换和 workbench 未提交修改。用户要求把 ryh 外部全 Top-1 后的姿态引导、安全节点选择、APF/一轮QP、局部定时及完整复验接入同一优化按钮。
- 所有者 ProjectMotionPlanning：纯 C++ 共享规划服务，不引入 Python/Qt 领域依赖；GUI 仅发起请求、呈现阶段，外部入口调用相同服务。不加载旧最终轨迹冒充重算。
- 以校准 profile 描述工件世界网格、长轴/中心、工具几何和运动上界；使用当前实际 FK、完整检测器。profile 与场景模型/基座/工件姿态不一致时拒绝执行，普通无 profile 场景沿用原算法。
- 保留原 Cartesian 输入和起终完整位姿；姿态/喷距调整为显式勾选模式，100mm 为原参考硬走廊，碰撞和喷射方向为硬验收。10mm 仍是普通模式默认；本次 profile 明示无碰撞/0.1mm软余量，不以达到10mm标成功。
- 验证：Release/Debug 编译与算法回归；原 MainWindow 真实按钮进行 ryh Top-1 全流程，比较共享脚本路径、引导及最终密验收；部署前后核对 DLL。预计不超过20个源文件，无新依赖或SDK变更。

- 已完成纯C++共享服务、GUI勾选/阶段/报告、外部guided入口及严格缓存。原MainWindow真实ryh全IK/Top1/按钮完整通过，7362点、2088538密状态碰撞/方向违规0、最大原TCP偏移91.255939mm；最新引擎独立复验和原窗口最终播放通过。两配置各5项回归，42个DLL/配置恢复初始SHA，CMake临时注入清除。
- 交付EXE SHA256 D3DAECAF073938C9F1EC663476882E033D65527BE48A89B61812A2450345CF71，C:/b原Release及外层build/Release/bin均同步。外部output/final为共享流程结果，旧数据完整备份；completed.json透明区分早期共享服务计算和最新binary独立验证，不冒充最新QP重算。详见本日audit、docs/cdf_inner_surface_guidance.md及build/spray-guided-button-delivery.json。

## 2026-10-14 Trajectory Transform 机器人程序导出（已完成）

- 用户要求脚本原窗口的Trajectory Transform支持当前导入/变换的new.txt位姿直接导出ABB MOD/FANUC LS，沿用CDF模板。所有者ProjectMotionPlanning新增显式Cartesian导出接口，复用格式化代码；GUI只选择计划和原子保存，不依赖全IK/QP或FK，不修改场景/原轨迹。
- 保留所有前轮未提交改动；预计11个源码文件，无新依赖/SDK变更。验证原CDF导出回归及Cartesian原/变换/奇异姿态/非法输入；Release/Debug构建和原MainWindow真实导入/变换/导出/回读。用户原主程序仍在运行，保留并采用独立输出目录编译，再提供新入口给脚本。

- 已新增“导出 ABB 程序（.mod）...”和“导出发那科程序（.LS）...”两个按钮，直接导出当前Cartesian目标，不依赖全IK/CDF结果。旧CDF导出与新接口共用模板、单位与姿态转换；原窗口验证new.txt 7362点原/局部旋转/世界变换、Debug全点及四元数样例，全回读通过且不改变源计划。两配置各4项回归、11源码ASCII/CRLF/三仓diff检查通过。
- run_ryh.cmd已指向隔离部署的最新版原MainWindow；原结果经最新Native 2088538状态复验、缓存回放通过，保留计算来源，未重算QP。正常Cb/外层Release入口及TransformExport别名也同步，SHA256=F25787AC9F4406CCCFEDBF1308F86F89E3FDEE531C1712612858DF3BCB99E211。原用户窗口保留至其自行退出，SDK每配置42个DLL与初始一致，CMake注入已清除。交付build/transform-program-export-delivery.json及外部README。

## 2026-10-15 ryh 位置贴合、关节平滑与安全减点（已完成）

- 保留三仓全部现有未提交修改；仅改共享 ProjectMotionPlanning 喷涂引导/验收与外部脚本入口，不改 SDK 或导出模板。原 ryh 2600 点，旧结果7362点、最大TCP偏移91.256mm。
- 首先统计原结果；优先原位置调姿候选，增加位置偏差/关节切换代价及连续平滑，然后自适应误差约束减点，所有新关节连接重新碰撞/工艺/原位置走廊验收。输出点数变化必须保留有序原站点对应，不能索引错配。
- 构建Release/Debug与领域回归；用原ryh/Top1在独立输出重算并密验收、对比旧/新位置偏差和光滑度、LS相邻位置间距；原MainWindow复核，不使用简化窗口，不关闭用户进程。现有输出先保留，新结果完整验收后才发布。无新依赖，预计低于15个源码/脚本文件。


- 已实现位置优先调姿、实际FK局部IK/合法turn、二阶束搜索、5/10/20/30mm局部APF走廊、安全误差约束减点与2mm近点合并、全区间喷射方向补修和FANUC等价WPR连续展开。共享服务/脚本均已更新，原全逆解Top-M规则保持。
- 新final3165点（旧7362），原站点实际FK RMS22.0mm（旧70.3mm），最大偏移80.132mm；LS最小点距2.045866mm，无小于2mm点对。同进度整体平滑指标改善3.45%，不声称每关节都改善。
- 1891158密状态碰撞/方向/限位/离散动态违规0，运动界正分离下界0.01000017mm；61度入射门限未放宽。原MainWindow3165点完整播放和录像通过；Release/Debug构建、各8项回归及最新各2项复验通过。SDK42 DLL/配置与初始一致，CMake临时hook清除。
- 实算使用本轮新引导/APF-QP中间检查点，最新共享服务完成减点/补修与完整复验，来源透明，不拿旧final作为新几何。默认run_ryh.cmd已验收缓存，--force才重算；原用户入口不覆盖，新ryh-quality-bin独立部署。详见cdf_inner_surface_guidance.md与外层build/ryh-quality-delivery.json。FANUC控制器编译/L-CNT插补未验。


## 2026-10-16 ryh 往返扫掠段两端覆盖保留（进行中）

- 用户要求保留原轨迹每段外伸覆盖并保证无碰撞。现有2600站点仍在，但表面投影及100mm有序走廊允许纵向缩短，仅整轨迹两端锁定。所有者为共享ProjectMotionPlanning/SprayGuidedRepair。
- 保持原Cartesian/Top-1、机器人/工件/TCP与全部碰撞过滤不变；增加沿工件长轴的覆盖约束，候选生成、APF/QP、减点与独立验收共同执行。姿态仍可调整。
- 修改共享服务私有头/源、回归与外部入口；无公开API/依赖/SDK变更。新结果独立计算验收后替换，旧结果及旧运行目录保留。验证Release/Debug构建、回归、原ryh实际完整碰撞与覆盖误差。

- 补充：严格覆盖带的APF更新需主动投影，单纯bool拒绝容易局部停滞。ProjectCdfQpRepairOptions新增可选tcpStateProjection源接口（默认空），由私有ApfGuidance传递到提议更新与平滑；共享服务用实际FK的轴向Jacobian最小关节修正，保持关节限位，随后仍执行原碰撞/整段工艺检查。无预编译SDK/DLL接口修改。涉及私有APF/CDF/密补修与定向回归，预计12个代码/脚本文件以内。

- 用户进一步允许大幅调姿、优先原TCP位置。固定位置探查确认原先主要遗漏绕工件长轴旋转喷射方向（保持长枪插入轴）的候选；9个困难站点均可原位调姿避碰，保留61度内表面入射规则。共享trackingDesign增加该姿态自由度，在放宽位置前搜索±15至±90度；后续仍验整段碰撞与轴向覆盖，不把安全节点当作完整安全路径。

- 诊断azimuth_v4：引导节点RMS位置偏差2.065mm，仅5点超过1mm；但506–509与2092–2095站点出现约85–119度姿态交接，12轮之前8轮惰性检验仍有4条未连通，未作为成功轨迹发布。进一步在断开连接邻域扩充方位角/小横移/俯仰组合，并对不同探查曲线的合法等价turn按各层参考对齐；azimuth_v5正在从原ryh/原全局Top1完整计算。旧output/final尚未替换。


## 2026-10-16 完整覆盖结果（已完成）

使用未改动的原桌面ryh.txt、固定ABB4600/长喷枪/A.STL、原全局Top-1，本轮最新Native从调姿引导、APF到一轮QP及最终验收完整重算成功，没有复用旧final或旧APF/QP几何。原主窗口完整播放3432/3432点，碰撞/无效状态0、距离覆盖层302/302帧有效；默认脚本缓存核验通过。原GUI优化按钮链接同一共享服务，但本轮未再次执行其长时间完整重算。

| 指标 | 之前3165点结果 | 本轮3432点结果 |
|---|---:|---:|
| 全部2600原站点实际FK位置RMS偏差 | 21.981497mm | 4.950296mm |
| 原始往返端部最大轴向回缩 | 63.230158mm | 1.429140mm |
| 全部原站点最大有序轴向误差 | 63.230158mm | 1.908945mm |
| 全程最大TCP位置偏移 | 80.132mm | 55.512121mm |

实际调姿引导2594/2600点位置误差小于1mm，但不能把该引导指标当成最终轨迹误差。最终仍有少数较大的横向避让，不能声称每个点位置保持不变。原2600站点全部参与实际关节插值FK检查，轴向覆盖缺省2mm；100个远端/99个近端折返点以及整条起终源范围均核对，不再用站点数或整轨迹端点保持代替覆盖验证。

最终从8172计算节点安全减到3432执行节点，独立0.0001rad检查2268062个状态，碰撞/内表面喷射方向/限位/离散速度与加速度违规0。运动界保守连续分离下界0.010043mm，节点最小距离0.113540mm；不是10mm安全距离。61度内表面入射门限保持，实测最大60.993906度。完整起终位姿误差约9.29e-12m/1.52e-11rad，重定时总长824.705s。

为覆盖端部，大幅调姿增加了部分关节运动。相同40000源进度网格的关节二阶差分RMS由0.019393度增至0.022032度，不能声称本轮比旧结果整体更平滑。相对本轮APF种子，碰撞约束平滑降低了关节长度和弯曲代价；QP仅运行一轮，任何未通过碰撞/工艺检查的步长均不采用。模板LS最小相邻点距1.966112mm，有1对低于工程合并目标2mm；2mm不是用户提供的控制器硬门限，安全/覆盖/姿态约束优先，不能声称所有点间距都超过2mm。

新结果已发布到Freeform_APF_QP/runs/ryh_20261014/output/final，旧3165点final完整归档。run_ryh.cmd默认核验并打开新结果，--force按同一共享方法重算，仍耗时较长。入口为C:/b/rs105-merge/Release/ryh-coverage-projection-bin/RobotQtViewerrx64.exe；没有替换用户其他运行窗口。最终关节TXT、FANUC LS、原11111格式TCP矩阵、最小距离图、原/旧/新独立覆盖图、端部对比和原窗口动画均已生成。

Release/Debug主程序和Native构建通过，各4项APF/SprayGuidance/TrajectoryQuality/LayeredIkGraph回归通过；SDK每配置42个DLL保持初始哈希、临时CMake注入清除，无新依赖。当前项目静态源码接口增加可选tcpStateProjection，普通无回调调用保持原逻辑；未改预编译SDK。三仓其他未提交修改保留。

验收范围仍为当前ABB4600含长喷枪对固定A.STL的关节直线插值、刚体网格和数值保护量。FANUC LS仅作格式/位姿回读，ROBOGUIDE控制器的构型、工具/工件标定及L/CNT插补碰撞未验证，不可把此证书用于另一机器人工作站。详见外层build/ryh-coverage-delivery.json及最终目录内验证文件。

## 2026-10-16 ryh1 新轨迹兼容与原主窗口完整流程（进行中）

用户要求使用桌面ryh1.txt，实际通过run_ryh场景、Basic Planning原始导入、全逆解、全局Top-1和CDF按钮得到无碰撞路径。保留此前所有未提交代码、旧ryh及已验证final；本轮优先改外部输入/任务编排，不先改共享规划算法。

新文件为2400个刚体矩阵，位置mm、时间严格递增，范围X=1100.171至2145.508mm，含比旧轨迹更长的外伸段；原位置/姿态均不改。独立Native全IK2400/2400，每层8解、共19200，实际FK位置/姿态误差均小于1e-6，原全连接分层图全局Top-1 cost=712.190545575791。

外部新增trajectory_job.py，按源内容SHA建立隔离runs/ryh1_95c130c7dd8d快照，复用已标定相同几何/位姿/过滤，scene仅重定位输入路径。run_ryh.py支持--trajectory和--scene-only，覆盖验收不再写死2600点；无Top-1时自动全IK/分层图，只有完整验收成功才切换默认活动任务。旧缓存仅允许已知旧launcher哈希的兼容迁移，要求引擎/模型/输入/规划库/Top-1及全部输出哈希相同，不冒充重新规划。

原MainWindow PID42844已从原ryh1.txt执行真实导入、全IK和全局Top-1选择（代价一致），目前正在原CDF按钮的姿态引导/后续APF一轮QP。用户原窗口PID18144保持运行。新结果尚未验收，不能称已无碰撞；未重算旧ryh或修改SDK。后续对实际GUI导出结果独立密碰撞、原2400站点覆盖/动态复验和原主窗口播放，通过后再发布与记录。


### ryh1 首轮真实 GUI 失败及有界 APF 扩展（进行中）

原 MainWindow 全流程失败，2811.2s，QP未启动；2400原站点全部恢复于11084点加密参考。Top-1原节点951处碰撞；调姿引导最大位移19.696mm，但143处SE(3)连接尚未连通。APF在加密47至50区间、5/10/20/30mm均失败，失败导出仅作诊断。

共享SprayGuidedRepair原局部走廊写死上限30mm，未使用原100mm总预算的剩余空间。保持窄范围优先，追加50/75mm及剩余预算尝试，局部走廊+最大引导偏移不超过原100mm；不放宽碰撞、61度方向、2mm轴向覆盖或最终验收。仅改共享源，无API/依赖/SDK变化。隔离ryh1-bounded-apf-bin构建Release/Debug，保留PID18144和旧已验证结果。尚未证明新轨迹可行。

进一步定位：原加密参考前170个节点无几何碰撞，但48/49/55/157喷射方向不通过。独立密过程检查首失败source_time=2.353410869s，方向余弦0.939592685小于cos(20度)，不是无法求IK或必须改变末端位置。工件端沿使原横向射线错过有限内表面，APF只有几何距离力及工艺bool拒绝，不能可靠自行生成前后倾姿态。

在后Top-1姿态引导的首次断开连接扩充中，先增加保持位置的±20/30/45/60度轴向倾角与7种方位角组合；第二轮仍断开才扩充原位置/小倾角候选。原全逆解和Top-M分层图不变。新增有限端沿回归确认同TCP位置下倾斜恢复内表面命中、20/61度门限不变。Release/Debug主程序和Native编译通过，各4项相关回归通过，SDK42DLL/配置已恢复初始SHA。当前PID36952原MainWindow v3正在重新执行ryh1全IK/Top-1/CDF，尚未发布成功结果。v2诊断与我方窗口已主动停止，记录于bounded_v2_aborted.json，用户PID18144保留。


### ryh1 主动固定位置调姿修正（进行中）

v3原MainWindow完整运行至1953s仍有146个调姿连接未通，首个5mm局部APF区间失败，已主动终止我方PID36952，未证明更宽走廊失败；二进制/静态库/日志保存在外部before_aim_projection，用户PID18144保留。

根因是APF现有喷射方向判据只有拒绝，没有朝向内表面的主动投影。私有SprayInnerSurface新增保持TCP位置的射线目标候选，按实际内表面最近三角形、最短转动生成姿态，并继续使用原有正侧/20度喷射锥/61度入射判据。共享局部实际模型IK将候选投回合法关节；APF更新投影与后Top-1连接探查共用，之后完整碰撞/路径检查不变。无SDK或公开接口变化。新增有限端沿、姿态合法、TCP不动及外侧拒绝回归；Release/Debug主程序/Native构建及各4项回归已通过。外部Native正在用真实v1失败引导诊断新修正，尚未发布ryh1结果；仍需原主窗口完整重算和独立验收。

进一步诊断显示，只对APF小步做调姿仍会在原关节弦的工艺无效站点停滞，因此共享喷涂服务在APF前增加实际模型笛卡尔连续IK预处理（20mm平移、0.04rad姿态/关节跨度），每个原始站点原时间和原引导关节均保留；只有整条局部探查的节点均通过实际FK、限位、方向和碰撞才采用，失败边仍交APF。该预处理不把安全节点当作整段安全，后续所有连接继续APF/运动界/密检查。已有细分后仅关闭额外固定插入一个样本，仍保留0.04rad自适应细分及0.00025rad各阶段碰撞检查。射线修正沿最短转动二分寻找最小可行转角，并留1度内部角余量，避免轻微越界直接跳20度。Release/Debug主程序、Native和各4项回归通过。v4原MainWindow PID34348正在从原ryh1全IK/Top-1重算；Native PID21016复用旧失败guide仅作定位，2267边完成笛卡尔节点预处理、132边仍交APF，不是最终无碰撞证书。


### ryh1 APF 逃逸场提前退出修复（验收进行中）

2026-10-16：共享ApfLocalPlanner在某个确定性扰动投影失败时直接return false，导致其余关节方向全部未尝试。现改为仅跳过当前场，保留其他逃逸方向。新增固定锚点、圆形障碍物与轴向投影受限的回归，要求轴向扰动失败后仍由横向场连通，Release/Debug APF及Debug四项领域回归通过。完整ryh1无碰撞结果尚未证明；Native诊断明确复用原失败引导而非旧final，用户PID18144及旧已验证输出保留。


### ryh1 中间瓶颈自适应调姿分层图（进行中）

固定TCP首个困难位置(2.018048870,0.599301194,0.564960684)m、源时间2.353410869s，按实际模型探查1105个方位角/俯仰/滚转姿态，686个IK有效，22个同时无碰撞且满足原喷涂判据。证据保存在外部rim_pose_grid，不是完整可执行轨迹。说明原站点之间存在离散姿态瓶颈，单靠原站点姿态图和事后APF可能选错通道。

共享服务仅在Top-1后的喷涂调姿图内，自适应插入首个不合格笛卡尔探查站点，补充同样实际FK/限位/完整碰撞/工艺筛选的姿态候选，再运行原二阶引导选择。原全逆解/Top-M图及全部2400导入站点、顺序、位置参考不改变。补充临时站点来自原笛卡尔轨迹的有序插值，不从关节FK冒充输入。最多四轮加点，之后仍APF、一轮QP和独立最终验收。

同时修复固定位置调姿投影的非单调可行性：完全对准三角形可能超出61度，但部分转角已经满足20度喷射锥；现先沿旋转弧找可行区间，再二分最小转角，原20/61度门限不变。新增相应有限端沿回归，Release/Debug四项领域回归通过。内点姿态连续携带和固定位置局部搜索仍接受全部最终检查。旧final及用户PID18144保留，v4实际GUI失败、v5/v6中止和各Native诊断均有记录；未发布ryh1无碰撞结论。


### ryh1 APF位置投影顺序与原始站点覆盖（验收进行中）

APF扰动原先先做工艺/喷射投影，再恢复TCP走廊；偏离工件内腔的有限提议会在可恢复位置之前被提前拒绝。现改为先恢复普通位置走廊，再做工艺投影，投影后再核对位置；已在走廊内仍必须执行更窄工艺投影。新增定向回归覆盖上述两条路径，原逃逸场失败后继续尝试其他方向的回归保持。Release/Debug四项领域回归通过。

新增可选profile axial_tracking_scope=source_stations，缺省all_states保持旧行为。仅试验配置允许点间连接在原100mm走廊内绕行，全部原始2400站点及往返端部仍逐点验证2mm轴向覆盖；减点新弦也显式查每个原站点。锚点匹配用按X排序的范围查找，避免每个密样本遍历4800锚点。尚未证明该配置可生成完整ryh1轨迹，不能当作完成结果。

原MainWindow v7与Native为重复旧算法；停止我方v7 PID16680并存gui_workflow_v7_aborted.json，Native21992继续计算，原用户PID18144保留。隔离scope运行目录测试失败引导恢复，不替换旧ryh已验证final，不把checkpoint诊断称为原输入完整重算。


### ryh1 喷射余量势场与局部实测（完整验收进行中）

共享APF增加可选signed process clearance，用于前瞻斥力与有限差分梯度；喷涂服务按实际内表面有向距离、20度锥方向余量和61度入射余量计算，不以势场值取代几何碰撞/整段工艺硬检查。普通调用无回调时保持原计算路径。新增“几何无障碍但工艺禁区需要绕行”回归，Release四项回归通过；Debug主程序/Native已构建，最新版复验仍在进行。新增源码options字段tcpStateClearance，未改预编译SDK或依赖。

固定位置优先的强空空间斥力另测在attitude61，未纳入当前算法（实验被撤回）；实际处理保留原边界抑制策略。外部profile新增可选apf_local_corridors_m、guidance_adaptive_passes、guidance_connection_passes，缺省仍为旧5/10/20/30/50/75mm、4轮插站和12轮连接搜索。当前试验process_corridor61从20mm开始，仅保留全部原站点覆盖，不超过原100mm总预算；不改变原全IK/Top-M定义。

第一局部24原站点诊断在20mm走廊连通54至56困难段，safe anchors42至68，并通过APF整段检查和一轮QP；该局部截取终点不等于原全局终点，最终位姿检查如期拒绝，不能作为完整最终轨迹。80度入射试验也只用于诊断，没有纳入当前61度配置。

当前Native用全2400原输入+原失败GUI-v4引导检查点重算完整APF/QP；原MainWindow v10 PID26256从原ryh1导入、全IK、原全局Top1、生产CDF按钮重新计算，不使用引导检查点。两者结果分别记录，不声称相同几何。原用户PID18144及旧ryh输出保持。尚无ryh1最终证书或可执行final。


### 2026-10-17 ryh1 区间独立走廊与中断后恢复（验收进行中）

CdfQpTrajectoryRepair原先某区间失败后由喷涂包装层从头放宽整条轨迹，丢失已验证局部连接并重复密查询。现对带喷射余量回调的每个区间按20/30/50/75mm及剩余全局预算独立尝试，各自再做三档安全锚点扩展；只保存经过全段几何/工艺检查的替换，边界关节与时间/点数保持。普通APF没有新回调时仍只用原配置走廊。全局100mm预算、全部原站点2mm轴向覆盖和最终原位置/FK检查保持。当前profile可只运行一次最大总预算的服务，局部先窄后宽，不因一段困难重复整条路径。

本轮完整实测又发现1039及1187附近的重复边沿区间均耗尽20mm场搜索后在30mm成功。新增仅限本次repair的空间邻域搜索顺序提示：在120mm邻域优先尝试最近一次完整验证成功的走廊宽度，其余宽度原样保留为回退，不复用旧关节路径或碰撞结论。成功替换依旧完整检查；不是缓存未验收路径。隔离regional-bands版本构建/运行验证中，旧local-bands计算及原MainWindow v11保留。仍无ryh1完整final，尚不切换用户脚本默认结果。

前轮被外部中断，恢复时PID41504、18076、26256及原用户PID18144均已不在，不能据此推断任何轨迹成功；部分日志与检查点保留。原ryh1哈希未变。Release主程序/Native已编译，四项领域回归通过，隔离ryh1-local-bands-bin启动Native28008（完整原输入+先前失败引导，重新APF/QP）及原MainWindow v11 PID16872（原始导入、全IK、全局Top1、生产CDF按钮，无引导检查点）。Debug构建/复验正在执行。尚无ryh1最终验收，旧ryh结果与默认活动任务保持。


### ryh1 喷射势场归一化试验撤回（完整验收进行中）

局部24点诊断曾将喷射方向余量乘40以增强势场；原方法在20mm局部走廊连接成功，而归一化试验耗尽20mm搜索后需30mm，未改善瓶颈，已撤回三个相关源码/测试改动。局部诊断之后完成APF/单轮QP，但因截断终点与原全局终点不同而被最终端点检查拒绝；不将其作为完整失败证明或可执行final。试验进程已自然退出，不重复终止已消失PID39760。SprayGuidedRepair.cpp已与试验前备份逐字节一致。当前regional-bands Native20376与原MainWindow33672仍采用未含归一化的隔离二进制继续运行，不重启长任务。

编译目录的共享静态库曾被试验构建覆盖，正在隔离重建撤回后的源码；运行中regional二进制的计算版本按regional_bands_identity.json与before_process_scale冻结库哈希记录，不能以当前全局库哈希冒充。原输入、模型、碰撞过滤及旧ryh已验收结果不变，ryh1仍未取得完整final。


### ryh1 连续步长对照与外部导出核对（完整验收进行中）

另对相同前24源站点进行0.01rad笛卡尔连续步长诊断：内部规划节点从109增至289，但未连通原边仍为2，初始不合格区间仍为2，未消除瓶颈，已停止我方隔离PID16528并撤回可选step参数，SprayGuidedRepair.cpp恢复为采用版本。不是新的完整轨迹或不可行性证明，两条既有长任务未中断。

外部run_ryh.py新增按任务读取冻结runtime.json的可选选择，核对实际EXE/DLL哈希，规划静态库身份取冻结计算记录，避免后续试验build覆盖全局lib后错误标记旧程序。ryh1尚无完整final，runtime_candidate.json仅准备未激活，默认活动任务未改。finish_gui_run.py改用同一冻结库身份。verify_exports.py增加实际FK生成的11111矩阵格式TCP导出和回读验证；原站点位置误差改用Native实际关节插值FK验收值，TCP直线/Slerp估计单独标注，避免把估算值误称实际原站点误差。对旧ryh结果的独立scratch拷贝进行格式回归通过，没有改变其final，也不作为ryh1规划成功证据。

撤回势场归一化后的隔离regional-rebuild Release/Debug主程序/Native构建通过，各四项领域回归通过。SDK DLL缺失部署检查已增加缺文件恢复，42DLL/配置SHA核对通过。当前仍保留regional-bands Native20376、原MainWindow33672，任务尚在运行，未交付无碰撞结论。


### 2026-10-17 ryh1 必要连接段150mm（验收进行中）

后续现状：早期弧长相位撤回说明已经过构建一致性审计更正，见agent_change_audit.md；当前明确采用喷涂弧长相位。统一Native新鲜完整2400点引导接续APF在加密2077-2080区间失败，QP未启动，之前19处修复不能当整条成功。现增加私有CdfQpRepairCheckpoint.h续算领域入口，重生成实际参考后核对全部q/时间/点数/限位/端点，再重查全部运动；公开API及依赖不变。APF六轴喷涂、局部走廊至少125mm时，原14场失败后补1.10/1.65rad扰动；原场、普通APF、全部硬验收保留。Release/Debug主程序/Native及各四项回归通过，源码CRLF与42DLL基线核对通过。原MainWindow v16 PID44612用ryh1-large-escape-bin从原始导入/全IK/Top1/生产CDF独立重新计算。v15主动停止并留aborted；Native30824等待失败诊断阶段导出，任务内监视7604之后接续新算法。尚无ryh1最终认证，runtime.json/活动任务均未切换。以外部output/current_execution_status.json及实时日志为准。

用户明确允许必要连接段放宽到150mm，全部2400原始站点保留并报告偏差。共享喷涂规划服务新增可选connection_tcp_deviation_m与source_station_tcp_deviation_m，缺省沿用原调用预算；本次隔离配置连接150mm、原站点100mm、原站点轴向覆盖2mm。保留位移后guide锚点到原点的映射，在APF admissible/projection、减点与最终验收中保持原站点边界；不把移动后的guide点当原始参考。各坏区间仍先窄后宽，再尝试100/125mm及剩余预算，不重启整轨迹。独立Native验证读取相同profile配置并分别报告连接与原站点误差。未证明改善的弧长相位已撤回源码，对照二进制与日志保留。固定机器人/工件/碰撞配对/喷向门限，不发布未验收final。下一步先实测348附近瓶颈，再完整真实MainWindow重算、单轮QP、连续及高密度独立验证，全部通过才激活脚本。


### 2026-10-17 ryh1 连续IK预处理源构型保持修复（整条验收进行中）

定位旧Cartesian预处理会将中间修正姿态带到后续原始站点，导致71个已选引导构型变化超过1度、最大约125.245度。其失败参考的关节弦FK最远偏离原参考948.488mm；这是APF前失败输入诊断，不是已交付最终轨迹。证据在外部任务output/cartesian_preconditioning_drift_diagnostic.json。

SprayGuidedRepair仅允许插值内点携带修正姿态，每个源边终点必须回到已选安全构型；数值IK单步跨越0.12rad则拒绝整个预处理边，保留原已选端点交给APF。预处理后断言全部源时间及关节值原样保留；不锁死后续APF的姿态自由度。Release/Debug主程序、Native及各四项领域回归通过，正常CMake部署配置恢复成功。私有续算门禁曾在旧相同参考上实际通过，但参考已改变，当前全APF从新参考重新算，未挪用旧部分路径。

Native46864实际确认全部2400已选构型保留，2244条预处理边、155条交APF、11744内部节点；密扫描12815点，182个不合格区间（旧参考16724点/238区间）。原MainWindow v17 PID30652从原ryh1导入、全IK、全局Top-1、生产CDF按钮独立重算，无引导检查点。以上只证明预处理修复和工作量变化，尚无整条无碰撞final，runtime.json及默认活动任务未切换。


外部report.py补充独立original_path.png、apf_path.png、final_path.png（统一视角/坐标范围）以及全部原站点实际FK位置/轴向误差图original_station_tracking.png。只读取真实验证CSV，不用TCP线性插值估计冒充FK误差；旧ryh文件仅复制到显式old_data临时目录做图像排版核对，旧final未改，也不作为ryh1成功证据。外部报告改动不影响正在计算的领域算法/运行时身份。


### ryh1 私有入口姿态逃逸场与轴向限制对照（实验，未采用为最终流程）

完整endpoint-guard Native46864继续运行，已通过17个局部段并越过旧参考2077对应瓶颈；原MainWindow v17 PID30652仍独立计算。其冻结18ff9ac9库身份、二进制和原2mm轴向限制保持，不把后续全局库冒充该运行版本。默认用户任务和旧已验证结果未改。

为处理后续局部极小值，在六个领域私有源/头/测试文件中添加ApfSeedFields：原14个关节方向场之后、较大通用场之前，尝试绕标定入口的18个保持TCP位置姿态族，并分别从两端构造吸引参考；模型IK、合法turn和限位筛选提议，最终仍由原完整运动/工艺/走廊检查接受。仅扩展私有repairCdfQpWithCheckpoint入参；公开服务Options、SDK与依赖不变。失败族继续其他族，越限/非有限提议不得进入oracle；固定端点和输出点数保持。Release/Debug主程序、Native和四项领域回归全部通过，源CRLF和SDK42DLL基线SHA核对通过。构建正常配置已恢复。当前入口场局部331-420点诊断PID16872仍在运行，不能称已改善整条轨迹。

另用局部365-379源点、原TCP位置和相同安全引导，隔离对照既有可配置axial_tracking_tolerance_m=20mm；原站点100mm与连接150mm球形位置界仍保留，模型/碰撞过滤/20度喷射锥/61度入射不改。该20mm轴向不是已采用参数；优先保留仍在推进的原2mm完整结果。首次对照PID29068因独立Native固定名inner_surface.json未复制而未进入规划，记录setup_failure；复制同SHA几何与运动界后PID5608重试，不能误称算法失败。所有裁剪窗口仅诊断，端点姿态检查可能预期拒绝，绝不能发布为完整final。

外部finisher补充传播显式axial_tracking_tolerance_m（未配置则移除并使用默认2mm），防止将来验证采用配置与手动流程profile不一致；当前v17仍读取原entry150配置。当前源hash在entry_fields_source_hashes.json，隔离runtime_entry_fields_candidate.json；原endpoint源码/库备份before_entry_fields。尚无ryh1可执行最终结果。


### ryh1 原2mm轴向的局部连接对照（全轨迹未验收）

2026-10-17：v14入口几何参考允许碰撞的内部吸引提议进入APF，实际运动接受门禁不变。源365-379局部在75mm走廊完成APF、一轮QP、保守连续几何与高密度喷向检查；裁剪端点不等于原Top-1端点，最终端点检查按预期拒绝，不作为完整final。放宽20mm轴向的局部也连通，但不必据此采用；完整20mm仅隔离对照，原2mm完整Native46864和真实MainWindow30652继续运行。

后续v15私有APF尝试在单个入口参考点生成失败时保留该点原参考而继续后续有效提议，至少一个有效提议才启用，所有接受边仍完整核验。增加缺口回归、入口拒绝原因限量日志；Release/Debug主程序、Native与四项精确匹配领域回归通过，源CRLF保持。隔离局部44868尚运行，无整条证据，不激活脚本默认任务。一次测试目标拼写错误已更正；一次过宽测试正则启动的Periodic测试已主动停止，随后四项精确测试全通过。


### 2026-10-17 ryh1 旧已验证姿态通道参考（v19，整轨迹验收中）

用户确认新轨迹仅扩展两端。实际核对旧2600、新2400点；有序笛卡尔对应动态规划确认大部分原点完全一致，新增开口段距旧原点最多100.0012mm。v14完整APF仍在源395附近耗尽，QP未开始；v18真实主窗口消失且没有完成报告，不能称通过。v16/v17局部都完成APF/QP但裁剪端点不等于整轨迹端点而拒绝，不能发布。

共享SprayGuidedRepair新增可选attitude_reference_file及FNV1a64校验；参考文件明确每个当前原点、旧原点及旧实际runtime弧度关节。只取旧实际FK姿态与偏移作提议，平移到当前原点重新实际模型IK、合法turn/限位、完整碰撞与原喷向检查。首尾仍用当前Top1原位姿；未通过的新端部周围继续旧候选族和连接搜索。公共段保留既有姿态通道，所有连接仍重新APF、单轮QP、连续及密验收，不继承旧安全结论。viewer_scene.py传播该可选路径。无公开API、依赖或SDK变化。

Release/Debug主程序/Native构建及各四项领域回归通过。Native35548首次运输2316/2400节点通过，剩余候选在求；原MainWindow46300从原ryh1导入、全IK、全局Top1、生产CDF独立重算。外部冻结runtime_prior_attitude_candidate.json；默认活动任务和runtime.json仍未激活，旧ryh final不改。CMake目前仍为prior-attitude隔离注入，交付前须恢复正常并核对Debug/Release42SDK DLL。


### ryh1 已验证曲线中间节点的姿态参考（v20，完整验收仍在进行）

v19仅转移离散姿态会移除旧路径中间的安全弯曲，且不必要地将原2mm允许范围内的轴向偏移归零。未发布v19，主动停止Native35548和真实GUI46300，原日志保留。v20参考增加旧实际关节直线曲线中间节点，2400原站点全部保留，附加2072参考节点，共4472。对已满足2mm限制的旧偏移保持，只在越界时恢复轴向；优先保留第一个完整通过的旧参考姿态，失败处才补充新姿态。仍对全部实际运动重新验碰撞、工艺、位置和末端位姿。

共享私有实现支持有序source_time_s参考节点，逐一验证原站点完整覆盖和当前原输入位置；既有普通无参考路径不变。末轮连接图迭代不再生成不会被下一轮使用的候选。Release/Debug主程序和Native构建、各四项领域回归通过；原输入不符/参考文件篡改/遗漏原始站点三项真实引擎拒绝测试通过。42SDK DLL/配置SHA不变，CRLF正确，正常CMake注入已恢复。

外部prepare_attitude_reference.py从旧已验收实际runtime关节曲线和原源时间构造有序参考，动态规划对应2000个完全相同位置和400个开口延伸站点。生成数据与运行数据字节完全一致。finish_gui_run.py仅在真实GUI完成、独立连续/密验收及真实MainWindow播放全通过后复制参考数据和profile到交付场景及任务输入，尚未执行激活。

当前Native29040与原MainWindow5584均用冻结ryh1-reference-knots-bin从原2400点/全局Top1运行，4360/4472参考节点通过，仍在端部姿态与连接搜索；首次连接探查132不合格，不等同132实体碰撞。watcher39136仅等待本次GUI报告后严格验收；无整条final，旧ryh不变。外部状态为output/current_execution_status.json。


### ryh1 端部横向偏移通道试验（尚无整条验收）

独立网格核查已证实旧姿态延伸时长喷枪中段与内壁真实相交，SDK约949mm穿深仅粗值，不作为实际穿入量。固定旧姿态的横向YZ提议覆盖全部200个远端新增离散站点，但最初probe的工艺参考随位置偏移，不能据此声称原喷涂工艺通过。新probe显式提供未移动原点作工艺参考：保留2400原站点和旧曲线节点，并在延伸边每10mm插值，10722节点全部IK有效，7碰撞、540工艺未过，最大相邻关节步14.145度；均为失败候选诊断，不是final。既有服务重验证10400/10722通过。

外部prepare_transverse_reference.py以原轨迹有序对应、端部候选小图和sin平方渐变构造可重复的参考提议；共享SprayGuidedRepair仅对未通过的参考节点，先尝试5至95mm的横向小偏移，保持原参考姿态/turn并重新执行实际IK、喷向、限位、位置与完整碰撞门禁，再回退原姿态族。无公开API、依赖或SDK变更。隔离reference-transverse Release/Debug构建验证待完成，旧二进制冻结继续运行，默认活动任务与旧ryh final未改。必须整条真实MainWindow、APF/单轮QP、独立连续及高密度验收通过才发布。


### 2026-10-17 新显式 STL 场景运行及检测器兼容（整轨迹未完成）

本次输入为 ABB4600-burnner.sys.json（SHA b0b303aab044）和当前 ryh1.txt（SHA 86f5af28c9d6），独立对象 A 保留原毫米缩放0.001及原变换；未复用旧工件URDF、旧安全证书或旧姿态参考。2400点全IK找到19200候选，全局Top1代价523.623345046679。原窗口首次因UI硬编码 obstacleId=burnner 及 robotId target 而提前拒绝，Native完成原站点姿态搜索但在可选加密点2.659053s无安全姿态提前退出；均不是最终成功。

共享ProjectCdfQpTrajectoryRepairService新增只读isCollisionSetupConfigured查询，与ensureCollisionSetup共用工件解析，支持显式RobotObject检测器；MotionPlanningModuleController移除三个重复局部检测函数，初始化、CDF按钮和播放统一使用共享查询。旧RobotRobot保持兼容。新增对象检测器就绪、禁用、缺target、错误机器人及旧机器人型工件回归。SprayGuidedRepair仅移除搜索不到安全姿态的本轮可选加密探针，原始站点始终保留；连接仍需后续APF和完整连续检测通过。未放宽碰撞/工艺门禁。

Release和Debug主程序、Native及四项领域回归均通过。一次测试target拼写错误（缺连字符）已更正，不影响主程序构建。外部launch_scene_workflow支持有独立证据名的attempt；v2原窗口39716、Native27176已重新计算。当前无最终无碰撞结论，runtime.json/active_ryh_job.json未激活，旧结果不动。隔离CMake注入待运行完成后恢复；完整导出、独立密验证及真实MainWindow播放仍待完成。


### 2026-10-17 新场景连接连续性与拐角前瞻（v3，完整验收中）

v2全轨迹尚未到完整APF阶段，发现post-Top1喷涂引导对未知可连边施加1e5惩罚，导致>60度关节步从原始Top1的3处增至219处（>120度143处），不应当将关节直线无法连接等同不可绕行。已主动中止本次v2 GUI39716/Native27176及其watcher，保留证据，未发布final。共享SprayGuidedRepair将该偏好降为可配置guidance_blocked_connection_weight（缺省10）；原全IK分层图完全二部连接未变。

ApfLocalPlanner仅对碰撞前瞻跨过拐角，并将拐角后的首站纳入有限120mm弧长预算；吸引和跟随仍逐站经过并在拐角停止吸引前移。回归覆盖跨角、超预算点不预测及输出原站点不被抄近路。CdfQpTrajectoryRepair对明确允许>=125mm连接范围的喷涂任务优先尝试完整腕部逃逸范围，全部窄范围fallback保留，已有区域成功宽度仍优先；后续轨迹形状恢复、单轮QP及全部硬安全验证不变。

外部任务显式采用planning_configuration_revision=2，并在output/before_planning_profile_revision_2保存上一版profile和metadata。源场景/2400源轨迹/模型/变换/缩放/碰撞过滤均未变；更新后的profile SHA记入metadata，旧运行自身viewer_config_v2与旧profile副本保留。新参数：原站点安全姿态图单次选择，不把困难Cartesian弦逐轮变成大幅换构型；后续连接交APF。以新STL左端截面边界计算开口引导中心[1.2,.6,.62283155]，仅产生经实际IK/碰撞/喷向检查的提议。连接150mm、原站点100mm和轴向2mm覆盖硬门禁保持。参数与prepare_scene新建任务逻辑同步；启动器新增源场景及所有静态输入SHA检查。

最新Release/Debug主程序和Native构建、四项领域回归全部通过；两配置42SDK DLL同既有冻结基线SHA，Debug重新部署的三DLL已恢复基线。原MainWindow45404、Native38972从原输入重新计算；watcher27388仅在当前原窗口四阶段成功后进行独立连续/密碰撞、限位、动态与全源站点覆盖核验和真实MainWindow播放，均通过才激活。当前无完整无碰撞final。早期24点切片仅诊断，均已主动中止且记录aborted，不用于整轨迹验收。隔离CMake注入仍待最后恢复。


### 2026-10-17 原始TCP走廊与安全IK种子分离（v4，184区间连通中）

v3在新150mm授权下实际仅使用79.731886mm局部范围：错误地把全轨迹最大guide偏移70.267114mm从每个局部段统一扣除；且APF位置引导仍贴在移位后的guide上，不是原始路径。已中止v3 GUI45404/Native38972与watcher，未发布final。v3安全引导2400点，相邻>60度7处、>120度2处，相比v2的219/143已显著减少，但不当成最终运动跳变结论。

私有repairCdfQpWithCheckpoint新增可选originalTcpPositions，独立传递每个规划结点按原始源时间插值的位置；原cartesianTargets仍是实际安全种子的完整姿态，原FK一致性验证保留。原始位置逐点数量/有限值/FK缺失均拒绝，回归覆盖不合法数量和NaN。SprayGuidedRepair使用原始位置直接约束150mm连接范围，不再扣全局guide偏移；原站点100mm、轴向2mm、原喷向及全部碰撞门禁不变。APF仅在种子超出原走廊时恢复位置，走廊内已有安全偏移不被强行拉回原碰撞中心线；新增回归验证保留安全offset。无公开API或SDK新增，此处仅私有续算参数扩展。

Release/Debug原主程序及Native构建、各四项领域回归通过。SDK42DLL基线与源CRLF已核对；最终正常CMake配置恢复待执行。原MainWindow25112用ryh1-original-corridor-bin从原ryh1重新全IK/Top1/生产CDF；Native33884仅复用并重新实际验证v3选择的guide，不复用APF/QP或任何碰撞证书，以快速诊断同版后段。当前完整扫描184碰撞/形状不合格区间，首个旧长期失败的56-58及其后两个区间已实际通过swept验证。真实GUI验收watcher38728严格等待当前报告后再完整独立复验与播放，无final/active/runtime.json激活。


### 2026-10-17 新场景 APF 喷向势场尺度诊断（v5 对照，未验收）

v4 Native与原MainWindow都在加密2135-2138附近的转向连接持续搜索，已经通过36个先前局部区间；未进入最终QP或发布final。发现后置喷涂tcpStateClearance用0.1*(cos(theta)-cos(limit))将20度喷向锥的最大软余量限制到6.031mm，而APF影响半径为80mm，因此完全对准仍有约2.31斥力权重；混合势场存在不必要竞争。

SprayGuidedRepair私有helper将每个角度锥的有符号余量按完整合法范围归一到当前APF影响半径；零边界、越界符号、真实喷向admissible、碰撞、位置及全源站点覆盖硬门禁均保持。只修改私有实现/头和SprayGuidanceTests，新增理想对准、边界、越界、合法内部及退化输入回归。Release/Debug与真实新场景对照尚待完成，不能声称已改善或获得整轨迹。v4冻结二进制与进程不覆盖，v5隔离bin为ryh1-angular-potential-bin；默认活动任务和旧已验证结果不变。


v5后续验证：Release/Debug主程序、Native、APF及喷涂测试构建通过；各四项领域回归通过。Debug构建重部署的lib_coacd-D.dll/tbb12_debug.dll已恢复42DLL冻结基线并重新运行Debug四项通过；正常CMake配置曾恢复。真实MainWindow v5 PID21396与Native21796运行中。旧v4原窗口25112及watcher38728主动停止保留记录，Native33884已完整退出为失败：通过37局部段，2135-2138最终在100mm窗口连通，2257-2261各窗口未进入力场迭代，QP轮数0，不能发布。

开始v6投影早退修复对照：ApfLocalPlanner将内部吸引参考的失败投影视为可修复提议而非整条失败；投影在副本上执行，失败/非有限/越限副本丢弃，原有限参考保留。固定端点、实际输出的全程碰撞和工艺/位置硬门禁不变。私有oracle补充未投影参考数量日志；APF测试新增故意使碰撞参考投影失败且污染副本NaN的案例，要求仍找到无碰撞绕行且端点、点数和工艺门禁不变。v6构建/实测尚待完成。v5冻结版本不覆盖；v5后台额外从v4部分路径续算只作诊断，门禁先核对重新生成的全参考与全部q/时间/限位/端点，再重新检查运动，不继承安全证书。


v6验证进展：Release/Debug主程序、Native和APF/喷涂回归目标构建通过，各配置四项领域回归全部通过。两配置42SDK DLL已恢复旧冻结SHA基线，CRLF/源hash核对成功；正常CMake配置已恢复（CMAKE_PROJECT_INCLUDE移除，Copy_RuntimeFor_RobotQtViewer=ON）。v5仅改软角度势场的续算诊断再次在2257-2261未进入力场即失败，0轮QP，原窗口21396、重复Native21796和watcher34144已主动停止且留证；v5续算Native39256自然失败退出，未发布。

v6真实MainWindow35584从原输入重新全逆解/Top1/生产CDF；Native16520用v4部分APF作为诊断，已通过重新生成参考与q/时间/限位/端点一致性门禁，正在重查全部运动/工艺。watcher32940只接受v6真实窗口成功四阶段导出后，独立连续/密检测、原站点实际FK覆盖、动态限制与真实窗口播放均通过才发布。尚无新场景最终无碰撞轨迹，runtime.json与active_ryh_job.json未激活。


### 新场景APF两阶段搜索顺序（v7实验，整轨迹未验收）

v6续算实测已通过旧失败2257-2261和后续2381-2388；日志显示10个投影失败内部参考并未阻断实际APF，修复后实际连接通过完整门禁。当前在2510-2514耗时，多组宽走廊场/锚点仍搜索，真实MainWindow35584独立前进；不是整条成功。

为减少单个宽走廊中先耗尽所有大幅逃逸场的等待，私有ApfFieldStage分All/Basic/Escape，通用APF缺省All保持原次序；喷涂repair先在所有原走廊/原锚点组合尝试原14个普通方向场，再在同样组合尝试全部原几何/大角度逃逸场。未删除任何原搜索场、未改各场迭代预算、端点/碰撞/工艺/位置/原站点门禁。明确>=125mm范围的宽度顺序改为由宽到窄，区域已成功宽度仍最先，其余较宽/较窄fallback都保留。增加阶段回归：Basic不能偷用窄通道几何种子，Escape仍可通过该通道并接受逐边运动核验。v7构建和完整实际效果尚待验证；v6冻结运行不覆盖。


v7对照选择：v5/v6角度标度归一试验未表现稳定改善，前部某些连接需更多方向场；恢复before_angular_potential_v5内SprayGuidedRepair.h/.cpp及SprayGuidanceTests.cpp，退役试验源存output/retired_angular_potential_v5。保留实测有效的v6投影副本/fallback修复和v7完整搜索集的两阶段次序。copy2保留旧mtime的首次恢复没有触发重编译，未运行该中间二进制；已显式刷新三源mtime、实际重编译并核对日志。最终Release构建及四项领域回归通过，v7原MainWindow37076/Native34524已启动（Native只用v4 partial作完整复验后的诊断），Debug构建待完成。v6两进程保留对照，停止watcher32940防止重复发布；当前仅v7 watcher19520允许触发严格验收。无最终新场景无碰撞结论。


v7最终构建核对：原角度势场版本的Release/Debug主程序、Native及两受影响测试目标均构建通过，各配置四项精确匹配领域回归全部通过。42SDK DLL均同原冻结SHA，Debug重部署的两个DLL已恢复；七个相关源/头/测试CRLF与hash核对通过。正常CMake配置恢复完成，未留注入。v7实际运行继续，无整条APF或QP成功，未发布final。更正一次用户进度口误：v6日志出现参考节点2632是扩大锚点的内部节点，并非2510段通过；已向用户明确更正，统计仅以APF interval verified为准。当前v6在2510段继续搜索，只有2257和2381两处新增连接实际通过。


### 2026-10-17 连续关节 APF 大幅逃逸搜索范围（v8，整条尚未验收）

v6已实际通过2257与2381连接，v7仍在2510局部搜索。核对发现全部continuous关节最后一级局部范围仅参考角上下0.75rad，启用的1.10/1.65rad场会被截断；有限硬限位关节最后一级原已使用硬全范围。仅在喷涂引导且TCP连接授权>=125mm的最后一级，将continuous搜索padding扩大至1.75rad；实际模型硬限位、TCP150mm、原站点100mm与轴向2mm、喷向和完整碰撞门禁均未放宽。保留v6失败参考投影副本隔离/fallback修复；撤回未证实有效的v7两阶段调度与角度势场归一试验，原场集和顺序恢复。

新增窄通道回归：0.75rad上限必须失败，1.75rad必须以固定端点通过逐边oracle检查。Release主程序、Native及受影响测试目标构建通过，APF等四项回归通过；42DLL与冻结基线SHA一致、源CRLF和hash核对通过。Debug构建及实际v8场景运行待完成，尚无整条无碰撞轨迹，未激活final或旧活动任务。


v8后续验证：Release/Debug主程序、Native及受影响测试构建全部通过，两配置四项领域回归全部通过；42SDK DLL均核对冻结SHA，Debug两个重部署DLL恢复后再测试通过。正常CMake配置恢复完成。v8真实MainWindow46536重新原输入全IK/Top1/CDF，Native27340仅以完整复验后的v4部分几何诊断，唯一严格watcher26136等待实际窗口四阶段结果后再独立验收。v6/v7旧计算保留对照；v7 watcher19520已停止防重复发布。当前尚无完整APF/QP成功。


### 2026-10-17 APF 原参考 TCP 横向平滑候选（v9 试验，未验收）

v8连续关节范围修复已构建并通过两配置回归，实际Native新增通过2257连接，但仍在2381连接搜索；原MainWindow独立运行到2135附近。未将搜索中间状态或内部节点编号当作连通成功。旧ryh最终轨迹在本次新场景抽查3432节点，843碰撞或无效；同STL几何浮点缩放误差约3e-8m，但本次工件Pitch=pi，旧为0，旧证书不可沿用。

为避免仅扰动关节导致长TCP不受控的大偏移，私有ApfSeedFields新增可选generatePhased回调，提供实际弧长phase及当前参考q，原三参接口保留。喷涂领域增加14组TCP横向候选，幅度25/50/95mm、sin平方进退、以当前参考实际FK为基准，保持轴向位置并限制提议总偏移；实际IK/turn/限位/喷向重新验证，旧18入口姿态族和全部原关节场保留。只改变吸引提议，不接受未经完整碰撞/工艺/走廊检查的边或路径。公开API、SDK、依赖未改。

新增回归覆盖分相位窄通道、正反方向物理phase、固定端点、逐边oracle及越限提议拒绝。四个领域私有源/头/测试使用CRLF，冻结v8运行未覆盖。v9构建/实际效果待验证，bin为ryh1-transverse-pulse-bin；无最终结果发布。


v9最终构建核对：Release/Debug主程序、Native与受影响测试目标全部构建通过，各配置四项精确匹配领域回归通过；42SDK DLL均同冻结SHA，Debug重部署的lib_coacd-D.dll和tbb12_debug.dll恢复后测试通过。源CRLF/hash核对通过，正常CMake配置恢复。真实MainWindow27840从原ryh1/当前指定场景重算全IK、Top1和APF/CDF；Native5960只用本任务v4部分路径、并完整重查作为对照诊断。唯一验收watcher45036等待v9原窗口四阶段及独立证书/播放通过才发布。v8 GUI46536与Native27340保留比较，v8 watcher26136已停止，v7 Native34524仍为诊断。当前无整条结果，不激活runtime.json或旧活动任务。


### 2026-10-17 可选APF几何场单点提议失败不取消整场（v11，待验收）

v9实际日志中2381附近第一局部窗口所有新增14横向场及18入口姿态场在力场迭代前均被取消；某一内部站点未能生成姿态提议，不能等同整段不可连。新增私有allowPartial opt-in（缺省false），仅喷涂几何场启用：逐点提议在副本上生成，失败/NaN/越限副本丢弃，该点继续使用已核对有限/限位的原吸引参考；其它有效点继续；整组没有任何有效新提议则跳过。输出点数、固定端点、完整运动/工艺/位置门禁不变，没有把失败状态当作安全状态。新增partialSeedField日志分清提议失败和实际运动失败。

新增回归覆盖部分提议失败并污染NaN、缺省整场取消、opt-in可通过窄通道、全部失败跳过及越限值不进oracle。只改五个领域私有源/头/测试，无公开API/SDK/依赖变化。v11隔离bin=ryh1-partial-field-bin，当前构建/实场待验证；v9冻结运行及guide3参数对照不覆盖，没有新最终结果。


v11构建与启动核对：Release/Debug主程序、Native与受影响测试目标均构建成功，两配置四项精确匹配领域回归全部通过。42SDK DLL冻结基线SHA一致，Debug重部署两个DLL已恢复；源CRLF/SHA核对通过。正常CMake已恢复（移除CMAKE_PROJECT_INCLUDE，Copy_RuntimeFor_RobotQtViewer=ON）。真实MainWindow42780从原输入重新全IK/Top1/CDF；Native30336仅作重新完整核验后的v4部分路径诊断；唯一严格watcher31156等待完整GUI与独立验收。v8对照已主动停止并记录retired_v8_processes_v11.json。尚无整条无碰撞final，不激活旧任务。



2026-10-17 新场景后置开口姿态连续提议诊断：v11局部停滞94状态单点均无碰撞，15状态对对应原参考喷向不合格，尚非整条成功；v9重复对照已主动停止。guide3参数对照完成：同2400源点，最大guide关节步由178.15降至48.60度、最大TCP偏移43.27mm，但仍有105个Cartesian连接未通过。独立开口提议实验3180个中3150单点安全，不视作连接证书。
基于当前新场景guide3（不是旧ryh最终轨迹）建立开口参考试验：对开口外节点使用两侧内区guide姿态及横向位置平滑延续，保留原x/全部2400站点、位移<=99.5mm；1592提议核验后替换398个外区节点，首末节点不变。新不可变job为原job名加_rim_reference；数据用已有attitude_reference接口仅作吸引提议，生产服务重新核对实际IK/碰撞/工艺/覆盖，没有复用安全证书，原全IK/Top1图不变。没有新增C++/SDK/依赖改动。原MainWindow46864和Native5796均从原输入重新全流程运行，严格watcher已启动、旧watcher31156停止，尚无final。



2026-10-17 当前场景开口参考DP试验：389外区提议经连续性选择，原2400源点全部保留、最大TCP偏移49.29mm、参考最大关节步48.60度。发现生产载入后末点却跳至172.89度：attitude_reference分支无条件用原Top1末端关节覆盖参考，虽然参考末端是相同TCP的另一有效IK分支。用户已允许相同起终位姿的不同逆解。拟在SprayGuidedRepair私有端点选择函数增加profile opt-in attitude_reference_equivalent_endpoints（缺省false兼容旧行为）；只有严格实际FK位置/姿态、有限值和硬限位均通过才保留参考分支/turn，完整safeCandidate及运动/工艺/覆盖/独立验收不变。新增非周期镜像构型、合法多圈、默认分支、非法位姿/角度/限位/NaN/维度回归。三源备份与SHA存_rim_dp/output，v14待构建验收。
外部启动脚本增加--job以及严格快照选择helper，避免发布/再次启动时丢失当前不可变profile变体而误回原profile。匹配输入/自动场景、错误源/目录越界/篡改profile五项校验通过。旧active不变。



v14端点参考修复验证：Release/Debug主程序、Native和受影响测试目标构建成功；各配置四项精确领域回归全通过。两配置42SDK DLL同v11冻结基线；Debug重部署的lib_coacd-D.dll/tbb12_debug.dll恢复后测试通过。正常CMake已恢复。原实际FK核对参考首末点误差分别约1.6e-12m/7.5e-7m、姿态1.1e-12rad/2.2e-7rad，均严格满足原容差。新实际guide最大关节步48.60度，原无条件覆盖导致的172.89度末点突变消失。
当前唯一GUI验收job为原job名加_rim_reference_endpoints；398个开口外点使用当前场景内区姿态平滑提议，2400点及原轴向范围完整、最大guide位移51.68mm。原窗口17440、Native24864运行全流程；唯一watcher45652等待真实四阶段导出后完整独立验证。旧v13 GUI/Native/watcher与guide3 Native已主动停止，记录retired_prior_trials_v14.json；rim12 Native5796仅保留前缀诊断对照。没有完整无碰撞final，旧active未改。


### 2026-10-17 v14开口连接与后置参考连接细化对照（未验收）
真实MainWindow17440继续指定ABB4600-burnner场景与原ryh1的完整全IK/原全局Top1/APF/CDF，36处局部修复已验证，2271-2274加密索引连接仍在扩大锚点搜索；该计数不是剩余碰撞数，没有完整最终轨迹或新的安全证书。旧rim12 Native5796只保留前缀诊断，不可发布。
为定位单轮后置参考选择的连接瓶颈，新建独立不可变job _rim_reference_endpoints_connections3，仅把已有guidance_connection_passes由1改3；原Top1、2400源站点、全部静态几何/碰撞/喷向/100mm源站点及150mm连接范围均不变，无C++/SDK/依赖更改。Native10908仅诊断；第一轮未连接68，第二轮70，尚不能认定细化改善。原GUI与唯一watcher45652继续，ACTIVE不变。输出live_progress.json明确区分局部修复、完整工作流和最终认证，不把中间路径当交付结果。

### 2026-10-17 双向APF已验证前沿拼接（v18，构建/实际验收中）
当前v14原GUI在2271-2274仍搜索，未发布完整结果。三轮后置连接参考试验v15单点未连接69（原68），但Native已实际通过2304-2306一处同类困难连接并到2432-2434；原MainWindow18868从原输入/全IK/全局Top1重新跑该配置，独立watcher27556尚未验收。现有两项输出均不可当整条无碰撞。固定参考位置的姿态平滑v16共7194提议，7137安全新候选，DP采用2124，初始shape/碰撞区间168降到155；它仍是Native诊断。旧v11前缀Native5796已验证身份后主动停止，记录retired_prefix_native12.json。
200个实际日志停滞状态以当前实际FK和碰撞器独立抽查，碰撞/IK无效0，喷向不合格0；61度与85度门禁均有效，故没有采用放宽喷向限制。瓶颈是连接而非这些状态本身。代码核对发现既有正反向场失败时丢弃已验证前缀。v18在私有APF保留正向prefix和原顺序reverse suffix，只在覆盖重叠/相接、原始turn差<=0.35rad的短连接中按代价选最多16个接缝；每个接缝与拼成的每一条边都重新检查原guide/工艺和完整运动oracle，保留原点数、锚点、原字段全搜索，无wrap、无删碰撞边、无跳过原站点。仅有clearance引导的路径启用；普通APF原流程保持。
修改ApfLocalPlanner.h/.cpp、CdfQpTrajectoryRepair.cpp与现有ApfLocalPlannerTests.cpp四个私有文件，CRLF、事前备份/源SHA已记录。新增前后半场互补的完整规划回归，拒绝坏接缝/无覆盖/NaN/越限/错误turn/被拒运动，验证逐边完整重查。公开API/SDK/依赖未改。隔离v18 bin构建中，不覆盖当前已运行v14；最终仍以真实MainWindow四阶段导出及独立密检/全部2400源站点覆盖和播放通过为发布条件。

v18最终构建核对：新增互补前后场规划回归最初使用vector精确浮点比较而返回157，改为1e-12数值误差检查，保留必须触发stitched回调及逐边oracle的条件后通过；算法无需为此放宽。额外拼接碰撞尝试最终限制为每个新场总计16次（不是每对16次），先按原turn短接缝代价排序，完整拼后逐边检查不受次数预算截断。Release/Debug主程序、Native及受影响测试均构建成功，两配置四项领域回归全部通过，SDK全部DLL与v14冻结基线相同，正常CMake已恢复。证据build_bidirectional_apf18_bounded.log、ctest_bidirectional18_*.log、bidirectional_apf18_final_source_hashes.json与事前备份在v14 job/output。
新不可变job为_rim_reference_endpoints_bidirectional，全部输入/profile/几何同v14，运行库为ryh1-bidirectional-apf-bin。真实MainWindow31200从原ryh1重新全IK/原Top1/生产APF/CDF，Native13940仅诊断，唯一严格验收watcher22760。旧watcher45652、27556已停止且身份/命令留档retired_old_finishers18.json，避免旧对照自动发布；旧GUI/Native暂作比较。尚无本场景完整无碰撞轨迹，ACTIVE仍未改变。

v18真实实场进展：原MainWindow31200在2151-2153区间通过bidirectional stitch（field2，正向9站/反向6站，全边复核），该区间402.363到409.115秒，约6.75秒；v14同区间390.593到686.000约295.41秒。该局部加速不能当整条成功。当前在2271-2274扩大锚点继续搜索。冗余Native13940已验证进程命令后主动停止，留档retired_redundant_native18.json，最终独立验证仍必须执行。
为结合互补进展，新不可变job _g3stitch使用v15三轮后置调姿实际选中的2400源构型作为几何提议，原始ryh1/scene、原全IK全局Top1、FK、限位、喷向及100/150mm范围不变，引用文件哈希和static_input_sha256均重建。该参考曾通过v14困难连接，但不是完整无碰撞路径，未复用证书。v19真实MainWindow37768从原输入重新全流程验证，bin仍为已双配置测试的v18；配置C:/b/rs105-merge/ryh1-cfg19。唯一自动验收watcher42784，旧v18 watcher22760停止但原GUI仍为对照，记录retired_old_finisher19.json。v19负责最终严格独立碰撞/2400站点覆盖/播放；未发布final，ACTIVE未变。

### 2026-10-18 现有自适应瓶颈细化参数对照（v20，未验收）
v19原MainWindow37768仍在2432-2434局部连接搜索，已38处局部修复verified，不代表整条成功。核对现有SprayGuidedRepair：guidance_adaptive_passes=0时不在长连接失败位置新增中间姿态候选，而原输入相邻位置中位距离约91.07mm。新建不可变job _g3stitch_adaptive20，只启用既有4轮自适应细化及12轮连接选择；原输入、全IK/全局Top1、场景、全原始2400站点、100/150mm范围和完整安全门禁均不变，未改C++/SDK/依赖。Native4052只作独立诊断，不能代替真实MainWindow验收，不复用安全证书。第一轮69处瓶颈补点，1个可选中间提议无安全姿态已交回APF，原源站点无裁剪。当前无新最终无碰撞结果；唯一发布watcher42784仍只等v19真实GUI，ACTIVE不变。复现实验脚本在v19/output/launch_adaptive20.py。

### 2026-10-18 APF双向前沿小缺口拼接（v22，已双配置回归，实场未验收）
v19在2432-2434长时间停滞，未取得完整APF/QP。检查现有双向拼接仅接受前沿覆盖重叠/相接，少数中间加密站点缺失时未尝试连接。仅改私有ApfLocalPlanner.cpp/.h和既有ApfLocalPlannerTests.cpp：最多8个缺失站点用原始关节坐标插值作提议，仍限制前沿关节总差<=0.35rad、不wrap、不改点数/两端锚点；每条补边及拼后完整全边重新检查原guide/喷向/位置/运动碰撞oracle，共享每新场16次接缝预算不变。原APF搜索集、场次和安全范围保留；公开API/SDK/依赖未变。增加缺口辅助函数和真实planApfPath缺2站点成功回归，以及碰撞接缝、错误turn、越限、NaN、过大缺口拒绝测试。
首次v21 Release通过但Debug捕获suffix.begin()+resume+1-suffixBegin中间越界迭代器，已停止该测试及首次主窗口试运行，改为suffix.begin()+(resume+1-suffixBegin)，没有接受其结果。v22 Release/Debug主程序、Native及受影响测试均构建通过，各四项领域回归通过（Debug APF约12.46秒），冻结SDK DLL哈希与v18基线一致；CMake已恢复正常注入关闭和Copy_RuntimeFor_RobotQtViewer=ON。三私有源CRLF核对、事前备份、独立diff APF_bounded_gap22_change.patch及源hash均在v19/output。v21隔离bin和失败日志仅归档，不能作为运行发布库。
正式实场新job _g3stitch_gap22，bin=ryh1-gap-frontier-fixed-bin，真实MainWindow21232从原输入全IK/全局Top1重跑；唯一严格验收watcher4480。旧v19 GUI37768已验证身份后主动停止，日志保留，不是自然失败；旧watcher42784和v21 GUI44276/watcher27380均停止。Native4052的既有自适应4轮/12轮参数对照仍为诊断，第一至第六轮未连接69/134/147/156/157/152，不能当改善或最终结果。ACTIVE未改，没有新完整无碰撞final。

v20诊断结论：四轮自适应后2898参考点，全部2400源点保留；第11/12轮均138未连接，原站点最大TCP偏移仍49.291772mm，但最大原结点关节步升至179.99256度，劣于v19/v22的48.595535度。因此未采用此参考，Native4052在guide checkpoint完成后主动停止，retired_unhelpful_adaptive20.json与comparison_decision.json留档，不进入其APF、不发布。当前仅v22真实MainWindow21232和严格watcher4480在运行；v22已21处局部连接verified，未完成整程。

### 2026-10-18 小缺口工艺投影v25与授权大幅调姿v26/v27（实场尚未验收）
v22实际仍停滞2432-2434，不能宣称修复；38处局部verified不是整条安全。相同已调姿TCP的全逆解分支诊断（postguide_branch_probe23）19200候选/2400点，直接[-pi,pi]典型区间结果有177.29度跳变；按实际URDF六joint均continuous的特殊模型做显式turn提升，代价118.46到118.22，最大步48.60到48.66度，没有实质改善，未采用。FK同姿态误差<1e-6m/rad，2400节点无碰撞仅是单点诊断，不是运动证书，也不改变原始输入全IK/Top1图。
v25仅三个APF私有源/头/测试：原始小缺口插值优先，失败时可对缺失内部站点使用既有喷向/源点覆盖投影；固定前后前沿不动。投影失败/NaN/越限/原始角差改变>.35rad直接拒绝，原raw turn保留，每次raw/投影尝试共享原16次预算；补边和整条全边完整验证。旧重叠前沿优先，防止新增缺口预算抢占旧成功接缝。新增投影恢复流形、污染副本/错误turn拒绝、预算耗尽不得调用投影等回归。Release/Debug主程序、Native和相关测试构建成功，各四项领域回归通过，全部冻结SDK DLL hash保留；正常CMake已恢复ON且无注入。隔离bin=ryh1-projected-gap-bin，源hash与备份before_projected_gap25、build_projected25*.log在v19/output。无公开API/SDK/依赖改变。
重新核对用户已授权“允许大幅调整姿态，只要仍能喷涂内表面且位置尽量不变”：原inner_surface的max_surface_target_angle_deg=20还限制相对指定参考落点方向，单独放宽到60做不可变job _g3stitch_wide_aim26；实际命中内表面、原61度入射角、全部2400原站点、100/150mm位置范围、碰撞模型/场景变换不变，相关依赖FNV和静态SHA重建。原69未连接降至14（同49.291772mm参考最大偏移、48.595535度最大关节步），这是参考连接预检查改善，不是最终安全。Native27584仅诊断，原全IK/全局Top1保持不变。
新正式job _g3stitch_wide27结合v25库和60度授权调姿配置，真实MainWindow29556从原ryh1/指定场景重新全流程，唯一严格watcher5080。v22旧GUI21232/watcher4480核对身份后停止，记录retired_for_authorized_wide27.json；不是自然失败，未发布结果。另发现被旧交接误记已停止的v16孤儿Native34028仍运行2小时以上，核对完整命令属于_smooth_reference且仍在2226附近后已停止，retired_orphan_native16_34028.json留档。用户独立运行的RSW GUI29136未操作。当前ACTIVE仍旧验收任务，未生成本次完整无碰撞final。


### 2026-10-18 v27 实场连接进展与现有连接候选诊断 v29（未验收）
v27真实MainWindow29556初检35个碰撞/形状区间，明确12处局部修复verified；现停中段加密索引6187-6204继续完整搜索，节点索引不是原始站点进度。此后尚无QP或整条成功。抽查59个日志停滞状态（日志舍入角度）实际状态均无碰撞/越限，其中1个以自身TCP为参考的工艺检查未通过；该诊断没有正确的原参考对应，不能用作完整连接/工艺证书。证据在v27/output/stall_diagnostic_28。
冗余v26 Native27584实际推进到与原窗口相同区域，核对完整进程命令后主动停止，留档retired_redundant_native26.json；不采用其路径，原窗口、唯一watcher5080及用户窗口29136未操作。
新建不可变v27_connections29诊断，仅guidance_connection_passes由1改3，guidance_adaptive_passes仍0；启用已有连接失败附近的pitch/azimuth/position候选扩展，原全IK/全局Top1与全部2400源点、场景/几何、60度落点方向/61度入射、100/150mm和碰撞门禁均不变。只生成调姿guide checkpoint后主动停止，避免重复APF；不是最终结果，不发布或改变ACTIVE。配置和静态hash经select_explicit_job核对。没有新增C++/公开API/SDK/依赖改动。
v25相对v18的完整三个私有文件补丁已存v19/output/APF_projected_bounded_gap25_complete.patch，源CRLF/hash再次核对。旧v22局部补丁保留，不冒充包含v25。


### 2026-10-18 低余量喷涂APF近距离排斥补充场 v30（构建/验收中）
v27同一中段仍在第二组锚点搜索，12处实际verified，未QP。核对APF原80mm排斥下限与本次0.1mm软余量差异，在狭窄工艺走廊可能令排斥与位置跟踪互相平衡；这是待实场验证的因素，不是已证明根因。
仅改领域私有ApfLocalPlanner.cpp/.h、CdfQpTrajectoryRepair.cpp和既有ApfLocalPlannerTests.cpp：在具有工艺clearance且0<配置余量<=1mm时，新增一组近距离普通双向/切向场，作用半径max(5mm,5*余量)，之后保留全部原普通、几何和大角度场。其余规划原次序/半径不变。只改变势场力的提议，硬碰撞、工艺、原站点、走廊、限位和固定端点门禁不变。日志增加实际势场作用半径，与验收安全距离明确区分。新场纳入既有已验证前沿拼接，拼接仍全边重验。
新增功能回归：圆障碍和其上方5mm窄缝，在0.1mm余量下生成同点数/固定端点/原turn路径，逐段密采无碰撞；oracle拒绝所有运动时不得返回路径。事前四源备份、CRLF和hash在v27/output/before_short_range30。独立Release/Debug bin=ryh1-short-range-bin，保留v25正式运行不覆盖；构建和测试尚未完成，没有新最终路径或证书。无公开API/SDK/依赖变动。


v30后续核对：Release/Debug主程序、Native和受影响测试目标均构建成功，两配置四项领域回归全部通过（Debug APF等总39.08秒）；新窄缝正例、拒绝运动反例通过。冻结SDK DLL哈希两配置与v25全部一致，四源CRLF核对通过，差异存v27/output/APF_short_range30_change.patch。正常CMake恢复流程执行中，独立bin=ryh1-short-range-bin。
v29三轮参数诊断完成，仍14未连接，2400点最大参考偏移49.29mm、最大步48.60度未改善；只有10个站点选择改变（最大39.25度），未采用，guide checkpoint后自动停止，未APF/未发布。证据comparison_decision.json。
v30真实MainWindow10820在Release四项通过后启动，Debug同时完成；唯一严格watcher现6636（原37996的构建报告等待路径错误，已停止更正，未验收或发布任何结果）。新正式job为v27名加_short30；配置ryh1-cfg30，所有输入profile同v27不变。旧v27原窗口29556及watcher5080核对命令后主动停止，retired_for_short30.json留档；并非自然失败或安全结果，用户窗口29136未操作。当前已完成原始全IK/原Top1及2400点Cartesian continuation，初始12330加密点正在全程扫描；未有完整APF/QP结果，ACTIVE仍未变。


v30实际只增加普通方向场的5mm尝试，普通14场仍未通过同一中段，后续几何/大角度场又使用80mm，尚不能检验对长喷枪入口姿态族的假设。v31仅进一步将同一5mm补充范围覆盖全部原方向/几何/大角度场，完整原80mm全场仍在其后保留；所有状态/运动/工艺/源站点/锚点门禁不变。只再改ApfLocalPlanner.cpp一行预算和注释，事前备份ApfLocalPlanner_before_all_short31.cpp，四源完整差异存v27/output/APF_all_short31_change.patch。
v31 Release主程序、Native和受影响测试构建成功，四项领域回归全部通过（2.70秒）；Debug构建与四项待结束。独立bin=ryh1-all-short-bin；新真实MainWindow12100重做原输入/原全IK/全局Top1，正式job为v27名加_allshort31，配置ryh1-cfg31，唯一严格watcher31036同样等待两配置构建标记及完整GUI结果才验收。v30旧窗口10820/watcher6636核对身份后主动停止并留档retired_for_allshort31.json；不是自然失败或安全结果。当前未有最终APF/QP结果、未发布final或更改ACTIVE，用户窗口29136不动。


v31最终构建核对：Release/Debug主程序、Native及相关测试均构建通过，两配置各四项领域回归全部通过（Release2.70秒，Debug46.47秒）。两配置全部冻结SDK DLL与v30基线一致；正常CMake已恢复无CMAKE_PROJECT_INCLUDE且Copy_RuntimeFor_RobotQtViewer=ON。当前唯一正式MainWindow12100与watcher31036继续相同原输入/场景/profile重算，仍在Cartesian continuation；最终碰撞/工艺/覆盖/程序和播放验收尚未发生，不能当已成功路径。


### 2026-10-18 低余量大角度避障先尝试宽锚点窗口 v32（实际验收中）
v31几何场也使用5mm排斥后，窄关节范围的首组锚点仍在中段6187-6204停滞，不能宣称力场半径已解决。检查发现原先60/120/240mm提前锚点分别只开放0.25/0.45/1.75rad连续关节padding，大角度场要等最后一级才真正开放。v32仅在明确>=125mm连接走廊且APF总余量<=1mm的喷涂模式，把原六个窗口的scale顺序由0,1,2,0,1,2改2,0,1,2,0,1；全部原physical/knot窗口保留，所有碰撞/工艺/源站点/走廊/限位门禁不变。仅再改CdfQpTrajectoryRepair.cpp及注释，无新增API/SDK/依赖；事前备份CdfQpTrajectoryRepair_before_anchor32.cpp，四源完整差异APF_anchor_first32_change.patch。
v32 Release主程序、Native与相关测试构建成功，四项领域回归全部通过（2.69秒）；Debug构建测试继续。独立bin=ryh1-anchor-first-bin，真实MainWindow44304从原ryh1/指定场景重算原全IK/全局Top1/CDF；job为v27名加_anchor32，配置ryh1-cfg32，唯一严格watcher39084等待两配置完成及真实GUI全部阶段后才验收。v31旧窗口12100/watcher31036核对身份后主动停止并留档retired_for_anchor32.json；未采用不完整路径。当前无最终无碰撞结果，ACTIVE不变，用户窗口29136未操作。


### 2026-10-18 v32双配置完成与开口内部调姿通道v39（实场未验收）
v32 Release/Debug主程序、Native和受影响测试均已构建成功，两配置四项领域回归全通过（2.69秒/49.01秒），全部冻结SDK DLL哈希不变，正常CMake恢复完成。宽锚点先行仍在6187-6204中段停滞，不能宣称修复；2026-10-18 02:33核对完整进程命令后主动停止原GUI44304/watcher39084，留档retired_for_channel39.json。不是自然失败，不是安全结果。
本轮没有再改C++、SDK、公共API或依赖。外部诊断在同一新场景实际FK/IK/完整RobotObject Exact检测器上进行：保持位置的姿态平滑609提议/520单点安全，相邻最大关节步48.60降33.61度，但抽样仍6条连接碰撞，未采用为最终路径。进一步比较双向倾角1680提议/656安全、缩短喷距1008/507安全，以及8个困难站点的倾角/方位/喷距1200/239安全。均只是状态提议，不是路径证书。
将这些实际单点安全候选作为原Top1之后的几何参考，局部DP按原turn差和小位置偏移代价选取；对所选候选弦仅做0.02rad抽样反馈。该辅助候选图不是原始全IK分层图，不修改原图或碰撞删边。第一轮99个源站点姿态/位置有变，最大相邻关节步24.23度，中部原1210/1214/1234/1238四条直接弦抽样已无碰撞；947/949两条仍需APF（不得把抽样当连续认证）。局部源站点偏移最大95mm，保留全部2400源站点及原起终位姿；原100mm源点/150mm连接、喷线命中内表面/60度参考方向/61度入射、0.1mm软余量等门禁不变。
新不可变job为ryh1_86f5af28c9d6_scene_b0b303aab044_g3stitch_wide27_channel39；新参考输入文件、profile FNV及静态SHA已更新并通过select_explicit_job核对。仍用经过双配置测试的v32隔离运行库ryh1-anchor-first-bin，不复用旧安全证书。真实MainWindow14068从原ryh1与指定ABB4600-burnner场景重新全IK/原全局Top1（523.623345047）/APF/单轮CDF-QP；唯一严格watcher47636等真实四阶段导出及独立完整复验/2400源点覆盖/主窗口播放通过再发布。当前没有完整无碰撞final，ACTIVE未改，用户自己的RSW窗口29136未操作。
诊断/候选选择/启动脚本和所有结果位于v27/output下的barrel_diagnostic33、smooth_diagnostic34、insertion_diagnostic35、inward_diagnostic36、rim_channel37、channel_refinement38及launch_channel39.py。只用于当前输入的安全姿态提议，不把旧ryh结果替代新输入。


v39实际APF阶段进展（2026-10-18 02:46，未最终验收）：真实MainWindow14068全IK/原Top1完成，调姿预检查未连接14降9；最大源点位移95mm、最大关节步24.228793度。Cartesian连续化保留全部2400源构型，2386边连续化、13边交APF、12082规划结点，固定位置调姿50降14次。12193加密点初检30个碰撞/形状区间；328.430秒进入整条APF检查，329.824秒通过门禁并开始安全窗口平滑。源码核对此日志仅在全段碰撞数为0且参考约束通过后出现，因此可确认APF阶段完整门禁已通过，但不能当最终CDF证书。目前第6/14次APF平滑（7种尺度两遍），后续仍单轮QP及最终独立验收。
额外0.0001rad喷向诊断发现原始直接弦1234附近入射角短暂越61度，说明粗采无碰撞不等于喷向合格；v39实际APF已自行处理，未放宽门禁。后续重复候选反馈诊断41已核对身份后主动停止，retired_redundant_diagnostic41.json留档，不采用其未完成候选。当前仅v39主窗口和逻辑唯一watcher47636（Windows venv另有实际Python子进程39404）运行；用户窗口29136未操作。v39/output/live_progress.json由write_live_progress.py生成，便于继续查看，final尚未发布，ACTIVE未变。


### 2026-10-18 v39最终验收与交付完成
实际MainWindow14068原始ryh1/ABB4600-burnner全IK共19200候选，原全局Top1代价523.623345046679，APF/单轮QP/安全平滑/减点完成，主窗口规划3435.110秒。12193加密点减为4221执行点；严格自适应检查发现17段需修补，其中14个平滑脉冲修补覆盖这些段，整路径重新认证。独立实际导出关节角0.0001rad密检2857936状态，碰撞/喷向/限位/离散速度加速度违规全0。最小节点距0.100517989mm，导出舍入扣减后连续间距保守下界0.009889816mm，不代表10mm或实测全程最小值。2400原站点覆盖通过，最大轴向1.996797322mm，站点最大位置99.981077739mm/RMS24.736899146mm，连接最大102.997985730mm，均在授权100/150mm范围。端点位置误差7.595184e-7m；时间参数化时长1027.596019秒。
自动验收首次因watcher默认GBK读取UTF8报告退出，只将该任务watcher改为utf-8-sig后续验。独立验证已成功后，复制GUI配置触发MAX_PATH；没有重算轨迹，任务内resume_verified_channel39.py重新校验输入/引擎/实际GUI导出hash、独立证书和全部站点覆盖，并用C:/b/ryh1-play39短配置继续原窗口录像。原MainWindow播放4221点全部检查，distancePassed/allPointsChecked/finished均true。随后正式发布本job/output/final、runtime.json、completed.json及ACTIVE。cached显式scene/job入口--no-view --no-record复验exit0，未重新计算。
新增Freeform_APF_QP/run_ryh1_burnner.cmd显式查看本次新场景结果，RYH1_BURNNER_RESULT.md说明算法、精确偏差与边界；output/delivery_manifest.json冻结最终文件sha/大小、原输入/场景与当前四私有源hash。当前源hash与v32双配置已构建/各4项领域测试通过时一致，SDK依赖未变；本次仅任务外部验收/启动脚本及文档调整，无新C++/公共API/依赖变化。
限制明确：安全结论仅ABB完整连杆含喷枪对A的关节插补。默认自碰撞器最后帧仍报Link6/Link5重叠，未过滤或声称解决。FANUC LS仅序列化核验，UFRAME2/UTOOL1/MoveL/CNT100未在目标工作站认证，110对点距<2mm、最小0.0273mm仍存在。QP只知执行一轮及五级回退，现有报告无acceptedQpSteps，不把全部平滑改善归因于QP。结果图已实际查看，工件右端仍有局部弯曲，未声称形状完全恢复或全局连续光滑/时间最优。


### 2026-10-19 ryh2 位置优先规划（执行中，未验收）
用户指定桌面ryh2.txt和同ABB4600-burnner场景，轨迹2400点，源SHA32d89774597ded44a7e11d98910550da2189bcedaf339cb7ce1f7d9ba626b50d。原法向姿态与ryh1一致，点位置沿法向约90mm变化，不能复用旧路径/证书。新不可变任务ryh2_32d89774597d_scene_b0b303aab044_pos30：原末端直接全IK19200候选、全局Top1代价510.2817922777992；原图不碰撞删边。后置仅采用旧已验收姿态作为提议，参考位置取真实FK以消除旧位置偏移，所有新位置/姿态重新实际IK及Exact完整喷枪对A检查。提高位置权重到32，源点上限先收紧30mm、连接仍150mm，60度目标方向/61度入射/2mm轴向覆盖/0.1mm软余量保持。
沿用冻结v32已双配置构建及各4项测试通过库，全部运行文件SHA重新校验；无C++/SDK/API/依赖改变。实际MainWindow30604从原ryh2重新全IK/Top1/生产APF单轮QP，唯一watcher33748，独立检验和原窗口播放通过后才发布，ACTIVE仍旧ryh1。新外部脚本在Freeform_APF_QP/ryh2_tools，完整结果尚未产生。验收脚本已提前指定UTF8及短录制配置C:/b/ryh2-play30。

ryh2首次pos30实际MainWindow30604在333.377秒因“原始站点12无安全喷涂姿态”失败，watcher按预期拒绝发布，未覆盖旧final/ACTIVE。原Top1实际节点检查2400点中900节点碰撞；端点各约50.65mm无碰撞。针对第12点891个实际模型IK/完整Exact碰撞/喷向探测中29个单点通过；原位置反向喷向另一内壁可通过，未作为首选；沿横向向内移动40mm并调方位可保持原喷涂一侧。不是整条路径证书，也不证明30mm内不存在可行解。
新不可变pos60任务只将源站点上限从30增60mm（用户已有100mm源点/150mm连接授权），位置权重32、2400源站点和工艺/碰撞硬门禁不变；真实MainWindow24760及唯一watcher20552从原ryh2重新全IK/全局Top1/生产服务。目前仅单点姿态准备，2319/2400旧姿态提议经新位置实际复验通过（663横向修补），尚无完整APF/QP结果。诊断与变体脚本ryh2_tools/probe_station.py、variant.py，未修改C++/SDK/依赖。

2026-10-19 ryh2补充姿态通道：pos60普通候选仍站点12失败，未发布。外部probe_channel.py实际新场景检查31200姿态提议，其中24525单点通过、覆盖2399源站点；station1499补充567提议又获得36个有效状态（包含保持原位置的解）。未采用station12诊断中的azimuth180反向内壁候选。select_channel.py仅对原Top1之后的安全调姿提议按原turn关节差/横向偏移连续性筛选，不改变原全IK图或删其边。新不可变pos60_channel1参考2400点，最大源点偏移50mm、RMS11.0943mm、最大原站点关节步49.5617度；这些仅单点/参考指标。
真实MainWindow42260+唯一watcher32504重新原ryh2全IK/原全局Top1，通道2400/2400复验通过，连接预检9未连接；Cartesian连续化后11266规划点，APF在243.256秒整条门禁通过并开始安全窗口平滑，目前12/14。仍未QP/最终认证/独立密检/播放验收，不发布final/ACTIVE。finish_gui_run.py本任务provenance中的raw ryh1纠正为raw ryh2，仅文字；原运行库/SDK/C++未变。ryh2_tools/deliver.py已准备但必须所有严格验证通过才可运行。

ryh2 pos60_channel1运行继续（MainWindow42260/watcher32504）：原APF整程通过，14次APF安全平滑、单轮QP（五档line search，报告不含accepted步数，不宣称QP本身改善全部形状）、QP后14次平滑、参考形状修整和时间参数化完成。11266规划点经1.5mm/.75deg容差及碰撞检查减到4536点。严格连续认证首遍耗时1717秒，发现53个未通过碰撞/喷向/10微米保守门槛的区间，不能简单称53个实际碰撞；48个安全窗口正弦平方脉冲修补覆盖全部这些区间，10:14第二遍全程重验到1500/4536，尚未最终通过。此处运行明显比旧ryh1慢，不能承诺分钟级结束。实际输出尚无完整final；独立0.0001rad密检、全部2400源站点复验、原窗口播放和导出序列核验仍需自动watcher执行并成功后才发布。
外部8个ryh2_tools脚本语法编译检查通过；本任务没有新C++/SDK/API/依赖修改，旧用户改动保留。deliver.py包含最终证书和原窗口播放硬检查，在所有检查通过前不得运行或创建ryh2_result；run_ryh2_burnner.cmd也由其最后生成，旧run_ryh1_burnner.cmd和旧已验证final不改。新结果目前仍未交付。


### 2026-10-19 ryh2 最终验收与交付完成
任务 ryh2_32d89774597d_scene_b0b303aab044_pos60_channel1 已完成。实际原MainWindow42260从原ryh2的2400个末端站点完成全逆解19200候选，原全连接图全局Top-1代价510.2817922777992；原图不作碰撞删边。后置横向移位与调姿候选、生产APF、单轮CDF/QP、安全平滑与减点完成，11266规划点减至4536执行点。主程序耗时6393.808秒，QP线性求解约8.6秒，主要耗时为安全平滑及连续认证；报告没有acceptedQpSteps，不将全部改善归因于QP。
连续认证首遍53个未通过区间经48次局部正弦平方脉冲修补后，第二遍全程通过。独立实际导出文件密检2736949状态，碰撞/喷向/限位/离散速度与加速度违规全部为0；2400原站点覆盖通过，最大位置偏差59.989840mm、RMS22.270650mm，端点位置/姿态误差2.2032245e-8m/1.8545997e-8rad。节点最小距离0.100157419mm，扣除导出舍入后的连续距离保守下界0.009876940mm。总时间参数化1034.852102秒，最大执行点关节步20.042914度。原MainWindow播放finished/distancePassed/allPointsChecked均true，4536点0碰撞，302可视距离帧无缺失。
完整结果在Freeform_APF_QP/runs/ryh2_32d89774597d_scene_b0b303aab044_pos60_channel1/output/final；便捷交付Freeform_APF_QP/ryh2_result，含关节角TXT、11111格式TCP、FREEFORM.LS、原始/APF/最终分图、距离图及动画。run_ryh2_burnner.cmd显式绑定新输入/场景/任务；缓存入口--no-view --no-record复验exit0，无重新计算。旧ryh1入口及结果保留。
安全结论仅完整ABB4600含长喷枪对A.STL的关节插补；未声明解决原场景Link6/Link5自碰撞重叠。最小余量很小，不代表10mm安全距离。FANUC使用UFRAME2/UTOOL1，序列化已核验，目标控制器编译和直线/CNT插补仍未验收；33对输出点间距低于2mm，最小导出点距0.061327mm。图中仍有局部避障弯曲，不宣称完全贴合、无折角或全局平滑最优。本轮无新C++/SDK/API/依赖修改，沿用既有v32双配置构建及各4项领域回归通过的冻结库；仅外部脚本、配置和文档更新，交付文件SHA核对通过。


### 2026-10-20 默认A.STL场景与删除恢复缺陷（实施中）
默认Qt启动配置切换为ABB4600-Aburnner.sys.json，保留用户已有机器人/工件位姿及轨迹。默认碰撞目标改为现有A对象，RobotObject优先，保留旧检测器ID兼容已有轨迹脚本。发现ensureCollisionSetup在工件缺失时自动push burnner RobotDesc，导致删除另存后在MotionPlanning初始化时复活；在领域服务移除隐式导入，缺失时报错不改变文档。ProjectScene喷涂射线测量扩展为STL对象，沿实际TCP+Z求最近正向三角形命中，应用模型单位和实时对象变换。后续构建/领域回归/原窗口检查及保存重开验证。事前备份C:/b/scene-A-fix-20261020，保留既有工作区修改，不触碰冻结脚本运行库。


### 2026-10-20 默认A.STL场景与删除恢复缺陷（完成验收）
Qt默认启动改为config/projects/ABB4600-Aburnner.sys.json；所给场景的机器人、工件位姿、毫米比例及完整轨迹扩展经JSON逐字段比较保持不变，仅对象sourcePath改为项目内data/Spray420/A.STL并增加ABB4600→A的Exact RobotObject检测器。A.STL从用户桌面同名文件复制，SHA一致。内部检测器ID cdf_abb4600_burnner_detector保留以兼容既有保存/脚本，显示名为CDF ABB4600-A.STL detector。Motion Planning默认目标A，对象优先，播放最近距离使用此独立检测快照。
领域服务ensureCollisionSetup移除缺失工件时自动push burnner RobotDesc的行为；缺失时报cdf_obstacle_missing且不改文档，不再隐式恢复删除的几何。旧场景已存在的URDF目标和显式RobotObject检测器仍兼容，既有参数字段保留，无新增公共API签名或SDK依赖。ProjectScene喷涂测量新增STL对象分支，CPU三角形应用AssetCore单位矩阵及实时对象位姿，沿实际TCP+Z取最近正向命中，原双面法向与带符号入射角规则保留；无命中返回无效，不伪造数值。喷涂导出和图标题更新为A.STL。
CMake配置成功，Release/Debug主程序与受影响领域测试均构建通过；SprayGuidance回归两配置通过（0.48s/2.45s），包含A优先于legacy URDF、删除保存重开、工件全缺失但显式提供旧URDF路径也不导入/不改变文档等检查。测试夹具最初缺名称、后因新增临时文件未清理而失败，均仅修正夹具并最终通过，未放宽业务校验。
两配置均在真实MainWindow默认启动场景中，使用12点调试轨迹完成动态播放、全部12点距离查询、最近距离TXT/折线图及喷涂距离/角度图检查，全部通过；独立读取原A.STL并按实际世界TCP求交，距离误差1.750851e-8m，角度误差1.897615e-5度。Release真实窗口额外执行缺工件场景加载→另存→再打开→再另存，两个保存文件均仅ABB4600，工件为空，无burnner复活。
运行程序C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe（Debug同目录配置下RobotQtViewerdx64.exe）。两配置部署场景与源码SHA一致；证据、事前备份、构建/测试日志、真实窗口报告/图与修改文件SHA清单在C:/b/scene-A-fix-20261020。既有用户ToolSetup、PlaybackWindowProfile等修改保留，冻结Freeform_APF_QP脚本运行库未动。Debug链接仍有既有OMPL缺PDB警告，不影响构建运行。本次未重新规划或重新认证此前交付轨迹。


### 2026-10-20 A.STL部署路径缺失黑屏（修复中）
用户实际运行外层build/Release/bin，存在旧data目录但没有新data/Spray420/A.STL；原MainWindow复现Assimp找不到该文件、ok=false。此前C:/b/rs105-merge没有本地data目录，main的源data回退使验证通过，遗漏了真实部署data目录的分支。改为A.STL与ABB4600-Aburnner.sys.json同目录发布，场景使用A.STL相对路径；保留原data/Spray420/A.STL兼容此前保存的场景。CMake将仅POST_BUILD复制配置改为RobotQtViewerRuntimeConfig依赖目标，每次增量构建也同步配置和场景资源，不创建不完整data根、不覆盖显式DATA_ROOT。将检查两种data布局的真实原窗口及重复构建部署。


### 2026-10-20 A.STL资源部署黑屏（完成修复）
实际用户EXE为外层build/Release/bin/RobotQtViewerrx64.exe；原窗口复现日志明确报Assimp failed to load model: data/Spray420/A.STL，场景初始化ok=false。其本地data根存在但缺新增文件，源data回退不会启用；此前C:/b测试data根不存在，故未覆盖该部署分支。本次将A.STL同SHA放入源码config/projects，与默认场景同目录；默认场景sourcePath恢复为A.STL。原data/Spray420/A.STL保留，另给实际build Release/Debug已有data/Spray420补同文件，兼容此前保存的旧路径；不建立不完整的新data根。
RobotQtViewer/CMakeLists.txt将POST_BUILD复制配置改为RobotQtViewerRuntimeConfig目标依赖，每次构建均复制config及场景内资源，增量无重链接也部署。C:/b与实际用户build均CMake配置成功，Release主目标构建成功，实际用户运行中的EXE未覆盖；仅执行用户build运行配置目标。Debug运行配置部署与重复Release增量部署通过。
验证使用实际用户EXE、实际原MainWindow：默认启动及打开源码新场景均ok=true且robots=1/objects=1/detectors=2；此前data/Spray420/A.STL旧路径场景也恢复。C:/b Debug原窗口打开新场景同样通过。三种场景均真实截图检查，非只判断进程返回值。C:/b Release12点播放、最近距离图、喷涂距离/角度图继续通过。配置及A.STL三处部署SHA一致；完整证据/事前备份在C:/b/scene-A-deploy-fix-20261020。只修改配置部署与资源位置，无运动学/碰撞判据/APF/QP算法变化，无新公共API或依赖。


### 2026-10-28 1.0.6 基础能力升级与 GA/APF/CDF 流程修复（实施中）
目标：保留 A 已有曲面、标定、后处理、GPU 膜厚和 FANUC 无初始轨迹场景，合入 12/1.0.6 的 SDK、整段运动校验、模型环境和构建/测试治理。
范围：cmake/包配置、PrebuiltPackages 的 Common/Core/Platform、ViewerCore/Qt 环境适配、ProjectMotionPlanning 及工作台。现有 main 分支有大量已暂存/未暂存/未跟踪业务变更，不重置；原件和补丁在外层 output/upgrade106_20261028/before。
不变量：检测器绑定当前机器人/实际工件且不隐式复活场景对象；GA 采用 RSW 默认参数，安全控制点经 APF 连接后才可选 CDF；失败/取消不得发布为无碰撞成功；模型环境仅为背景。用户 1.txt 只读。
资源：data/Desktop/environments/factory-detailed 与 data/Desktop/MLRobotUBO.*，需同步公共 shader include 并保持现有业务 shader 兼容。
验证：独立短目录 CMake 配置，Release/Debug 主程序及领域测试；新增接口/环境真实 GL 回归、检测器/距离绘图回归；真实 1.txt 的 GA→APF→可选 CDF、结果验证和日志。实际失败须记录，不能借用上一轮结果。


### 2026-10-28 1.0.6 合入与规划流程修复（完成基础验证）
已在本源码合入 1.0.6 SDK/整段保守运动接口、真实 glTF 厂房环境及构建/测试治理；RSW 业务能力和本轮开始时的 FANUC 无初始轨迹 JSON 保留。检测器按当前机器人/实际工件绑定并识别 selection set；最近距离图支持直接查询关节轨迹。
工作台调用新增领域 GaApfCdfPipeline：实际保存 TCP、RSW 原参数 GA 安全点、APF 点间采样与独立回放、可选 CDF；失败/取消不发布半成品。修复 GA 内部候选数组未初始化崩溃、导入后按钮状态，以及加密 CDF 重新计时时的笛卡尔时间映射错误；新增回归确认真正接受 CDF。无现有 API 重命名或新增第三方依赖。
C:/b/rs106-20261028 的 Release/Debug 最终全目标构建通过；Release 71/71 全量回归通过，Debug 13/13 受影响回归通过。Debug 全量在旧大模型 CDF 查询长用例处主动停止，不称为全量通过；同项 Release 通过。并行测试占用 DLL 导致的一次 Debug 部署失败，在退出测试后完整重建消除。
两配置原生及原 MainWindow 的 3 源点短轨迹均接受 GA/APF/CDF，最终 5 点，距离全部有效且无碰撞；Release 关闭 CDF 的流程也通过。模型环境真实 OpenGL 两配置通过，厂房实际截图已检查。SDK 504 个参考文件与 1.0.6 SHA 一致；修改源码 CRLF 检查通过。
用户 1.txt 原生及真实 MainWindow 均完成实际失败测试：13 个源点，GA 单点 180 秒预算耗尽，碰撞/挡板拒绝，未找到全部安全控制点，因此 APF/CDF 未启动、最终导出禁用；没有完整成功轨迹或连续无碰撞认证。原文件 SHA 不变。GA/APF/CDF 采样门禁不等同于新 SDK 保守运动证书；厂房只作显示背景。
详见 docs/upgrade_1_0_6_integration_20261028.md；备份、修改/资源 SHA 清单和全部证据在外层 output/upgrade106_20261028。运行更新程序请用 C:/b/rs106-20261028/Release/bin/RobotQtViewerrx64.exe，原用户 build 旧窗口未替换。


### 2026-10-28 用户指定 GA 表面球冠与渐进变半径（实施中）
本轮按用户要求修正 GA→APF→CDF 工作台：UI 输入原始喷距（默认100mm），球心 S=P+d*原始TCP+Z，半径 d、法向半角80度、均匀面积随机5000候选；第3/4/5轮仍无安全姿态才开放±30/50/80%原始喷距。原球心与姿态参考不可随结果/重新计时漂移；后续阶段保持实际开放的范围。真实碰撞、安装工具和虚拟挡板检查继续生效，失败不能发布成功。现有大量重叠改动保留，事前副本在外层output/ga-surface-20261028/before。所有语义由ProjectMotionPlanning拥有，Qt只收集喷距。计划涉及领域流程/已有搜索/工作台/回归约10文件，不新增第三方依赖。验证球冠均匀分布/映射/分轮范围/安全父代，以及Release/Debug受影响构建测试和真实1.txt；明确区分搜索未完成与无解。


### 2026-10-28 GA球冠搜索实现与RSW差异定位（已实现，验证边界已记录）
A新增原始喷距UI输入，固定S=P+d*n、80度、5000均匀面积初始池及安全父代进化；3/4/5轮无安全时开放±30/50/80%，原始参考与半径范围传过APF/CDF及重新计时。取消工作台单点180秒限时，默认0无限时，评估上限与取消保留。Release受影响主程序/工具/测试最终构建部署通过，7/7领域回归；真实MainWindow以100mm接受3源点GA/APF/CDF，5点距离均有效无碰撞，导出可用。跨工作台4/4通过；Debug构建通过，6/7回归通过，新增不可能安全壳体夹具的ExternalTrajectoryEvaluation达CTest900秒超时，不称全通过。
1.txt原生100mm实际完成5000池+8轮、末点约679秒21976次仍无安全，验证进程主动停止，不是无解证明，未进入APF/CDF。用户转问RSW差异后，单变量对照定位TCP：A默认yaw约0度，RSW成功保存场景约180度；只改测试副本yaw使原始安全点1/13变7/13，与RSW一致。当前A原生程序复核RSW已知50点轨迹，yaw0报spray_target_error、yaw180全部采样通过。另确认A轴转开关默认false而Agent显式true，以及安全点重搜、进化顺序、后续喷距范围等不同。RSW默认JSON也约0度，不能冒认历史成功场景为默认配置。正式场景与1.txt未改。详见docs/ga_surface_search_20261028.md与docs/ga_rsw_comparison_20261028.md；证据/备份/对照在外层output/ga-surface-20261028。


### 2026-10-28 按RSW对齐GA并重新验证1.txt（实施中）
用户明确要求正式TCP yaw=180度、开启轴转、已安全点不重搜、保留原固定半径24后代并额外演化变半径后代、GA后固定选定逐点喷距。领域层恢复RSW搜索调度并保留取消单点超时；原始球心/法向参考不可漂移，实际碰撞/工具/虚拟挡板门禁保留。改动约7个源码/配置文件，无新依赖；源码与场景备份在外层output/ga-rsw-align-20261028/before。验证固定距离传递/重新计时、已安全点跳过与双种群完整轮次，Release/Debug受影响构建与实际100mm、原始1.txt的原生及UI流程；失败不得当作完成轨迹。


### 2026-10-28 RSW GA 对齐与原始1.txt验收完成
正式 FANUC 场景仅工具TCP yaw改为180度；GA启用喷射轴旋转，已安全控制点保留有效原解并跳过5000池重搜。恢复RSW每轮24个固定半径后代及随机序列，第三/四/五轮无安全时独立追加变半径后代，不消耗固定半径名额；单点0无限时、取消与131072评估上限保留。GA最终按实际FK锁定每个原始控制点喷距，APF/CDF/重新计时/独立回放沿用；修复原始球心检查误用选定喷距的缺陷。领域结果新增gaSelectedSprayDistances（米），无API重命名或新增第三方依赖。
原始桌面1.txt在100mm、真实挂载工具及虚拟挡板开启下，原生与真实MainWindow均完成GA→APF→CDF：13源点→2398 APF点→4795 CDF点，CDF实际接受、非回退。原生最终独立回放和4795点距离/工艺核查通过，最小节点距离1.347281mm，最大瞄准误差0.000995mm、喷距误差0.000988mm；13点喷距均锁定100mm。本输入未触发扩半径，扩半径完整轮次由实际模型回归验证。UI最终导出可用、最近距离图4795/4795有效、0碰撞、0无效，最小节点距离1.377186mm；窗口和图已实际检查，厂房显示正常。
Release/Debug受影响主程序、领域库和工具完整构建通过。最终Release领域7/7及跨工作台4/4通过；Debug六项CTest通过，综合测试首轮900秒超时位于不可能间隙夹具，夹具收紧为恰好216次完整进化预算后直接运行完整退出0，生产预算与安全判据未变。一次测试程序部署因运行窗口占用lib_coacd.dll失败，窗口退出后重建成功。保留初始失败记录，不覆盖用户运行窗口。
证据/备份/本轮差异/哈希清单在外层output/ga-rsw-align-20261028；详情docs/ga_rsw_alignment_20261028.md。运行程序C:/b/rs106-20261028/Release/bin/RobotQtViewerrx64.exe。源场景与Release/Debug部署SHA一致，1.txt SHA仍为23d5a3e1dbcb5d43b0e5ec8cade0588ef9a964973983932c4eac907b79cdae45。结论为实际模型采样碰撞/挡板/工艺门禁与独立回放通过，不是保守连续运动证书；未宣称连续加速度或控制器动力学认证。既有工作区改动保留，RSW与原始1.txt未改。


### 2026-10-28 用户VS2019旧build链接错误修复
用户实际打开外层build/RobotSimulator2026.sln，Release/x64报LNK1120。直接读取其RobotQtViewer.log定位LNK2019/LNK2001：validateProjectSceneEnvironment缺失。源码有实现且CMake已列入；dumpbin证实旧ProjectSceneEnvironment.obj（38291字节，2026-10-28 11:09）没有该符号，新构建对象有。合入源文件保留2026-10-26时间戳，旧build增量判断错误而复用旧对象。此前验收C:/b/rs106-20261028新构建未覆盖用户旧缓存。
已在用户原build定向Rebuild RobotViewerCore（Release/x64，不清理整个解决方案），随后cmake --build build --config Release --target RobotQtViewer --parallel 4完整构建通过；未改业务源码、CMake或调试配置。直接运行实际build/Release/bin/RobotQtViewerrx64.exe，从VS默认项目工作目录启动，加载部署场景，ok=true，保存真实视口截图并正常退出0。用户当前Release/x64链接故障已消除；未声称重建其旧build Debug配置。
证据在外层output/vs2019-link-fix-20261028：original-link-error.log、旧对象备份、core/viewer构建日志、vs-build-startup.log/png、diagnosis.json。若后续再次覆盖保留旧时间戳的源码，需对受影响项目“重新生成”以更新目标文件；本次无需重新写函数或取消环境功能。


### 2026-10-28 GA/APF速度与输出点预算（实施中）
目标：QP默认关闭；13源点最终不超过100节点、按原始区间均匀分配；在原有5000池/8轮、碰撞/挡板/工艺容差不变条件下优化计算。领域ProjectMotionPlanning拥有点预算与安全检查，Qt仅切换CDF；优先解耦输出节点与0.001rad安全采样、减少重复runtime FK和挡板全网格变换，随后量化GPU与CPU热点。现有main大量改动保留，事前源码与Git状态在外层output/ga-speed-20261028/before。验证Release/Debug受影响构建回归、原始1.txt100mm原生和真实UI，再部署实际外层build Release。用户后续明确100点为软目标，必要时自动增加节点并均衡区间，不因点数目标报失败；20秒为实测目标，不降低安全判据。无新依赖。


### 2026-10-28 GA/APF 加速与均匀约100点输出（验收完成）
按用户要求把 CDF/QP UI 和设置默认值改为关闭，保留手动勾选；用户随后明确 100 点仅为软目标，领域层不因超过目标点数报失败。将轨迹支持节点与0.001rad安全采样解耦，按原始区间均匀分配，必要时保留额外工艺细分并均衡区间；关闭流程中的盲目二次加密。GA 5000池/8轮/固定与变半径后代、实际工具/实体/挡板/奇异性/喷距/时间门禁均保留。
加速包括经实际模型验证的 POE FK/解析雅可比、CPU逆解回退与精确安全检查的独立场景线程池、精确距离/状态缓存、重复初值IK尝试复用、CPU挡板BVH与光线包含筛选，以及保留完整网格归一化的稀疏GPU评分和较大有界kernel批次。不同阶段仍新建场景独立回放，不跨阶段复用安全结论。领域新增可选preferredOutputWaypoints/worldJacobian，无公共API重命名和新第三方依赖；所有受影响依赖已重编译。
原始1.txt、100mm、正式FANUC挂载及虚拟挡板开启：最终原生24.121秒，用户实际外层build原MainWindow按钮24.235秒；13源点输出100点，12区间分成8或9子段（新增7或8点）。100点距离有效/0碰撞/0无效，最小节点距离0.233444mm；工艺和新的独立回放通过。最终并行/稀疏GPU结果与本轮第一版串行100点结果最大关节差0。显式开启CDF原生95.913秒，CDF实际接受、最终仍100点并通过独立回放；此前完整含CDF基线1134.35秒。约24秒接近20秒目标，不承诺固定20秒，不混淆关闭QP与开启QP的功能差异。
用户实际build Release主程序/原生工具及C:/b独立Release工具/领域测试、Debug主程序/工具均构建通过；Release领域7/7与实际build跨工作台4/4最终通过。Release/Debug GPU数值、解析雅可比验证及512组BVH/逐网格对照通过。Debug原MainWindow短轨迹3→100点、默认QP关闭、距离图及导出通过（152.487秒，测速使用Release）。未重复此前长达11分钟的Debug综合用例，不声称Debug全量通过。
首次新测试编译误用TransformDesc接口后已修正；外层build跨工作台初次因测试EXE未构建为Not Run，补建后发现旧用例假设QP默认开启，已改成默认关闭断言+显式选择QP，最终4/4通过，保留初始记录。源C++保持CRLF；大量既有改动保留，RSW/厂房data/正式场景及桌面1.txt未改，输入SHA仍23d5a3e1dbcb5d43b0e5ec8cade0588ef9a964973983932c4eac907b79cdae45。
本轮为细采样门禁和独立回放通过，不是保守连续运动证书。详见docs/ga_apf_speed_20261028.md；备份、限定本轮差异/哈希、构建/回归、分阶段计时、完整轨迹、真实UI与图证据在外层output/ga-speed-20261028。运行更新后的外层build/Release/bin/RobotQtViewerrx64.exe，VS2019使用Release/x64。

### 2026-10-28 用户 VS2019 MSB8066 配置并发修复
用户原外层 build 的 Release CollisionDetectorConfig.log 证实：多个工作台项目同时执行 --check-stamp-file，因源码/CMake 与 cmake.verify_globs 时间戳变化触发全局重新配置。并发配置争用 pinocchio 的 build/cmake/tmp-cxx-standard.cpp，并在 install_headers 的 REMOVE_RECURSE/复制之间发生竞争，导致 write_file/configure_file 失败，汇总为 MSB8066；错误指向的 CollisionDetectorConfig/CMakeLists.txt 不是语法错误根因。
在顶层 CMakeLists.txt 的最低版本声明之后、依赖准备与 project() 之前加每个 binary tree 独立的 file(LOCK ... GUARD PROCESS TIMEOUT 180)，串行保护本工程的配置期写入。相对本轮备份仅新增此锁块，保持 CRLF、原有用户改动、自动重新生成、Debug/Release 分离；无业务代码/API/依赖变化。
用户实际 build 重新配置、Release CollisionDetectorConfig 与 RobotQtViewer 构建及重复增量构建退出0；Debug CollisionDetectorConfig 构建退出0。刻意并发启动两次真实 CMake 配置，第二次在第一次活动期间没有进入 project() 输出，176次100ms观察均等待，两进程最终退出0。Release 实际主程序从 VS 默认项目工作目录启动，场景/厂房/机器人加载 ok=true，截图已检查，正常退出0。未重建 Debug 全部主程序或重跑轨迹性能测试，本轮修复仅涉及配置生成并发。
证据、事前 CMakeLists/cache/错误日志、两次并发日志、构建日志及启动截图在外层 output/vs2019-msb8066-20261028，摘要 diagnosis.json。VS 已打开旧工程时应接受重新加载修改后的项目，或重新打开外层 build/RobotSimulator2026.sln，再用 Release/x64 启动 RobotQtViewer。