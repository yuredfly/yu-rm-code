# OpenCV 第 6 周复习笔记：预处理 · 阈值 · 形态学 · 轮廓目标检测

> 对应任务：`10.12-10.18 任务.pdf`（硬性参数与验收）+ `10.12-10.18 引导.pdf`（概念与排查）
> 方案一指定图：`images/input.png`（960×640，3 通道）
> 本笔记中标注 **[实测]** 的数字，是我对 `images/input.png` 实际测量得到的，不是估算。

---

## 0. 一页速览（先看这张）

| 项 | 内容 |
|---|---|
| **本周目标** | 读图 → 滤波 → 分割出 **mask** → **形态学**修整 → **轮廓**提取与面积过滤 → 外接框/中心点测量与标注 |
| **验收主线** | 程序要能说明"**目标是什么、从哪里分出来、检测到了几个、每个目标在哪里**" |
| **不做** | 真实物体识别、视频、跟踪、机器学习 |
| **方案** | **二选一**（方案一：指定图固定参数；方案二：自选图自调参数） |
| **核武器级约束** | ① 四种形态学**必须四次独立以同一张 mask 为输入**，禁止链式；② 轮廓检测**只能用开运算结果**；③ `moments` 除法**前必须判 `m00 != 0`**；④ 全程**项目内相对路径** |
| **不做就扣分** | 9 个输出文件、终端输出面积+中心点+总数、4 项测试、测试后恢复原参数重跑 |

### 参数总表（方案一，**全部任务给定，不可自创**）

| 环节 | 函数 | 参数 | 输出文件 |
|---|---|---|---|
| 灰度滤波 | `cv::GaussianBlur` | `Size(5,5)`, sigma=0 | `fixed_blur_gray.png` |
| 灰度分割 | `cv::threshold` | thresh=**120**, maxval=255, `THRESH_BINARY_INV` | `fixed_mask_gray.png` |
| 颜色滤波 | `cv::medianBlur` | ksize=**5** | `fixed_blur_color.png` |
| 颜色分割 | `cv::inRange` | 下界 `(35,80,80)` 上界 `(85,255,255)` | `fixed_mask_hsv.png` |
| 腐蚀 | `cv::erode` | kernel **3×3** | `fixed_eroded.png` |
| 膨胀 | `cv::dilate` | kernel **3×3** | `fixed_dilated.png` |
| 开运算 | `cv::morphologyEx` + `MORPH_OPEN` | kernel **3×3** | `fixed_opened.png` |
| 闭运算 | `cv::morphologyEx` + `MORPH_CLOSE` | kernel **7×7** | `fixed_closed.png` |
| 轮廓 | `cv::findContours` | `RETR_EXTERNAL` + `CHAIN_APPROX_SIMPLE` | 终端候选面积 |
| 过滤 | `cv::contourArea` | 面积 **< 100 丢弃** | — |
| 测量 | `boundingRect` / `minAreaRect` / `moments` | 两框不同颜色；**先判 m00** | 终端面积+中心点+总数 |
| 标注 | `rectangle` / `circle` / `imwrite` | 画在 **`inputImage.clone()`** 上 | `output/fixed_result.png` |

---

## 1. 概念底：mask 与坐标

| 概念 | 要点 | 为什么重要 |
|---|---|---|
| **mask（掩膜/二值图）** | **单通道**，只有 0/255；**白=前景（目标）**，**黑=背景** | 后续形态学与轮廓**全部**依赖它；分错则后面全错 |
| **BGR 通道顺序** | OpenCV 读彩色图默认 **BGR**，不是 RGB | 灰度权重的 R/B 系数、`cv::Scalar(B,G,R)` 全受影响 |
| **坐标系** | 原点**左上角**，**x 向右、y 向下**；点是 `(x,y)` | 调试图上位置时不要写反 |
| **Mat 是浅拷贝** | `Mat b = a;` 共享像素；要独立用 `.clone()` / `copyTo()` | `findContours` 前、绘图前**必须 clone** |
| **rows/cols** | `rows`=高=H，`cols`=宽=W；但 `at<T>(y,x)` 是先行后列 | 打印尺寸与访问像素顺序**相反** |
| **Size(w,h) / Rect(x,y,w,h)** | Size 是 (宽,高)；Rect 是 (左上x, 左上y, 宽, 高) | 参数顺序错会导致奇怪结果 |

