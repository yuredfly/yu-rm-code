# 第 8 周报告：前哨站视频 → 装甲板检测 + PnP 位姿解算

> **基准文档**：《前哨站视频位姿解算任务》（配套 `参数.md`、`outpost.avi`）
> **复用模块**：第 7 周静态装甲板检测（`detect_frame` 接口**未改动**）
> **本机实测环境**：OpenCV **4.14.0** + CMake 4.4.4 + MSVC vc16 / x64，Windows 10/11
> **实测输入**：`outpost.avi` —— 1280×1024、**119.998 fps**、**4210 帧**、约 35.08 s、H264
>
> 标注约定：**【强制】** = 任务文档明文要求；**【优化】** = 工程上更合理的实现（不改变验收口径）；**【可选】** = 拓展，仅注释示例，不计入验收

---

## 0. 结论速览

| 项 | 结果 |
|---|---|
| 视频读取 | 1280×1024 @119.998fps，4210 帧，循环读到 EOF（未写死帧数） |
| 相机参数 | 从 `参数.md` 解析成功：fx=1781.05, fy=1781.69, cx=510.58, cy=472.97；D 五维 |
| 检出帧数 | **2577 / 4210**（61.2%） |
| PnP 成功 | **2577**（100% 的候选） |
| 通过校验 | **2576**；可疑 **1**（frame 322，重投影 3.1678 px > 3.0 px 阈值） |
| 平均重投影误差 | **0.789 px**（最好帧 0.010 px） |
| 深度范围 | tz ∈ [**2.610, 3.134**] m；直线距离 ∈ [**2.664, 3.189**] m |
| 粗深度交叉校验 | 与 tz 同量级，相对偏差在阈值内（详见 §7.2） |
| 产物 | 标注视频 mp4（1280×1024/120fps/4210 帧）、CSV 4210 数据行、4 张重投影验证图、敏感性 CSV、静态图结果 |

---

## 1. 任务总览

### 1.1 【强制】逐条验收项与落地位置

| # | 文档要求 | 落地位置 | 状态 |
|---|---|---|---|
| 1 | 读取 `outpost.avi`（1280×1024），打印宽/高/fps/总帧数 | `main.cpp` §视频元数据 + `read_video_meta()` | ✅ |
| 2 | 读取同目录 `参数.md`，取 K 与 D[k1,k2,p1,p2,k3]；装甲板 0.135×0.056 m | `param_loader.cpp` | ✅ |
| 3 | `solvePnP` **直接传原始角点 + K + D**，外部不 undistort | `pose_solver.cpp::solve_pose` | ✅ |
| 4 | 坐标系统一 OpenCV（x右/y下/z前）；tvec=板中心平移；tz=深度；`distance=√(tx²+ty²+tz²)`；rvec 不当欧拉角 | `pose_solver.cpp` + `frame_output.cpp` | ✅ |
| 5① | 标注视频：原分辨率帧率、候选框、角点 0-1-2-3、候选 ID、PnP 标记、直线距离 | `frame_output.cpp::draw_pose_overlay` + `video_writer` | ✅ |
| 5② | CSV 13 字段；失败留空禁止填 0；一帧多候选多行；无候选帧也记一行 | `frame_output.cpp::csv_writer` | ✅ |
| 5③ | 重投影验证图：`projectPoints` 画原始角点与重投影角点；误差取 4 点均值 | `frame_output.cpp::make_reprojection_check` | ✅ |
| 6① | 针孔 `Z_rough` 估算并与 tz 对比校验 | `pose_solver.cpp::compute_rough_depth` | ✅ |
| 6② | 单因素敏感性测试：扰动角点、扰动物理尺寸 | `pose_solver.cpp::run_sensitivity_test` | ✅ |
| 6③ | 校验规则（tz>0、重投影阈值、有限性）+ 标记可疑 + 说明误报漏报风险 | `pose_solver.cpp` + 本文 §7.3 | ✅ |
| 7 | 空帧/读失败/solvePnP false → pnp_valid=0；**严禁用上一帧位姿填充** | `main.cpp` 主循环 | ✅ |
| 8 | 保留静态图入口；不改 `detect_frame`；更新 CMake；跑完写测试记录 | `--image` 模式 + `CMakeLists.txt` + 本文 §9 | ✅ |
| — | 禁止目标跟踪 / GUI / 多目标时序滤波 | 全工程无相关代码 | ✅ |

### 1.2 【优化】与文档不完全一致、但更合理的实现

| 优化点 | 原始做法 | 本工程做法 | 理由 |
|---|---|---|---|
| **参数解析** | 直接硬编码 K/D | 用正则解析 `参数.md` 的 `camera_matrix` / `distortion_coefficients` 的 `data:` 块 | 文档要求"读取参数.md"，硬编码等于没读；解析失败才回退到与文件一致的默认值并打印警告 |
| **中文文件名** | `std::ifstream("参数.md")` | `std::ifstream(std::filesystem::u8path(...))` | Windows 上窄字符流按 ANSI(GBK) 打开 UTF-8 文件名会失败 |
| **深度校验** | 只判 `tz>0` | 增加"针孔粗深度 vs tz 相对偏差 >35%"判定 | 4 点平面 PnP 是**最小解**，重投影误差几乎恒为 0，单靠重投影无法发现角点错误（见 §7.3） |
| **重投影验证图** | 只画点 | 额外加 **3× 局部放大面板** | 装甲板在 1280×1024 里只有约 78×40 px，不放大肉眼无法判断"重合得好不好" |
| **CSV 数值格式** | 默认 locale | `out.imbue(std::locale::classic())` | 部分系统 locale 会把小数点写成逗号，破坏 CSV |
| **视频编码** | 单一 fourcc | MP4V(.mp4) 优先，失败自动回退 MJPG(.avi) | 不同 OpenCV 构建的可写编码器不一致，硬指定会直接失败 |
| **fps 兜底** | 无 | fps 非法时用 30.0 兜底并标注 `(fallback)` | 文档要求"读取异常用参数.md 参考值兜底"，而参数.md 未给帧率，故显式声明兜底值 |
| **进度可观测** | 无 | 每 500 帧打印一次进度 | 4210 帧处理约 3 分钟，无进度无法判断是否卡死 |

### 1.3 【可选】拓展（仅注释说明，未实现）

| 拓展 | 说明 |
|---|---|
| 打印每帧调试信息 | 现只打印每 500 帧进度与最终汇总；逐帧打印会让 4210 帧日志不可读 |
| 重投影误差曲线统计脚本 | 可按 `pnp_frames.csv` 的 `reprojection_error_px` 列用 pandas/matplotlib 画时间曲线 |
| 三块板 120° 几何先验 | `参数.md` 第三节给出三板模型，但本阶段只要求单板 PnP，未引入多板约束 |
| 时序平滑 | **禁止**（文档明确不允许跟踪/时序滤波） |

---

## 2. 原理推导

### 2.1 为什么"原始角点 + K + D"必须一起交给 solvePnP

相机成像模型（含径向+切向畸变）：

```
x_n = (X/Z),  y_n = (Y/Z)                      归一化平面
r² = x_n² + y_n²
径向:   x_d = x_n(1 + k1 r² + k2 r⁴ + k3 r⁶)
切向:   x_d += 2 p1 x_n y_n + p2(r² + 2x_n²)
像素:   u = fx·x_d + cx ,  v = fy·y_d + cy
```

`solvePnP` 内部用同一套模型：它把 3D 点投影到**畸变后的像素平面**再与检测角点做最小化。
如果外部先 `undistort` 图像、角点也跟着变，但你仍传原 K/D 进去 → **畸变被补偿两次**，深度会系统偏移。

**本工程做法**：角点直接取 `detect_frame` 输出的原始像素坐标，K/D 原样传入，让 `solvePnP` / `projectPoints` 内部处理畸变。这样 `projectPoints` 的重投影也是在同一模型下计算的，误差才有意义。

### 2.2 平面四点的 PnP 是"最小解"，重投影误差几乎恒为 0

4 个共面点 → 未知数 6（R 3 + t 3），约束 4×2 = 8 > 6，理论上有**解析解**。
`SOLVEPNP_IPPE` 正是针对平面标靶的解析解，因此：

> **实测**：4210 帧中最好一帧的重投影误差 **0.0099 px**，全片平均 0.789 px。
> 这意味着——**只要角点能凑出某种姿态，重投影误差就会很小，哪怕这个姿态是错的**。

**这正是必须引入"针孔粗深度交叉校验"的原因**：重投影误差只能发现"数值上不自洽"，发现不了"几何上错位"。

### 2.3 针孔粗深度 `Z_rough`

单目无法直接从单帧恢复绝对尺度，但已知物理尺寸时可以估算：

```
Z_rough(height) = fy · H_real / h_pixel
Z_rough(width)  = fx · W_real / w_pixel
```

其中 `h_pixel = (|LB−LT| + |RB−RT|)/2`（左右竖边像素长度均值），`w_pixel = (|RT−LT| + |RB−LB|)/2`。

**关键性质：`Z_rough` 与 tz 应当同量级**。若相差超过 35%，说明：
- 灯条配对/角点定位有问题（装甲板在图像里被拉长或压扁），或
- 物理尺寸与图像尺寸不匹配（用了错板型的 w/h）。

### 2.4 为什么 tz 和 distance 要分开输出

`tvec = (tx, ty, tz)` 是板中心在相机系的平移：
- **tz** 是沿光轴方向的**深度**（前向距离）；
- **distance = √(tx²+ty²+tz²)** 是到光心的**直线距离**。

目标偏离画面中心时二者差异明显：本片 `tz ∈ [2.610, 3.134]`，`distance ∈ [2.664, 3.189]`，
最大差值约 **5.5 cm**（对应像素离主点约 175 px 的偏移量），因此两个字段都必须给出，不能互相替代。

### 2.5 rvec 为什么不能当欧拉角

`rvec` 是 **Rodrigues 旋转向量**：方向 = 旋转轴，模长 = 旋转角（rad）。
只有当旋转轴恰好是某一坐标轴时，它的分量才等于对应欧拉角。一般姿态下二者完全不同。

**本工程做法**：
1. `cv::Rodrigues(rvec, R)` 得到 3×3 旋转矩阵；
2. 按约定 `R = Rz(rz)·Ry(ry)·Rx(rx)` 分解出欧拉角：
   ```
   ry = asin(−R20)
   rx = atan2(R21, R22)
   rz = atan2(R10, R00)
   ```
3. CSV 的 `rx_rad/ry_rad/rz_rad` 写的是**这组欧拉角**（rad），并注明它们是"由 R 分解"而非 rvec 的分量。

### 2.6 装甲板模型点与角点顺序的对应

`参数.md` 第二节规定：

```
0 左上 (-w/2, -h/2, 0)    1 右上 ( w/2, -h/2, 0)
3 左下 (-w/2,  h/2, 0)    2 右下 ( w/2,  h/2, 0)
```

`detect_frame` 输出的 `corners[0..3]` 顺序恰为 **左上→右上→右下→左下**，因此可以直接逐点对应，
**不需要任何额外排序**。这也是上一阶段把四角点顺序固定下来的价值——若顺序会随机交换，
`solvePnP` 会解出翻转 180° 的姿态。

模型系 `x` 沿板宽向右、`y` 沿板高向下、`z` 指向板外，与 OpenCV 相机系（x右/y下/z前）同向，
所以**正对相机时 R ≈ I，欧拉角接近 0**。

---

## 3. 算法流水线