---

## 2. 读图与检查

```cpp
cv::Mat image = cv::imread("images/input.png", cv::IMREAD_COLOR);
if (image.empty()) { std::cerr << "Failed to read image. Check the path and file name.\n"; return 1; }
std::cout << "width=" << image.cols << " height=" << image.rows
          << " channels=" << image.channels() << '\n';
```

| 知识点 | 说明 |
|---|---|
| `imread` 返回值 | **失败不抛异常**，只返回空 Mat → **必须 `empty()` 检查** |
| 相对路径基准 | **从程序运行时的"当前工作目录"开始算**，不是从源码文件位置 |
| 读图失败的 4 个常见原因 | 路径不对 / 文件不存在 / 扩展名错 / 运行目录不是项目目录 |
| `IMREAD_COLOR` | 强制 3 通道 BGR（丢 alpha）；灰度用 `IMREAD_GRAYSCALE` |
| `imwrite` | **返回 bool，必须检查**；**不会自动建目录**，`output/` 要提前手工创建 |

**易错点**：读图失败却继续往下跑 → 空图让阈值、轮廓、绘图**连锁出错**，最终报错位置离真正原因很远。**先在第一步就把读图打出来验证**。

---

## 3. 滤波（分割前必做）

| 滤波器 | 核参数类型 | 机制 | 适用 | 本周位置 |
|---|---|---|---|---|
| **`GaussianBlur`** | `Size(w,h)` 正奇数 | 按距离加权平均，邻近像素平滑混合 | 一般平滑 | 灰度路线，`Size(5,5)` |
| **`medianBlur`** | `int` 奇数（>1） | 取邻域**中间值** | **孤立小黑点/小白点**（椒盐噪声） | HSV 路线，`5` |

**规则**：核**通常取正奇数**；**核越大平滑越明显，但边缘也可能被模糊**。
**高频坑**：滤波结果写在**第二个参数**里，**后续必须用滤波后的图**，不要又把原图拿去分割。
**两个滤波器为什么不同**：任务规定，不要互换；`medianBlur` 对孤立杂点更强，`GaussianBlur` 更"柔和"。

---

## 4. 两条分割路线

### 4.1 灰度固定阈值

| 类型 | 规则 | 什么情况用 |
|---|---|---|
| `THRESH_BINARY` | `T > thresh` → 白 | **目标比背景亮** |
| **`THRESH_BINARY_INV`** | `T <= thresh` → 白 | **目标比背景暗** ← 本周 |

**[实测] 本周三个灰度值**（BGR2GRAY 权重 `0.299R+0.587G+0.114B`）：

| 区域 | RGB | 灰度 | `BINARY_INV(120)` 结果 |
|---|---|---|---|
| 浅色背景 | (248,248,248) | **248** | 黑 ✅ |
| **深绿主体** | (40,120,40) | **87** | **白** ✅ |
| 黄色干扰 | (248,216,24) | **204** | 黑 ✅ |

→ **阈值 120 + `INV` 方向正确**：主体比背景暗，所以必须 `INV`。

### 4.2 HSV 颜色筛选

| 通道 | OpenCV 范围 | 本周实测值 |
|---|---|---|
| **H 色相** | **0–179**（不是 0–360，OpenCV **除以 2**） | 绿 **59**，黄 **25** |
| **S 饱和度** | 0–255 | 绿 **170** |
| **V 明度** | 0–255 | 绿 **120** |

任务区间 `(35,80,80) ~ (85,255,255)` → **绿 H=59 在里面且余量充足；黄 H=25 在 85 之外 → 天然被排除**。
`inRange` 语义：**三个通道都落在闭区间 `[lower, upper]` 内** → 白，否则黑；**输出天然是二值图，不用再 `threshold`**。

**调参纪律**：结果不理想时**每次只调一个上下界**，再重新看 mask，才知道是哪项起作用。

### 4.3 两张 mask 对比（**提交时必须写**）