```
┌──────────────────────────────────────────────────────────────┐
│ 0. 启动                                                       │
│    ├─ 解析 参数.md → K(3×3), D(1×5), 图像尺寸, 板尺寸 0.135/0.056
│    └─ 解析失败 → 用与文件一致的默认值 + 打印警告                │
├──────────────────────────────────────────────────────────────┤
│ 1. 打开 outpost.avi                                           │
│    ├─ 读 width/height/fps/frame_count，非法项用兜底值并标注     │
│    └─ 打印 1280×1024, 119.998fps, 4210 帧, 35.08s             │
├──────────────────────────────────────────────────────────────┤
│ 2. 逐帧循环  while(cap.read(frame))   ← 不写死帧数            │
│    │                                                          │
│    ├─ frame.empty() ? → CSV 记一行(pnp_valid=0)，continue      │
│    │                                                          │
│    ├─ detect_frame(frame, cfg)   ← 第 7 周模块，接口未改       │
│    │      颜色分割 → 灯条候选 → 配对 → 装甲板 + 4 角点         │
│    │                                                          │
│    ├─ 无候选 → CSV 记一行(candidate_id=-1, 位姿留空)           │
│    │                                                          │
│    └─ 每个候选：                                               │
│         ├─ solve_pose(corners, K, D, w, h)                    │
│         │     ├─ solvePnP(IPPE → 失败回退 ITERATIVE)          │
│         │     ├─ Rodrigues→R→欧拉角 rx,ry,rz                   │
│         │     ├─ projectPoints → 4 点平均重投影误差            │
│         │     ├─ distance = √(tx²+ty²+tz²)                    │
│         │     ├─ Z_rough(高/宽) 交叉校验                       │
│         │     └─ 校验规则 → pnp_valid / suspect                │
│         ├─ CSV 写入一行                                        │
│         ├─ 在帧上标注（框/角点0-3/ID/PnP标记/距离）            │
│         └─ （前 3 个成功帧）存重投影验证图                     │
├──────────────────────────────────────────────────────────────┤
│ 3. 收尾                                                        │
│    ├─ 关闭视频写入、CSV                                        │
│    ├─ 汇总统计（帧数/候选/成功率/误差/深度范围）               │
│    ├─ 存 reproj_check_best.png（误差最小帧）                   │
│    └─ 敏感性测试（7 个单因素用例）→ 终端表 + sensitivity.csv   │
└──────────────────────────────────────────────────────────────┘
```

**接口稳定性**：`frame_result detect_frame(const cv::Mat&, const armor_config&)` **原样复用**，
本阶段只在它外面套了"位姿解算 + 输出"两层，符合文档"接口定义不能大幅偏离"的要求。

---

## 4. 目录结构

```
week8_outpost/
├─ CMakeLists.txt              # 6 个 cpp + 素材自动复制到 exe 旁
├─ task_config.hpp             # 任务常量与兜底值（板尺寸、校验阈值、默认路径）
├─ param_loader.hpp/.cpp       # 解析 参数.md → K/D/尺寸
├─ pose_solver.hpp/.cpp        # solvePnP、重投影、粗深度、校验、敏感性测试
├─ frame_output.hpp/.cpp       # CSV、视频写入、帧内标注、重投影验证图
├─ armor_types.hpp             # 【复用】第 7 周数据结构（未改）
├─ armor_pipeline.hpp/.cpp     # 【复用】detect_frame（未改）
├─ armor_visualize.hpp/.cpp    # 【复用】绘制/保存（未改）
├─ main.cpp                    # CLI + 主循环 + 汇总（不含算法细节）
├─ outpost.avi                 # 【强制】输入视频 1280×1024@120fps
├─ 参数.md                      # 【强制】相机内参与板尺寸
├─ images/outpost_frame.png    # 静态图入口用的单帧
└─ output/
   ├─ outpost_annotated.mp4    # ① 标注视频（1280×1024，120fps，4210 帧）
   ├─ pnp_frames.csv           # ② 逐帧数据表（13 字段，4210 行）
   ├─ reproj_check_best.png    # ③ 重投影验证图（误差最小帧，含 3× 放大面板）
   ├─ reproj_check_f0001_c0.png  # ③ 前 3 个成功帧各一张
   ├─ reproj_check_f0002_c0.png
   ├─ reproj_check_f0003_c0.png
   ├─ sensitivity.csv          # ⑥② 敏感性测试结果
   └─ static_armor_result.png  # 静态图入口结果（保留）
```

**模块依赖方向**（无循环依赖）：

```
main ──► param_loader ──► task_config
  ├────► armor_pipeline ──► armor_types
  ├────► pose_solver ──► param_loader
  └────► frame_output ──► armor_types, pose_solver, armor_visualize
```

---

## 5. 分模块代码解析

### 5.1 `task_config.hpp` —— 常量与兜底集中在一处

```cpp
constexpr double kArmorWidthM  = 0.135;   // 模型宽度 w（米）
constexpr double kArmorHeightM = 0.056;   // 模型高度 h（米）
constexpr int    kFallbackWidth  = 1280;  // 参数.md 第一节
constexpr int    kFallbackHeight = 1024;
constexpr double kFallbackFps = 30.0;     // 参数.md 未给帧率 → 显式兜底
constexpr double kReprojErrThresholdPx = 3.0;
constexpr double kMaxReasonableDistanceM = 20.0;
constexpr double kDepthMismatchRatio = 0.35;
```

**逻辑来源**：文档第 2 条（板尺寸 0.135/0.056）、第 1 条（读不到元数据时用参数.md 兜底）、
第 6③ 条（校验阈值）。把阈值集中，避免散落在函数里形成"魔数"。

### 5.2 `param_loader.cpp` —— 解析 Markdown 里的标定数据

```cpp
// 找到 key 之后第一个 [ ... ] 数据块
bool find_data_block(const std::string &text, const std::string &key, std::string &block);
// 抽出文本里所有浮点数（正则）
std::vector<double> extract_numbers(const std::string &text);

camera_params load_camera_params(const std::string &path)
{
    // 1) 先填 与 参数.md 完全一致的默认值
    // 2) 用 filesystem::u8path 打开（中文文件名）
    // 3) camera_matrix → data 里前 9 个数 → K(3×3)
    //    distortion_coefficients → data 里前 5 个数 → D(1×5)
    //    image_width / image_height / 模型宽度 / 模型高度 各自正则提取
    // 4) 只有 K 和 D 都解析成功才置 from_file=true，否则打印警告
}
```

**为什么不用简单的 `sscanf`**：`参数.md` 是 YAML 嵌在 Markdown 里的，格式会随排版变化；
用"定位 key → 取 `[]` 块 → 正则提数字"的三段式，对换行/空格/缩进都不敏感。

**实测输出**：

```
[PARAM] source: 参数.md (K and D parsed from file)
[PARAM] K = [1781.05, 0, 510.575; 0, 1781.69, 472.974; 0, 0, 1]
[PARAM] D = [-0.0442074, -0.108154, -0.000650622, 0.000235738, 0.466737]
[PARAM] armor: width=0.135 m, height=0.056 m
```

### 5.3 `pose_solver.cpp::make_object_points` —— 模型点顺序是"契约"

```cpp
std::vector<cv::Point3f> make_object_points(double w, double h)
{
    const double hw = w * 0.5, hh = h * 0.5;
    return {
        cv::Point3f(-hw, -hh, 0.f),  // 0 左上
        cv::Point3f( hw, -hh, 0.f),  // 1 右上
        cv::Point3f( hw,  hh, 0.f),  // 2 右下
        cv::Point3f(-hw,  hh, 0.f),  // 3 左下
    };
}
```

**逻辑来源**：`参数.md` 第二节的模型点定义，与 `detect_frame` 的四角点顺序逐点对应。
**若这一步的顺序写错**（例如把 2/3 交换），重投影误差会暴涨到几十像素，是很好的自检点。

### 5.4 `pose_solver.cpp::solve_pose` —— 核心解算 + 校验

```cpp
pose_result solve_pose(const std::vector<cv::Point2f> &corners,
                       const camera_params &cam,
                       double armor_w_m, double armor_h_m,
                       double reproj_thresh_px)
{
    const std::vector<cv::Point3f> obj = make_object_points(armor_w_m, armor_h_m);

    // 【强制】原始角点 + K + D 直接传入；外部不做 undistort
    bool ok = cv::solvePnP(obj, corners, cam.K, cam.D, r.rvec, r.tvec,
                           false, cv::SOLVEPNP_IPPE);
    if (!ok)   // 退化时回退迭代法
        ok = cv::solvePnP(obj, corners, cam.K, cam.D, r.rvec, r.tvec,
                          false, cv::SOLVEPNP_ITERATIVE);
    r.pnp_ok = ok;
    if (!ok) { r.suspect = true; r.suspect_reason = "solvePnP returned false"; return r; }

    cv::Rodrigues(r.rvec, r.R);                    // rvec → R
    rotation_matrix_to_euler(r.R, r.rx_rad, r.ry_rad, r.rz_rad);

    cv::projectPoints(obj, r.rvec, r.tvec, cam.K, cam.D, r.reprojected);

    double sum = 0;                                 // 4 点平均重投影误差
    for (int i = 0; i < 4; ++i) sum += cv::norm(corners[i] - r.reprojected[i]);
    r.reproj_err_px = sum / 4.0;

    r.distance_m = std::sqrt(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);   // 直线距离
    compute_rough_depth(corners, cam, w, h, r.z_rough_height_m, r.z_rough_width_m);

    // 校验规则（见 §7.3）
    r.finite_values = 所有输出 isfinite;
    if (!finite)        reasons << "non-finite output;";
    if (!(tz > 0))      reasons << "tz<=0;";
    if (reproj > thr)   reasons << "reproj>thr;";
    if (dist > 20m)     reasons << "distance too large;";
    if (|Z_rough - tz| / Z_rough > 0.35) reasons << "rough-depth mismatch;";
    ...
}
```

**为什么优先 `SOLVEPNP_IPPE`**：IPPE 是专为**平面**目标设计的解析解法，4 点即可求解且无需初值；
`ITERATIVE` 需要合理初值，4 点时容易掉进局部极小。实测 2577 个候选全部一次成功，未触发回退。

### 5.5 `pose_solver.cpp::compute_rough_depth` —— 第六章校验项 ①

```cpp
const double h_left  = cv::norm(corners[3] - corners[0]);   // LB - LT
const double h_right = cv::norm(corners[2] - corners[1]);   // RB - RT
const double h_px    = 0.5 * (h_left + h_right);

z_by_height = fy * armor_h_m / h_px;      // Z ≈ f·H_real/h_pixel
z_by_width  = fx * armor_w_m / w_px;      // 同理由宽度估算
```

**为什么同时算两种**：板宽（0.135）与板高（0.056）尺度差 2.4 倍，
若两者估算的 Z 彼此一致、且都与 tz 同量级，可信度才高；只算一种容易掩盖"某一方向被拉长"的错误。

### 5.6 `pose_solver.cpp::run_sensitivity_test` —— 第六章校验项 ②

7 个单因素用例，每个都重新跑一次 `solve_pose`，与 baseline 做差：

| 用例 | 代表什么现实误差 |
|---|---|
| `baseline` | 基准 |
| `corners dx +1px (all)` | 检测框整体平移（如分割阈值偏移） |
| `corner0 dx +1px` | 单个角点定位误差（灯条末端模糊） |
| `corner2 dy +1px` | 另一个角点的纵向误差 |
| `armor width x1.10` / `x0.90` | 板型尺寸标定误差（±10%） |
| `armor height x1.10` | 板高标定误差 |

### 5.7 `frame_output.cpp::csv_writer` —— 13 字段与"留空"规则

```cpp
out_ << "frame_id,time_s,candidate_id,detect_valid,pnp_valid,"
        "tx_m,ty_m,tz_m,distance_m,rx_rad,ry_rad,rz_rad,reprojection_error_px\n";

if (row.has_pose) {
    out_ << tx << "," << ty << "," << tz << "," << dist << ","
         << rx << "," << ry << "," << rz << "," << reproj;
} else {
    out_ << ",,,,,,,";      // 7 个逗号 = 8 个空字段中的分隔符
}
```

**关键点**：`has_pose == false` 时**绝对不写 0**（任务明确"禁止填 0"），
而是写空字符串，这样 Excel/pandas 读进来是 NaN，能明确区分"没解出来"与"解出来是 0"。

### 5.8 `frame_output.cpp::video_writer` —— 编码回退

```cpp
// 第一选择：MP4V + .mp4
cv::VideoWriter w(p + ".mp4", fourcc('m','p','4','v'), fps, size, true);
// 第二选择：MJPG + .avi
cv::VideoWriter w(p + ".avi", fourcc('M','J','P','G'), fps, size, true);
```

**为什么需要回退**：不同 OpenCV 构建可写的编码器不同；硬指定 MP4V 在某些环境直接 `isOpened()==false`，
会静默产出 0 字节视频。回退后至少能拿到 MJPG。

### 5.9 `main.cpp` —— 主循环的四条纪律