| 对比项 | 灰度阈值路线 mask | HSV 路线 mask |
|---|---|---|
| 6 个绿色主体 | 完整保留（绿 87 vs 背景 248，差 160） | 完整保留（H=59 稳定） |
| 3 个黄色干扰 | 排除（黄 204 > 120） | 排除（黄 H=25 < 35） |
| 小绿点 | 保留（需开运算+面积过滤） | 保留（同样处理） |
| 主体内部小孔 | 黑（孔内浅色 → 248 > 120） | 黑（浅色 → S 低，不在区间内） |
| **本质差异** | 靠**明暗差**：若干扰与目标**亮度相近但颜色不同**，会被误选 | 靠**颜色**：只要色相不同就能分开，**对亮度变化更"讲道理"**但需调 S/V |
| **一句话结论** | 两者都能得到 6 个主体；**HSV 对"颜色不同"的干扰排除更干净、更可控** | — |

---

## 5. 形态学（本周重点）

### 5.1 结构元素 kernel

```cpp
cv::Mat kernel3 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3,3));
cv::Mat kernel7 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7,7));
```

| 项 | 规则 |
|---|---|
| 形状 | `MORPH_RECT`（本周）/ `MORPH_ELLIPSE` / `MORPH_CROSS` |
| 大小 | **正奇数**；`1×1` 等于什么都没做；偶数核没有真正中心 |
| 锚点 | 默认 `(-1,-1)` = 核中心 |

### 5.2 四种操作

| 操作 | 内核机制 | 效果 | 本周核 | **[实测] 本图预期** |
|---|---|---|---|---|
| **腐蚀 erode** | 核内**全 1 才保留**（局部最小） | 白色区域**缩小**，小白点可能消失，边缘变细 | 3×3 | 主体各缩一圈；**6 个小点（21/37 px）全部消失** |
| **膨胀 dilate** | 核内**有 1 就变 1**（局部最大） | 白色区域**扩大**，断开可能连接，相近目标可能粘连 | 3×3 | 主体各胖一圈；小点变粗 |
| **开运算 OPEN** | **先腐后膨** | 去掉**小白噪点**，尽量保留较大主体 | 3×3 | **小绿点消失，6 主体保留** ← 用它做轮廓 |
| **闭运算 CLOSE** | **先膨后腐** | 填补**小黑孔**，连接很近的白色区域 | **7×7** | **主体内部浅色小孔被填补** |

**记忆锚**：**开 = 先腐蚀（去掉小的"白"）；闭 = 先膨胀（填上小的"黑"）。**

### 5.3 ★ 一号硬性约束：四种操作独立输入，禁止链式

> 任务原文："下面四种操作都以 `fixed_mask_hsv` 为输入，并分别运行、分别保存，**不能把它们串成一条连续处理链**。"
> 引导原文："每一种都应从同一张原始 mask 单独开始……**否则看到的变化会叠加，难以判断是哪种操作造成的**。"

**正确**（四个调用的**第一个实参是同一个 `mask_hsv`**）：

```
erode          (mask_hsv, fixed_eroded,  kernel3)
dilate         (mask_hsv, fixed_dilated, kernel3)
morphologyEx   (mask_hsv, fixed_opened,  MORPH_OPEN,  kernel3)
morphologyEx   (mask_hsv, fixed_closed,  MORPH_CLOSE, kernel7)
```

| 错误链写法 | 实际发生了什么 | 可见后果 |
|---|---|---|
| `erode → dilate` 串 | 这**就是开运算** | `fixed_eroded.png` 其实是 opened，**与 opened 图几乎一样**，无法对比 |
| 从 `opened` 再做 close | 在已去噪点的图上做闭 | 原始小孔的填补效果**观察不到** |
| 全部串成一条 | 只有最后一张是"新的" | **中间四张图全部失真** |

**自检口诀**：**四次调用的第一个实参，变量名必须完全相同。** `fixed_opened` 只在 `findContours` 时被消费。

---

## 6. 轮廓提取

| 参数 | 取值 | 理由 |
|---|---|---|
| 输入 | **`fixed_opened.clone()`** | ① 用开运算结果去掉小绿点；② `clone` 保留原 mask 供保存；③ 某些版本 `findContours` 会改输入图 |
| `mode` | **`RETR_EXTERNAL`** | 只取**最外层**轮廓，每个相连白色区域算一个候选目标 |
| `method` | **`CHAIN_APPROX_SIMPLE`** | **压缩连续冗余点**，点数大减，够用于面积/框/矩 |