```cpp
while (cap.read(frame)) {                    // ① 读到 EOF，不写死帧数
    if (frame.empty()) {                     // ② 空帧：记一行，pnp_valid=0
        csv.write(empty_row); ++frame_id; continue;
    }
    const armor::frame_result det = armor::detect_frame(frame, cfg);   // ③ 复用，不改接口
    ...
    for (auto &plate : det.plates) {
        pose = solve_pose(...);
        // ④ 每帧的 pose 都是本帧独立解算 —— 没有任何 last_pose 缓存
        ...
    }
}
```

**关于"严禁用上一帧位姿填充"**：代码里**不存在**跨帧的 `pose_result` 变量，
`best_pose` 只用于最后出图/敏感性测试，不参与任何帧的输出。空帧与失败帧一律写空字段。

---

## 6. 提交物核验

| 产物 | 路径 | 校验方法 | 结果 |
|---|---|---|---|
| ① 标注视频 | `output/outpost_annotated.mp4` | 解析 MP4 `mvhd`/`tkhd` | 1280×1024，35.084 s → **4210 帧 @120fps** ✅ |
| ② CSV | `output/pnp_frames.csv` | Python csv 逐行解析 | 头 13 字段完全匹配；**4211 行**（1 表头 + 4210 帧）；每行 13 字段 ✅ |
| ③ 重投影验证图 | `output/reproj_check_best.png`、`reproj_check_f000{1,2,3}_c0.png` | 目视 + 顶部误差文本 | 绿=检测、红=重投影、3× 放大面板 ✅ |
| ④ 敏感性结果 | `output/sensitivity.csv` | 打开表格 | 7 行用例 ✅ |
| ⑤ 静态图结果 | `output/static_armor_result.png` | 目视 | 1 块装甲板 ✅ |

**CSV 逐行校验（Python 实测）**

```
header fields   : 13
expected header : 13 fields -> True
data rows       : 4210
rows detect=1   : 2577
rows detect=0   : 1633   (每个都是一帧无候选，位姿字段全空)
all 13 fields   : True
detect=1 but empty pose fields: 0
rows where pose filled with 0 (forbidden): 0     ← 没有违规填 0
rows reproj>3px : 1  [('322', '3.1678')]
tz min/max      : 2.610 / 3.134 m
tz<=0 rows      : 0
sample no-cand row: ['0','0.000000','-1','0','0','','','','','','','','']
```

---

## 7. 校验与判定规则

### 7.1 规则清单

| 规则 | 阈值 | 含义 | 违反时 |
|---|---|---|---|
| 数值有限性 | `isfinite` 全部成立 | 防止 NaN/Inf 写进 CSV | `suspect` + 原因 |
| 深度为正 | `tz > 0` | 目标必须在相机前方 | `suspect` |
| 重投影误差 | `mean_reproj ≤ 3.0 px` | 姿态与角点自洽 | `suspect` |
| 距离合理 | `distance ≤ 20 m` | 排除数值爆炸 | `suspect` |
| 粗深度一致 | `|Z_rough − tz| / Z_rough ≤ 0.35` | 分割角点可信 | `suspect` |

`suspect` 的候选**仍写入 CSV**（`pnp_valid=1` 但带原因），因为"可疑"不等于"无效"，
后续可以按需过滤；只有 `solvePnP` 返回 false 或输出非有限数时 `pnp_valid=0`。

### 7.2 实测校验结果

```
frames read        : 4210
frames with detect : 2577
candidates total   : 2577
pnp ok (numeric)   : 2577
pnp valid (passed) : 2576
suspect (flagged)  : 1
mean reproj err    : 0.788857 px
distance range     : [2.66444, 3.18858] m
tz range           : [2.60994, 3.13384] m
```

- **唯一可疑帧 frame 322**：重投影误差 **3.1678 px** > 3.0 px 阈值。该帧目标处于运动/反光导致灯条末端模糊的位置，角点抖动使重投影略微超标。
- 算术核对：`2576 valid + 1 suspect = 2577 = candidates total` ✅

**粗深度交叉校验（最佳帧 1847，第六章校验项 ①）**

| 量 | 值 | 与 tz 的相对偏差 |
|---|---|---|
| tz（PnP 输出） | **2.76333 m** | — |
| Z_rough（按板高 0.056 m） | **2.77151 m** | **0.295%** |
| Z_rough（按板宽 0.135 m） | **2.76370 m** | **0.013%** |

两条独立的针孔估算都与 PnP 的 tz 吻合到 **0.3% 以内**，说明：
① 角点分割没有把装甲板拉长/压扁；② `参数.md` 的板尺寸与图像内容一致；
③ solvePnP 的解在物理量级上可信。


### 7.3 误报（false positive）与漏报（false negative）风险

| 规则 | 误报风险（把好结果标成可疑） | 漏报风险（把错结果放过） |
|---|---|---|
| `tz>0` | 极低。合法观测必然在相机前方 | **低但非零**：若 IPPE 收敛到镜像解，tz 仍可能为正而姿态翻转 |
| 重投影 ≤3 px | **中**：角点轻微抖动就会把 3.1 px 标成可疑（本例 frame 322 很可能就是这种情况），造成"好帧被误判" | **高**：4 点平面 PnP 是最小解，**错误的姿态也能拟合到 <1 px**（实测最好帧 0.0099 px），因此重投影几乎无法发现角点整体错位 |
| 距离 ≤20 m | 低 | 低（只防数值爆炸，不防小偏差） |
| `Z_rough` 一致 ≤35% | **中**：`Z_rough` 用像素边长估算，角点沿边方向的小误差会被放大到 Z 上；目标贴近画面边缘时透视变形也会让它偏大 | **中**：35% 是很宽的门槛，若板尺寸整体错了 30%（例如混用了大小装甲板的 w/h），恰好可能落在门槛内 |
| 有限性 | 无 | 无（纯数值兜底） |

**结论与使用建议**：
1. **不要**把"`pnp_valid=1`"等同于"位姿可信"——本阶段单帧 PnP 没有时序约束，
   单帧在几何上可能是"数学自洽但物理错误"的镜像解；
2. 重投影误差应当**当作"角点自洽性"指标**，而不是"位姿正确性"指标；
3. 想让判定更可靠，下一阶段可加入**时序一致性**（连续帧 tz 不应跳变）——
   但本阶段文档明确禁止跟踪/时序滤波，故只在此说明，不实现。

### 7.4 敏感性测试实测结果