| 其他 mode | 含义 | 本周 |
|---|---|---|
| `RETR_LIST` | 全部轮廓不分层 | 不用 |
| `RETR_CCOMP` | 外边界 + 孔洞两层 | 不用 |
| `RETR_TREE` | 完整树形层级 | 不用（想统计孔洞才需要） |

**重要推论**：`RETR_EXTERNAL` **不会把主体内部的小孔算成独立轮廓**，所以**小孔不会让轮廓数从 6 变成 12**。

### 轮廓"漏斗"模型

```
mask 上所有白色相连区域        ← [实测] 12 个（6 主体 + 6 小点）
   └─ RETR_EXTERNAL 后           = 12 条候选轮廓
        └─ contourArea >= 100    = 6 条  ✅ 目标数 6
```

---

## 7. 面积过滤

| 项 | 说明 |
|---|---|
| 单位 | **像素平方** |
| 本周阈值 | **小于 100 丢弃**（任务给定） |
| 引导要求 | **先输出候选面积**，观察目标与噪声差多少，**再**定阈值 → 所以要打印 |

**[实测] 候选轮廓面积（HSV 路线）**：

| # | 形状 | 面积(px²) | 外接框 w×h | 框面积 | **填充率** |
|---|---|---|---|---|---|
| 1 | 上排大矩形 | **20054** | 181×111 | 20091 | **1.00** |
| 2 | 菱形（旋转方块） | **12151** | 161×151 | 24311 | **0.50** |
| 3 | 左上圆形 | **11460** | 121×121 | 14641 | 0.78 |
| 4 | 倾斜方块 | **10711** | 141×131 | 18471 | 0.58 |
| 5 | L 形 | **10571** | 121×126 | 15246 | 0.69 |
| 6 | 右侧小圆 | **5145** | 81×81 | 6561 | 0.78 |
| 7–12 | **6 个小绿点** | **21, 21, 21, 21, 21, 37, 37** | 5×5 / 7×7 | — | — |

**结论**：6 个主体最小 5145 px²，与小绿点最大 37 px² **相差约 139 倍** → **阈值 100 分得非常干净**（既能清掉噪点，又不会误删目标）。

**★ 关键认知**：**填充率是识别形状的钥匙** —— 菱形 ≈ 0.50、圆 ≈ π/4 ≈ 0.785、轴对齐矩形 ≈ 1.00。**同一张图上 fill ratio 变了，说明形状/朝向变了**。
**坑**：不要用"外接框面积"代替 `contourArea` 做过滤 —— 菱形框面积 24311 是真实面积 12151 的**两倍**。

---

## 8. 外接框与中心点

### 8.1 两种框

| 函数 | 返回 | 特点 | 本周颜色 |
|---|---|---|---|
| `cv::boundingRect` | `cv::Rect(x,y,w,h)` | **轴对齐**，对倾斜目标明显**偏大** | 绿 `Scalar(0,255,0)` |
| `cv::minAreaRect` | `cv::RotatedRect(center,size,angle)` | **最小旋转矩形**，**紧贴**倾斜目标 | 红/其他 |

**画旋转框**：`rotated.points(corners)` 取 4 角点，**依次连线**（`for i: line(corners[i], corners[(i+1)%4])`）。

**[实测] 两框差异（用填充率预测）**：