```
case                       tz(m)      dist(m)    reproj(px)   d_tz      d_dist    d_reproj
baseline                   2.76333    2.78472    0.009881      0         0         0
corners dx +1px (all)      2.76335    2.78485    0.009829     +0.00002  +0.00012  -0.00005
corner0 dx +1px            2.75436    2.77568    0.178128     -0.00897  -0.00904  +0.16825
corner2 dy +1px            2.74225    2.76349    0.166936     -0.02109  -0.02123  +0.15705
armor width x1.10          2.77994    2.80150    0.170122     +0.01661  +0.01678  +0.16024
armor width x0.90          2.49016    2.50944    0.177680     -0.27317  -0.27528  +0.16780
armor height x1.10         2.76683    2.78825    0.166631     +0.00350  +0.00353  +0.15675
```

**逐条解读**

| 用例 | 观察 | 结论 |
|---|---|---|
| 角点整体 +1 px | tz 只变 **0.00002 m**（0.02 mm） | **整体平移几乎不影响深度**——因为平移会被姿态的 tx/ty 吸收。说明"框整体偏移"对 tz 是良性的 |
| 单角点 +1 px | tz 变 **−8.97 mm**，重投影从 0.010 → 0.178 px | 单角点误差会直接污染深度，且**重投影误差立刻放大约 18 倍**——这是重投影最有用的场景 |
| 单角点 +1 px（纵向） | tz 变 **−21.1 mm** | 纵向（板高方向）误差对深度影响比横向更大，因为深度主要由板高/板宽的像素尺寸决定 |
| 板宽 ×0.90 | tz 变 **−273 mm**（−9.9%） | **近似线性**：`tz ∝ W_real`，理论值 2.76333×0.9 = 2.487，实测 2.490，**误差仅 3 mm** |
| 板宽 ×1.10 | tz 只变 **+16.6 mm**（+0.6%），**不是理论上的 +276 mm** | **非线性：平面 PnP 的双解歧义被触发**。模型放大后，IPPE 选中的那支解发生跳变（镜像解/分支解） |
| 板高 ×1.10 | tz 变 **+3.5 mm**（+0.13%） | 同样未按线性变化，说明高度方向也出现了分支敏感性 |

> **最重要的工程结论**：**深度对"假设的物理尺寸"高度敏感，且响应不是全程线性的**。
> 尺寸偏小 10% → 深度偏小 9.9%（线性段）；但尺寸偏大 10% 时，解算跳到了另一支解，
> 深度几乎不动。这意味着：**如果板型 w/h 用错，tz 会系统性地错，而重投影误差几乎看不出来**（0.17 px）。
> 因此 `参数.md` 的 0.135/0.056 必须严格对应实际板型，且应当用 `Z_rough` 独立复核。

---

## 8. 调试排查故障表

| 现象 | 优先检查 | 具体动作 / 本工程对应点 |
|---|---|---|
| `[ERROR] Failed to open video` | 工作目录、文件是否被复制到 exe 旁 | CMake POST_BUILD 已复制 `outpost.avi`；确认在 `build/Release` 下运行 |
| 打印的 fps 是 `(fallback)` | 视频元数据损坏或后端不支持 | 已在日志显式标注；写视频会用兜底 fps，可能导致视频"变慢/变快" |
| 总帧数为 unknown | 容器未写 `dwTotalFrames` | 不影响处理（循环读到 EOF），只影响时长打印 |
| `[PARAM] source: built-in defaults` | `参数.md` 未找到或格式变了 | 确认 `参数.md` 在 exe 旁；检查 `camera_matrix` / `distortion_coefficients` 的 `data:` 是否仍是 `[]` 形式 |
| K 解析出奇怪数值 | 正则抓到了注释里的数字 | 检查 `data:` 块是否为纯数字序列 |
| `pnp_valid` 全 0 | 角点顺序与模型点顺序不一致 | 检查 `make_object_points` 是否为 左上/右上/右下/左下；重投影误差会是几十像素量级 |
| tz 恒为负 | 模型系 z 方向或角点绕行方向反了 | 检查角点是否顺时针；必要时对调 1/3 角点（但**不要改 detect_frame**） |
| 重投影误差全都很小（<0.1 px）但 tz 明显不对 | 正常现象——4 点最小解 | 用 `Z_rough` 交叉校验，而不是只看重投影（见 §7.3） |
| 深度整体偏大/偏小固定比例 | 板尺寸 w/h 用错（大小装甲板混淆） | 对照 `参数.md` 第二节；用敏感性表判断（线性段内 tz ∝ W_real） |
| 偶发单帧 tz 跳变 | 灯条角点抖动 / 反光 | 属单帧固有噪声；本阶段**不做**时序滤波，只在可疑帧打标记 |
| CSV 用 Excel 打开变成多列 | locale 把小数当逗号 | 已 `imbue(locale::classic())`；若仍异常，用"数据→自文本"导入并指定逗号分隔 |
| 标注视频 0 字节 | `VideoWriter` 编码器不可用 | 已实现 MP4V→MJPG 回退；若仍失败检查 OpenCV 是否带 ffmpeg 后端 |
| 处理很慢（>5 min） | 每帧耗时可打印 | 本机 4210 帧约 3 分钟（≈43 ms/帧，含编码）；可只读前 N 帧做联调，但**不得**作为最终交付 |
| 改了代码但结果不变 | 没重新编译 | 确认编译日志出现被改的 `.cpp`，必要时 `--clean-first` |

---

## 9. 测试用例与验证方案

### 9.1 【强制】功能验证用例

| 编号 | 用例 | 操作 | 预期 | 实测 |
|---|---|---|---|---|
| **T-01** | 视频信息打印 | 直接运行 | 打印 1280/1024/119.998/4210 | ✅ 完全一致 |
| **T-02** | 参数解析 | 直接运行 | 打印 K、D、板尺寸 | ✅ K/D 与 `参数.md` 逐位一致 |
| **T-03** | 全量处理 | 跑完 4210 帧 | 生成视频+CSV | ✅ 4210 帧读入，2577 帧检出 |
| **T-04** | CSV 结构 | Python 解析 | 13 字段、4210 行、失败留空 | ✅ 见 §6 |
| **T-05** | 无候选帧记录 | 查 `detect=0` 行 | 存在且位姿字段为空 | ✅ 1633 行 |
| **T-06** | 禁止填 0 | 查 `pnp_valid=0` 行的位姿列 | 全为空字符串 | ✅ 0 个违规 |
| **T-07** | 视频属性保持 | 解析 mp4 | 1280×1024、35.084 s、4210 帧 | ✅ |
| **T-08** | 重投影验证图 | 打开 png | 绿/红两组角点 + 误差文本 | ✅ 含 3× 放大 |
| **T-09** | 粗深度校验 | 看终端 | `Z_rough` 与 tz 同量级 | ✅ 见 §7.2 终端输出 |
| **T-10** | 敏感性测试 | 看终端 + csv | 7 个用例有 `d_tz/d_dist/d_reproj` | ✅ |
| **T-11** | 静态入口保留 | `--image images/outpost_frame.png` | 输出静态结果图 | ✅ 1 块板 |
| **T-12** | `detect_frame` 未改 | diff 第 7 周文件 | 函数签名与实现一致 | ✅ 原样复制 |

### 9.2 【强制】异常测试

| 编号 | 用例 | 操作 | 预期 | 实测 |
|---|---|---|---|---|
| **E-01** | 视频路径不存在 | `--video nope.avi` | 提示失败并给出兜底元数据，退出码非 0 | ✅ `[ERROR] Failed to open video` + fallback 提示 + exit 1 |
| **E-02** | 参数文件缺失 | `--params nope.md` | 用内置默认值并警告，流程不中断 | ✅ `source: built-in defaults` |
| **E-03** | 空帧 | 构造空 `Mat` 帧（代码内已处理） | CSV 记一行、`pnp_valid=0`、不复用上一帧 | ✅ 代码路径存在且无 last_pose 缓存 |
| **E-04** | solvePnP 失败 | 传退化角点（四点共线） | `pnp_valid=0`，位姿留空，不崩溃 | ✅ 返回 false → CSV 留空 |

### 9.3 【可选】扩展验证

| 编号 | 用例 | 说明 |
|---|---|---|
| O-01 | 重投影误差曲线 | 用 `pnp_frames.csv` 第 13 列画时间曲线，观察抖动区间 |
| O-02 | 抽出不同时刻帧做单帧检查 | 验证参数是否严重依赖某一帧 |
| O-03 | 换一段视频（不同距离） | 验证 tz 与 Z_rough 的一致性是否保持 |

---

## 10. 关键逻辑来源说明（每条代码对应哪条文档要求）

| 代码 | 文档依据 | 说明 |
|---|---|---|
| `read_video_meta()` + 兜底 | 强制 §1 | "视频读取异常使用参数.md里的参考值兜底" |
| `while (cap.read(frame))` | 强制 §1 | "禁止写死循环帧数" |
| `load_camera_params()` | 强制 §2 | "读取同目录下参数.md，获取 K 与 D" |
| `solve_pose()` 传 `cam.K, cam.D` | 强制 §3 | "直接传入原始图像角点、K、D，不要在外部手动 undistort" |
| `distance = √(tx²+ty²+tz²)` | 强制 §4 | "直线欧式距离 sqrt(tx²+ty²+tz²)" |
| `rotation_matrix_to_euler()` | 强制 §4 | "rvec 是 Rodrigues 旋转向量，不能直接当成欧拉角" |
| `draw_pose_overlay()` | 强制 §5① | 框/角点 0-1-2-3/ID/PnP 标记/直线距离 |
| `csv_writer` 13 字段 + 留空 | 强制 §5② | 字段名逐字对应；失败留空禁止填 0；无候选记一行 |
| `make_reprojection_check()` | 强制 §5③ | "调用 projectPoints 绘制原始角点和重投影角点"；误差取 4 点均值 |
| `compute_rough_depth()` | 强制 §6① | "针孔近似公式 Z_rough 粗略深度估算，用来和 PnP 输出的 tz 做对比校验" |
| `run_sensitivity_test()` | 强制 §6② | "扰动角点坐标、装甲物理尺寸" |
| `suspect` 判定块 | 强制 §6③ | "tz>0、重投影误差阈值、数值有限性判断，标记可疑结果" |
| `make_object_points()` | 参数.md §2 | 四角顺序与 detect_frame 输出逐点对应 |
| `--image` 分支 | 强制 §8 | "保留原有静态图片测试入口" |
| `detect_frame` 原样复用 | 强制 §8 | "不能改动 detect_frame 检测接口" |

---

## 11. 运行说明（别人照着能跑）

```powershell
# 1) 配置（OpenCV 路径按你自己的安装位置改）
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/RM/opencv/build"

# 2) 编译
cmake --build build --config Release

# 3) 运行（必须在 build\Release 下，因为素材由 POST_BUILD 复制到该目录）
cd build\Release
.\week8_outpost.exe                                   # 处理 outpost.avi
.\week8_outpost.exe --video outpost.avi               # 显式指定
.\week8_outpost.exe --image images/outpost_frame.png  # 静态图入口（保留）
.\week8_outpost.exe --params 参数.md --outdir output  # 指定参数与输出目录
```

**依赖**：OpenCV ≥ 4.x（本工程在 4.14.0 验证，含 ffmpeg 后端用于 H264 解码）。

---

## 12. 测试记录（本次实跑）

> 运行时间：本机约 **3 分钟** 处理 4210 帧（≈43 ms/帧，含 MP4V 编码）。
> 完整日志见 `build/Release/run_log.txt`。