| 形状 | 两框关系 |
|---|---|
| 轴对齐矩形 (#1) | 几乎完全相同（fill=1.00） |
| **正菱形 (#2)** | **差别最大**：轴对齐框 161×151，旋转框会缩到约 110×110 → **这就是任务说"倾斜菱形帮助观察差别"的意思** |
| 倾斜方块 (#4) | 明显差别（fill=0.58） |
| 圆 (#3/#6) | 几乎相同（fill=0.78=π/4） |

### 8.2 中心点与 `moments`

```cpp
cv::Moments m = cv::moments(contour);
if (m.m00 != 0.0) {                                  // ★ 硬性要求：先判空
    const int cx = static_cast<int>(m.m10 / m.m00);
    const int cy = static_cast<int>(m.m01 / m.m00);
    cv::circle(result, cv::Point(cx, cy), 4, cv::Scalar(0,0,255), -1);
}
```

| 项 | 说明 |
|---|---|
| `m00` | **就是轮廓面积**（零阶矩），与 `contourArea` 一致 |
| 质心公式 | **`cx = m10/m00`，`cy = m01/m00`** |
| **为什么必须判 `m00!=0`** | 退化轮廓（点/线）`m00=0` → **除零** → `inf/nan` → 坐标崩坏、画图异常 |
| `RotatedRect::angle` 版本坑 | OpenCV 4.5+ 改为 `[0,90]`（旧版 `[-90,0)`）；本周**不需要用 angle**，要用先确认版本 |

---

## 9. 绘制与保存

| 函数 | 关键参数 | 坑 |
|---|---|---|
| `cv::rectangle` | `img` 是 `InputOutputArray`（**原地改**） | **必须画在 `inputImage.clone()` 上**，否则原图被毁 |
| `cv::circle` | `thickness=-1` = **实心** | 坐标越界不崩但画不出来 |
| `cv::line` / `polylines` | 画旋转框四边 | — |
| `cv::imwrite` | 返回 bool | **必须检查返回值**；**`output/` 要提前建** |

**★ 颜色坑**：`cv::Scalar` 在 BGR 图上是 **`Scalar(B, G, R)`**。

| 想要 | 正确写法 | 错写会变 |
|---|---|---|
| 绿框 | `Scalar(0,255,0)` | `Scalar(255,0,0)` → **蓝框** |
| 红点 | `Scalar(0,0,255)` | `Scalar(255,0,0)` → **蓝点** |

---

## 10. ★ 易错点 / 坑点总表（按严重度排序）

| # | 坑 | 后果 | 正确做法 |
|---|---|---|---|
| **1** | 四种形态学**链式**调用 | 输出互相污染，**四张图无法对比**，直接违反任务 | **四次都从 `mask_hsv` 出发**，第一个实参同名 |
| **2** | 用 `fixed_mask_hsv` 而不用 `fixed_opened` 做轮廓 | 小绿点被算成目标 → **目标数 12 而不是 6** | 轮廓输入**必须**是 `fixed_opened` |
| **3** | `moments` 除法前不判 `m00` | 除零 → `inf/nan` → 画图崩坏 | **`if (m.m00 != 0.0)` 包住除法** |
| **4** | 阈值方向反了（用 `BINARY` 而非 `INV`） | mask 几乎全白/全黑，**轮廓数 0** | 目标比背景暗 → **`INV`** |
| **5** | 阈值/`inRange` 用的是**原图**而不是**滤波后的图** | 噪声全部进入 mask | 传 `blur_gray` / `blur_color` |
| **6** | `findContours` 未传 `clone()` | 原 mask 可能被改写，保存的中间图自相矛盾 | `fixed_opened.clone()` |
| **7** | 绘图未用 `clone()` | **原图被画花**，最终图与中间图不一致 | `inputImage.clone()` |
| **8** | `cv::Scalar` 写成 RGB 顺序 | 蓝框被当成红框/绿框，颜色不符合要求 | **`Scalar(B,G,R)`** |
| **9** | 忘记建 `output/` | `imwrite` 返回 false，**图静默丢失** | 提前建目录 + **检查返回值** |
| **10** | 绝对路径 | 违反任务；换机器跑不了 | 全部**相对路径** |
| **11** | `imread` 后没查 `empty()` | 后续连锁报错，定位困难 | 读图后立即检查并打印尺寸 |
| **12** | 核用了偶数 | 锚点无中心、效果偏移，部分函数直接报错 | 一律**正奇数** |
| **13** | 把行列与 x/y 写反 | 框/中心点位置不对 | 点是 `(x,y)`；`at` 是 `(y,x)` |
| **14** | 用外接框面积做过滤 | 倾斜目标被误判 | 用 **`contourArea`** |
| **15** | 测试后忘记恢复参数 | **最终结果图不是原图的结果**，直接判错 | 测试记录 + **最后恢复重跑一次** |

---

## 11. ★ 调试排查表（照顺序查，别跳步）

### 11.1 引导官方排查表（现象 → 优先检查）

| 现象 | 优先检查 |
|---|---|
| 图片读取失败 | 文件是否在**运行目录对应的相对路径**下；文件名和扩展名是否正确 |
| mask 几乎全黑或全白 | **阈值方向、阈值大小、HSV 上下界**，以及传入的是**不是正确的滤波结果** |
| 目标在 mask 里是黑色 | **阈值类型或颜色范围是否选反**（后续轮廓把白色当前景） |
| 轮廓数量为 0 | mask 是否为**单通道**；**目标是否为白色**；轮廓函数是否收到处理后的 mask |
| 轮廓数量特别多 | mask 是否有很多白色噪点；**形态学是否起作用**；**面积门槛是否太低** |
| 两个目标被当成一个 | 它们在 mask 里**是否已经连接**；**膨胀或闭运算是否过强** |
| 框或中心点位置不对 | 绘图坐标是否来自**同一张图**；是否把 **x/y 写反** |
| 中心点异常 | 是否先检查 **`m.m00`**；轮廓是否**为空或退化** |
| 结果图片没有保存 | **输出目录是否存在**；是否检查 **`imwrite` 的返回值** |

### 11.2 我的"倒查顺序"（任务原文要求）

**轮廓数不是 6 时，按顺序检查：`mask` → 形态学结果 → 面积阈值。**

```
轮廓数 ≠ 6
 ├─ ① 看 mask：目标白不白？背景黑不黑？黄色被排除了吗？（打印非零像素数最直观）
 │     ├─ 全黑/全白 → 阈值方向 or 数值 or HSV 上下界 错
 │     └─ 黄色也是白 → HSV 上界太大 / 灰度阈值把黄色也收进来了
 ├─ ② 看形态学：四个输出是不是不一样？opened 里小绿点没了吗？
 │     └─ 四张一样 → 又链式调用了
 ├─ ③ 看面积阈值：把候选面积全打印出来，看 6 和 12 的分界在哪
 │     └─ 小点面积 ≥100 → 阈值太低 或 开运算核太小
 └─ ④（最后才看）绘图/中心点 —— 它们**不会改变轮廓数量**
```

**铁律**：**mask 不对就先别调轮廓；数量不对就先别调中心点。** 每次只改**最近新增的一步**。

### 11.3 快速自检命令（不用改代码）

| 想问什么 | 怎么在代码里最快得到答案 |
|---|---|
| mask 里有多少白色像素 | `cv::countNonZero(mask)` 打印一个数，比看图快 |
| 轮廓到底几条 | 打印 `contours.size()`，并在过滤前后各打一次 |
| 面积分布 | 循环里**无条件**先 `std::cout << cv::contourArea(c)`，再判断 |
| 形态学是否生效 | 把 `eroded/dilated/opened/closed` 与 `mask_hsv` 的 `countNonZero` 一起打印并对比 |
| 是否读到了对的图 | 打印 `cols/rows/channels()`，对照 960×640×3 |

---

## 12. 函数签名速查（含参数顺序）

```cpp
cv::Mat  cv::imread(const String& filename, int flags = IMREAD_COLOR);
bool     cv::imwrite(const String& filename, InputArray img);
void     cv::cvtColor(InputArray src, OutputArray dst, int code, int dstCn = 0);
void     cv::GaussianBlur(InputArray src, OutputArray dst, Size ksize,
                          double sigmaX, double sigmaY = 0, int borderType = BORDER_DEFAULT);
void     cv::medianBlur(InputArray src, OutputArray dst, int ksize);
double   cv::threshold(InputArray src, OutputArray dst, double thresh, double maxval, int type);
void     cv::inRange(InputArray src, InputArray lowerb, InputArray upperb, OutputArray dst);
cv::Mat  cv::getStructuringElement(int shape, Size ksize, Point anchor = Point(-1,-1));
void     cv::erode (InputArray src, OutputArray dst, InputArray kernel, Point anchor = Point(-1,-1),
                    int iterations = 1, int borderType = BORDER_CONSTANT, const Scalar& = ...);
void     cv::dilate(InputArray src, OutputArray dst, InputArray kernel, /* 同上 */);
void     cv::morphologyEx(InputArray src, OutputArray dst, int op, InputArray kernel, /* 同上 */);
void     cv::findContours(InputArray image, OutputArrayOfArrays contours, int mode, int method,
                          Point offset = Point());
double   cv::contourArea(InputArray contour, bool oriented = false);
cv::Rect cv::boundingRect(InputArray array);
cv::RotatedRect cv::minAreaRect(InputArray points);
cv::Moments cv::moments(InputArray array, bool binaryImage = false);
void     cv::rectangle(InputOutputArray img, Rect rec, const Scalar& color, int thickness = 1, ...);
void     cv::circle(InputOutputArray img, Point center, int radius, const Scalar& color,
                    int thickness = 1, ...);
void     cv::line(InputOutputArray img, Point p1, Point p2, const Scalar& color, ...);
void     cv::resize(InputArray src, OutputArray dst, Size dsize, double fx = 0, double fy = 0,
                    int interpolation = INTER_LINEAR);
```

---

## 13. 提交清单（勾选式自查）

### 13.1 产物清单（方案一，9 个文件）

| # | 文件 | 内容 |
|---|---|---|
| 1 | `output/fixed_blur_gray.png` | 灰度高斯滤波结果 |
| 2 | `output/fixed_mask_gray.png` | 灰度阈值 mask |
| 3 | `output/fixed_blur_color.png` | 彩色中值滤波结果 |
| 4 | `output/fixed_mask_hsv.png` | HSV 颜色 mask |
| 5 | `output/fixed_eroded.png` | 腐蚀（3×3，输入 mask_hsv） |
| 6 | `output/fixed_dilated.png` | 膨胀（3×3，输入 mask_hsv） |
| 7 | `output/fixed_opened.png` | 开运算（3×3，输入 mask_hsv）← **轮廓用它** |
| 8 | `output/fixed_closed.png` | 闭运算（7×7，输入 mask_hsv） |
| 9 | `output/fixed_result.png` | 原图副本 + 两种框 + 中心点 |

外加：**C++ 源码** + **CMake 配置** + **终端输出/截图** + **简短说明**。

### 13.2 自查表（引导第十一节 + 任务验收）

- [ ] 只选了一种方案并**完整完成**
- [ ] 生成全部 9 个输出文件（或方案二对应的 9 个 `custom_*`）
- [ ] 检查过 mask：**目标为白色、背景尽量为黑色**
- [ ] 理解四种形态学差别，且**分别从同一张 mask 开始**比较
- [ ] 用 `contourArea` 过滤小区域（阈值 **100**）
- [ ] 对有效轮廓**同时**计算外接框与中心点
- [ ] 检查过 `moments.m00`，终端输出**目标数量、每个目标面积和坐标**
- [ ] 完成 4 项测试，且**最后恢复正常参数重新生成结果**
- [ ] 代码和说明**全部相对路径**，无本机绝对路径

### 13.3 说明文档必须回答的问题（**提交硬性要求**）

| 方案 | 必须写的内容 |
|---|---|
| **方案一** | ① **两张 mask 的差别**；② **四种形态学操作的效果** |
| 方案二 | ① 图片来源、目标、要排除区域、**让分割变难的干扰因素**；② 两张 mask 的比较；③ **选用哪条路线及参数**；④ 形态学效果；⑤ 检测到的目标数 |

---

## 14. 附：数据锚点（本图专用，便于快速判断异常）

| 指标 | **[实测] 正常值** | 异常含义 |
|---|---|---|
| 尺寸 | 960 × 640 × 3ch | 读到别的图 / 读了灰度 |
| 绿色像素总数 | **70250** | 偏少 → 阈值太严；偏多 → 把背景/黄色收进来了 |
| candidate contours | **12** | 少 → mask 合并了目标；多 → 有噪点 |
| 过滤后轮廓数 | **6** | 本周**唯一正确验收值** |
| 主体面积范围 | **5145 – 20054** | 若出现 <100 的"目标" → 小点混进来了 |
| 小点面积范围 | **21 – 37** | 都 <100，应被清掉 |
| 绿色 HSV | H=59, S=170, V=120 | 与 (35,80,80)~(85,255,255) 对照 |
| 黄色 HSV | H=25 | 应在上界 85 之外 |
| 缩半后主体面积 | 约 **1286 – 5014**（原 1/4） | 仍 ≫100 → **面积阈值 100 不需要改**（测试题答案） |