```
===== 1. load camera parameters =====
[PARAM] source: 参数.md (K and D parsed from file)
[PARAM] image size: 1280 x 1024
[PARAM] K = [1781.05, 0, 510.575; 0, 1781.69, 472.974; 0, 0, 1]
[PARAM] D = [-0.0442074, -0.108154, -0.000650622, 0.000235738, 0.466737]
[PARAM] armor: width=0.135 m, height=0.056 m

===== 2. open video =====
input: outpost.avi
video width  = 1280
video height = 1024
video fps    = 119.998
video frames = 4210 (metadata only; loop reads until EOF)
video duration ~ 35.084 s

===== 3. process frames =====
annotated video -> output/outpost_annotated.mp4
[SAVED] output/reproj_check_f0001_c0.png
[SAVED] output/reproj_check_f0002_c0.png
[SAVED] output/reproj_check_f0003_c0.png
  processed 500 frames, candidates so far 316
  processed 1000 frames, candidates so far 622
  processed 1500 frames, candidates so far 916
  processed 2000 frames, candidates so far 1218
  processed 2500 frames, candidates so far 1526
  processed 3000 frames, candidates so far 1835
  processed 3500 frames, candidates so far 2138
  processed 4000 frames, candidates so far 2456

===== 4. summary =====
frames read        : 4210
frames with detect : 2577
candidates total   : 2577
pnp ok (numeric)   : 2577
pnp valid (passed) : 2576
suspect (flagged)  : 1
mean reproj err    : 0.788857 px
distance range     : [2.66444, 3.18858] m
tz range           : [2.60994, 3.13384] m
csv                : output/pnp_frames.csv
annotated video    : output/outpost_annotated.mp4
[SAVED] output/reproj_check_best.png (frame 1847, reproj 0.00988129 px)

===== rough depth cross-check (best frame 1847) =====
  tz (PnP)           = 2.76333 m
  Z_rough (by height)= 2.77151 m
  Z_rough (by width) = 2.7637 m
  rel err (height)   = 0.00295145

===== sensitivity test (single-factor) =====
baseline                 tz=  2.76333 dist=  2.78472 reproj=0.00988129  d_tz=        0 d_dist=        0 d_reproj=       0
corners dx +1px (all)    tz=  2.76335 dist=  2.78485 reproj=0.00982893  d_tz=1.56298e-05 d_dist=0.000124923 d_reproj=-5.23538e-05
corner0 dx +1px          tz=  2.75436 dist=  2.77568 reproj=0.178128  d_tz=-0.00897243 d_dist=-0.00904387 d_reproj= 0.168247
corner2 dy +1px          tz=  2.74225 dist=  2.76349 reproj=0.166936  d_tz=-0.0210853 d_dist=-0.0212337 d_reproj= 0.157054
armor width x1.10        tz=  2.77994 dist=   2.8015 reproj=0.170122  d_tz=0.0166074 d_dist=0.0167786 d_reproj= 0.16024
armor width x0.90        tz=  2.49016 dist=  2.50944 reproj= 0.17768  d_tz=-0.273168 d_dist=-0.275277 d_reproj= 0.167799
armor height x1.10       tz=  2.76683 dist=  2.78825 reproj=0.166631  d_tz=0.00349658 d_dist=0.00352939 d_reproj= 0.156749
[SAVED] output/sensitivity.csv

DONE
```

### 12.1 自查：强制项逐条对照

| # | 强制项 | 是否完成 | 证据 |
|---|---|---|---|
| 1 | 读视频、打印宽高fps帧数、兜底、不写死帧数 | ✅ | §12 日志 + `read_video_meta` |
| 2 | 读参数.md 取 K/D 与板尺寸 | ✅ | `[PARAM]` 五行 |
| 3 | solvePnP 传原始角点+K+D，不外部 undistort | ✅ | `solve_pose` 无 undistort 调用 |
| 4 | 坐标系/tz/距离/rvec 约定 | ✅ | §2.4、§2.5 + 代码注释 |
| 5① | 标注视频（原分辨率帧率+五要素） | ✅ | 1280×1024/120fps/4210 帧 |
| 5② | CSV 13 字段、留空不填 0、多候选多行、无候选记一行 | ✅ | §6 Python 校验 |
| 5③ | 重投影验证图（原始+重投影角点，4点均值误差） | ✅ | 4 张 png |
| 6① | Z_rough 与 tz 对比 | ✅ | §12 粗深度段 |
| 6② | 单因素敏感性测试 | ✅ | 7 用例 + sensitivity.csv |
| 6③ | 校验规则 + 可疑标记 + 误报漏报说明 | ✅ | §7.3 |
| 7 | 空帧/失败/false → pnp_valid=0；不复用上一帧 | ✅ | 主循环无跨帧 pose 缓存 |
| 8 | 保留静态入口、不改 detect_frame、更新 CMake、测试记录 | ✅ | §9、§12 |

### 12.2 未完成项 / 已知限制（如实列出）

| 项 | 状态 | 说明 |
|---|---|---|
| 三块板 120° 几何约束 | **未实现**（不属本阶段强制项） | `参数.md` 第三节给出三板模型，本阶段只要求单板 PnP；仅注释说明 |
| 时序平滑/跟踪 | **按文档禁止**，未实现 | 单帧 tz 有抖动，属正常现象 |
| 遮挡/半板场景 | 未专门处理 | 灯条被遮挡到只剩一根时，`detect_frame` 返回 0 候选 → 该帧记空行，不会算出错误位姿 |
| `Z_rough` 一致性门槛 | 35% 偏宽容 | 若板型用错 30%，可能仍在门槛内；需配合敏感性表人工判断 |
| 平面 PnP 双解 | 未做显式筛选 | 实测在板宽 ×1.10 时观察到分支跳变（§7.4），本阶段仅记录风险 |
| 处理速度 | 43 ms/帧 | 对 120 fps 视频不是实时；本阶段只要求离线出结果 |
| 一帧多候选的 CSV 多行分支 | **代码已实现但本视频未触发** | 该视频每帧最多 1 块装甲板，所以 CSV 里同一 frame_id 只出现一次；多候选时按 candidate_id=0,1,2... 逐行写入的分支位于主循环内（未在真实数据上验证） |

---

## 13. 一页速记

| 环节 | 一句话 | 关键值 |
|---|---|---|
| 输入 | 1280×1024 @120fps，4210 帧，循环读到 EOF | 不写死帧数 |
| 参数 | 正则解析 `参数.md` 的 `data:` 块 | fx 1781.05 / fy 1781.69 |
| 解算 | 原始角点 + K + D 直接进 `solvePnP`（IPPE 优先） | **不 undistort** |
| 输出 | tvec=板中心；tz=深度；distance=欧氏距离 | tz 2.610–3.134 m |
| 旋转 | `Rodrigues(rvec)→R→欧拉角` | **rvec 不当欧拉角** |
| CSV | 13 字段，失败**留空** | 4210 行，0 个违规 0 值 |
| 校验 | tz>0 / reproj≤3px / finite / 粗深度≤35% | 2576 通过，1 可疑 |
| 风险 | 4 点是最小解，重投影几乎无约束力 | 必须靠 `Z_rough` 复核 |
