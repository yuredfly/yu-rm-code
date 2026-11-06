# OpenCV 第 6 周：官方文档与检索关键词清单

> 用途：预习/实现/排错时的**查阅索引**。按"概念 → 官方链接 → 检索关键词 → 该看哪一段"组织。
> 适用版本：**OpenCV 4.x**（`docs.opencv.org/4.x/` 会自动跳到当前最新，如 4.13.0）。
> 生成时间：随 `10.12-10.18` 任务一起使用。

---

## 0. 最重要的三条查阅原则

| 原则 | 说明 |
|---|---|
| **1. 先查函数签名，再查教程** | 教程给"怎么用"，**签名给"参数顺序和默认值"**。本周 90% 的编译错误来自参数顺序/类型（如 `medianBlur` 第 3 参是 `int` 而不是 `Size`） |
| **2. C++ 与 Python 页面要分清** | 教程页默认常见 Python 版本（如 `tutorial_py_*`）。**C++ 版看带 `tutorial_*`（无 `_py`）的页面**，或用 doxygen 函数页里的 `void cv::xxx(...)` 签名 |
| **3. 版本要对齐** | 你本机 OpenCV 版本决定 `RotatedRect::angle` 语义、`findContours` 是否修改输入等行为。**先查版本再看结论**（见第 5 节） |

---

## 1. 官方文档总入口

| 名称 | 链接 | 用途 |
|---|---|---|
| **当前版本文档（自动跳转）** | https://docs.opencv.org/4.x/ | **收藏这个**，永远指向最新 4.x |
| 文档首页/教程总目录 | https://docs.opencv.org/4.x/d9/df8/tutorial_root.html | 按模块找教程 |
| **imgproc 教程目录**（本周主战场） | https://docs.opencv.org/4.x/d7/da8/tutorial_table_of_content_imgproc.html | 平滑/阈值/形态学/轮廓教程全在这里 |
| imgproc API 总索引 | https://docs.opencv.org/4.x/d7/dbd/group__imgproc.html | 找函数所属分组 |
| **Image Filtering**（滤波 + 形态学 API） | https://docs.opencv.org/4.x/d4/d86/group__imgproc__filter.html | `GaussianBlur`/`medianBlur`/`erode`/`dilate`/`morphologyEx`/`getStructuringElement` |
| **Image Processing → Misc**（阈值） | https://docs.opencv.org/4.x/d7/d1b/group__imgproc__misc.html | `threshold`/`adaptiveThreshold` |
| **Color conversions** | https://docs.opencv.org/4.x/d8/d01/group__imgproc__color__conversions.html | `cvtColor` + `COLOR_*` 常量 |
| **Contours（API 分组）** | https://docs.opencv.org/4.x/d3/dc0/group__imgproc__shape.html | `findContours`/`contourArea`/`boundingRect`/`minAreaRect`/`moments` |
| **Drawing functions** | https://docs.opencv.org/4.x/d6/d6e/group__imgproc__draw.html | `rectangle`/`circle`/`line`/`polylines` |
| **Image file input/output** | https://docs.opencv.org/4.x/d4/da8/group__imgcodecs.html | `imread`/`imwrite`/`imshow` + `IMREAD_*` |
| **Basic structures（Mat/Point/Size/Rect/Scalar）** | https://docs.opencv.org/4.x/dc/d84/group__core__basic.html | 参数类型速查 |
| **BorderTypes（enum）** | https://docs.opencv.org/4.x/d2/de8/group__core__array.html | `BORDER_CONSTANT` 等边界语义 |
| 中文镜像（备用） | https://docs.opencv.ac.cn/4.10.0/ | 英文页读不动时的对照 |

---

## 2. 按本周知识点：教程页 + 检索关键词 + **该看哪一段**

### 2.1 滤波（GaussianBlur / medianBlur）

| 项 | 内容 |
|---|---|
| **官方教程** | [Smoothing Images](https://docs.opencv.org/4.x/dc/dd3/tutorial_gausian_median_blur_bilateral_filter.html) |
| **API 页** | [Image Filtering](https://docs.opencv.org/4.x/d4/d86/group__imgproc__filter.html) |
| **检索关键词** | `OpenCV GaussianBlur ksize sigmaX 0 meaning` · `OpenCV medianBlur ksize must be odd` · `OpenCV GaussianBlur sigma formula 0.3*((ksize-1)*0.5-1)+0.8` · `OpenCV smoothing images C++ tutorial` |
| **该看哪一段** | ① `ksize` 必须是**正奇数**的说明；② `sigmaX=0` 时**由 ksize 推算**；③ `medianBlur` 的 `ksize` 是 **int 不是 Size**；④ 教程里三个滤波器的对比表（选型依据） |
| **本周落点** | `GaussianBlur(5×5, 0)` 于灰度路线；`medianBlur(5)` 于 HSV 路线；**滤波结果必须送进分割，不要送原图** |

### 2.2 阈值（threshold）

| 项 | 内容 |
|---|---|
| **官方教程** | [Basic Thresholding Operations](https://docs.opencv.org/4.x/db/d8e/tutorial_threshold.html) |
| **API 页** | [Miscellaneous Image Transformations](https://docs.opencv.org/4.x/d7/d1b/group__imgproc__misc.html) |
| **检索关键词** | `OpenCV threshold THRESH_BINARY THRESH_BINARY_INV difference` · `OpenCV threshold type table <= thresh maxval` · `OpenCV threshold C++ example` · `OpenCV adaptiveThreshold vs threshold` |
| **该看哪一段** | ① **五种阈值类型的数学表达式表**（判断方向就看这张表）；② `maxval` 的作用；③ 返回值含义；④ `THRESH_OTSU` 的组合写法（了解即可，**本周不用**） |
| **本周落点** | `threshold(blur_gray, mask_gray, 120, 255, THRESH_BINARY_INV)`；**目标比背景暗 → INV**（本图绿 87 < 120 < 背景 248） |

### 2.3 HSV 与 inRange

| 项 | 内容 |
|---|---|
| **官方教程** | [Thresholding Operations using inRange](https://docs.opencv.org/4.x/da/d97/tutorial_threshold_inRange.html) · [Color conversions API](https://docs.opencv.org/4.x/d8/d01/group__imgproc__color__conversions.html) |
| **检索关键词** | `OpenCV inRange HSV lowerb upperb Scalar C++` · `OpenCV HSV hue range 0 179 why not 360` · `OpenCV COLOR_BGR2HSV hue 0-180 scale` · `OpenCV inRange output single channel binary` |
| **该看哪一段** | ① `cvtColor` 的 `code` 枚举里 **`COLOR_BGR2HSV`**（注意是 BGR 在前）；② 教程里**滑条调 HSV 上下界**的做法（这是本周"每次只调一个界"的官方对应做法）；③ `inRange` 说明里"输出为 8 位单通道" |
| **本周落点** | `inRange(hsv, Scalar(35,80,80), Scalar(85,255,255), mask_hsv)`；**H∈0–179**，本图绿 H=59、黄 H=25 |

### 2.4 形态学（erode / dilate / morphologyEx / getStructuringElement）

| 项 | 内容 |
|---|---|
| **官方教程** | [Eroding and Dilating](https://docs.opencv.org/4.x/db/df6/tutorial_erosion_dilatation.html) · [More Morphology Transformations](https://docs.opencv.org/4.x/d3/dbe/tutorial_opening_closing_hats.html) |
| **API 页** | [Image Filtering](https://docs.opencv.org/4.x/d4/d86/group__imgproc__filter.html) |
| **检索关键词** | `OpenCV morphologyEx MORPH_OPEN MORPH_CLOSE C++` · `OpenCV getStructuringElement MORPH_RECT ellipse cross` · `OpenCV erode dilate kernel odd size` · `OpenCV morphologyEx iterations equivalent kernel size` · `OpenCV morphological opening removes small white noise` |
| **该看哪一段** | ① **`cv::MorphTypes` 枚举表**（`MORPH_OPEN=2`、`MORPH_CLOSE=3` 等）；② **`cv::MorphShapes`**（`RECT/CROSS/ELLIPSE/DIAMOND`）；③ `erode` 的 `dst(x,y)=min(...)`、`dilate` 的 `max(...)` 公式（**这是"腐蚀缩小、膨胀扩大"的数学根据**）；④ `kernel=Mat()` 时默认 3×3 的说明 |
| **本周落点** | **四种操作各自独立以 `fixed_mask_hsv` 为输入**；`MORPH_OPEN` 3×3 用于后续轮廓；`MORPH_CLOSE` 7×7 仅用于观察 |

### 2.5 轮廓（findContours）

| 项 | 内容 |
|---|---|
| **官方教程** | [Finding contours in your image](https://docs.opencv.org/4.x/df/d0d/tutorial_find_contours.html) |
| **API 页** | [Structural Analysis and Shape Descriptors](https://docs.opencv.org/4.x/d3/dc0/group__imgproc__shape.html) |
| **检索关键词** | `OpenCV findContours RETR_EXTERNAL CHAIN_APPROX_SIMPLE meaning` · `OpenCV RetrievalModes RETR_LIST CCOMP TREE difference` · `OpenCV ContourApproximationModes CHAIN_APPROX_SIMPLE` · `OpenCV findContours modifies input image clone` · `OpenCV findContours hierarchy Vec4i meaning` |
| **该看哪一段** | ① **`RetrievalModes` 枚举 + 每个模式的示意图**（判断该用 EXTERNAL 还是 TREE）；② **`ContourApproximationModes`**；③ 教程页里"**输入必须是单通道二值图、白色为前景**"的说明；④ `hierarchy` 的四元组含义（本周可跳） |
| **本周落点** | `findContours(fixed_opened.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE)` |

### 2.6 面积与几何（contourArea / boundingRect / minAreaRect / moments）

| 项 | 内容 |
|---|---|
| **官方教程** | [Contour Features](https://docs.opencv.org/4.4.0/dd/d49/tutorial_py_contour_features.html)（Python 版但概念通用）· [Creating Bounding boxes and circles for contours](https://docs.opencv.org/4.x/da/d0c/tutorial_bounding_rects_circles.html) · [Creating Bounding rotated boxes and ellipses](https://docs.opencv.org/4.x/de/d62/tutorial_bounding_rotated_ellipses.html) · [Image Moments](https://docs.opencv.org/4.x/d0/d49/tutorial_moments.html) |
| **API 页** | [Structural Analysis and Shape Descriptors](https://docs.opencv.org/4.x/d3/dc0/group__imgproc__shape.html) |
| **检索关键词** | `OpenCV contourArea pixel squared unit` · `OpenCV boundingRect vs minAreaRect difference rotated` · `OpenCV RotatedRect points corners draw rotated box C++` · `OpenCV moments m00 m10 m01 centroid formula` · `OpenCV RotatedRect angle changed 4.5 range 0 90` · `OpenCV moments m00 zero division guard` |
| **该看哪一段** | ① `contourArea` 的**参数 `oriented`** 与单位说明；② `boundingRect` 返回 `Rect` 的字段；③ **`RotatedRect` 的 `center/size/angle` 三个成员 + `points()` 方法**（画旋转框必看）；④ **`cv::Moments` 结构体成员表**（`m00` 与 `mu00`/`nu00` 的区别）；⑤ **`angle` 的版本变更说明**（旧教程会误导） |
| **本周落点** | 面积阈值 100；两框不同颜色；**`m00 != 0` 后才做除法** |

### 2.7 绘制与读写

| 项 | 内容 |
|---|---|
| **官方教程** | [Basic Drawing](https://docs.opencv.org/4.x/d3/d96/tutorial_basic_geometric_drawing.html) |
| **API 页** | [Drawing Functions](https://docs.opencv.org/4.x/d6/d6e/group__imgproc__draw.html) · [Image file input/output](https://docs.opencv.org/4.x/d4/da8/group__imgcodecs.html) |
| **检索关键词** | `OpenCV cv::Scalar BGR order rectangle color` · `OpenCV rectangle thickness FILLED -1` · `OpenCV imwrite returns false output directory not exist` · `OpenCV imread returns empty Mat no exception` · `OpenCV imread relative path working directory` |
| **该看哪一段** | ① 绘制页开头"**颜色按 BGR 顺序传入 `Scalar`**"的说明；② `thickness=-1` = 填充；③ `imwrite` 的**返回值与支持的扩展名**；④ `imread` 的 `IMREAD_*` 枚举 |
| **本周落点** | 绿框 `Scalar(0,255,0)`、红点 `Scalar(0,0,255)`、两框不同色；`output/` 必须已存在 |

### 2.8 缩放（resize，测试用）

| 项 | 内容 |
|---|---|
| **API 页** | [Geometric Image Transformations](https://docs.opencv.org/4.x/da/d54/group__imgproc__transform.html) |
| **检索关键词** | `OpenCV resize dsize vs fx fy which wins` · `OpenCV resize INTER_AREA downscale INTER_LINEAR upscale` · `OpenCV resize half size C++` |
| **该看哪一段** | ① **`dsize` 与 `fx/fy` 只要给一个，两者都给时 `dsize` 优先**；② `interpolation` 枚举里**缩小推荐 `INTER_AREA`** 的说明 |
| **本周落点** | 测试④"缩半后重新处理"；**面积按 1/4 缩放**，据此判断面积阈值是否要改 |

---

## 3. 检索关键词总清单（可直接复制）

### 3.1 函数级（英文）
```
cv::GaussianBlur C++ signature ksize sigmaX
cv::medianBlur ksize odd int not Size
cv::threshold THRESH_BINARY_INV explained
cv::inRange lowerb upperb Scalar HSV
cv::cvtColor COLOR_BGR2GRAY COLOR_BGR2HSV
cv::getStructuringElement MORPH_RECT
cv::erode cv::dilate kernel anchor iterations
cv::morphologyEx MORPH_OPEN MORPH_CLOSE
cv::findContours RETR_EXTERNAL CHAIN_APPROX_SIMPLE
cv::contourArea oriented pixel area
cv::boundingRect cv::minAreaRect difference
cv::Moments m00 m10 m01 centroid
cv::rectangle cv::circle cv::line thickness
cv::imread cv::imwrite empty() check
cv::resize INTER_AREA half size
```

### 3.2 概念级（英文）
```
OpenCV binary mask white foreground black background
OpenCV HSV hue 0-179 scale factor 2
OpenCV morphological opening closing removes noise fills holes
OpenCV structuring element odd size requirement
OpenCV border type constant morphological default border value
OpenCV contour hierarchy RETR_EXTERNAL meaning
OpenCV BGR channel order Scalar color confusion
OpenCV Mat shallow copy clone deep copy
OpenCV working directory relative path imread fails
```

### 3.3 问题级（排错直接用，中英双写）
```
OpenCV findContours returns 0 contours binary mask
OpenCV mask all black or all white threshold inverted
OpenCV contour count too many noise area threshold
OpenCV imwrite returns false image not saved output folder
OpenCV moments m00 zero division
OpenCV RotatedRect angle range change 4.5
OpenCV medianBlur assertion ksize odd
OpenCV 轮廓数量为 0 排查
OpenCV mask 全黑 全白 阈值方向
OpenCV 形态学打开的核大小选择
OpenCV 旋转矩形 angle 含义 版本变化
```

### 3.4 中文检索（附站点限定）
```
site:docs.opencv.org findContours RETR_EXTERNAL
site:docs.opencv.org morphologyEx MORPH_OPEN
site:docs.opencv.ac.cn 阈值 THRESH_BINARY_INV
site:github.com/opencv/opencv samples cpp imgproc
OpenCV 轮廓 面积 外接矩形 质心 教程
OpenCV 形态学 开运算 闭运算 区别
```

---

## 4. 权威度排序（时间不够时按这个顺序查）

| 优先级 | 来源 | 适合查什么 |
|---|---|---|
| **1** | **官方 doxygen 函数页**（`docs.opencv.org/4.x/...`） | 签名、参数含义、默认值、枚举取值、版本变更 |
| **2** | **官方 C++ 教程页**（`tutorial_*` 无 `_py`） | 完整用法流程、可运行示例的结构 |
| **3** | **官方 samples 源码**（GitHub `samples/cpp/tutorial_code/imgproc/`） | 真实可编译代码、CMake 写法 |
| 4 | OpenCV 官方论坛 https://forum.opencv.org/ | 官方维护者回答的具体报错 |
| 5 | Stack Overflow（标签 `opencv`） | 报错信息、编译器问题的现实解 |
| 6 | 中文镜像 https://docs.opencv.ac.cn/ | 英文吃力时对照（**版本可能滞后**） |
| 7 | 博客/CSDN/知乎 | **最后参考**，务必回官方页面核对参数（大量文章混用 3.x/4.x 与 Python/C++） |

---

## 5. 版本与本地自查（避免被旧教程误导）

| 要确认的事 | 方法 |
|---|---|
| 本机 OpenCV 版本 | 命令 `pkg-config --modversion opencv4`（Linux/macOS）；CMake 里 `find_package(OpenCV REQUIRED)` 后 `message(${OpenCV_VERSION})`；或看 `opencv2/core/version.hpp` 的 `CV_VERSION` |
| `RotatedRect::angle` 语义 | 4.5 之前为 `[-90,0)`，4.5+ 为 `[0,90]` → **看 API 页对应版本的说明**，不要照抄旧博客 |
| `findContours` 是否改输入 | 看 API 页参数表里 `image` 的说明（4.x 明确不改；3.x 会改）→ 所以引导要求 **`clone()`** 仍是最稳做法 |
| CMake 配置 | 官方 [Installation in Linux / Windows](https://docs.opencv.org/4.x/d9/df8/tutorial_root.html) 与 [Using OpenCV with gcc and CMake](https://docs.opencv.org/4.x/db/df5/tutorial_linux_gcc_cmake.html) |
| **本地头文件就是最权威文档** | `opencv2/imgproc.hpp` 等头文件里每个函数上方都有注释；编辑器 `F12`/跳转定义最快 |

---

## 6. 按学习阶段（配合第四步 P0–P8）的查阅计划

| 阶段 | 查什么 | 用哪个关键词 |
|---|---|---|
| **P0** 工程与读图 | `imread`/`imwrite`/`empty`/`Mat::cols` | `cv::imread cv::imwrite empty check relative path` |
| **P1** 灰度阈值 | `cvtColor` + `threshold` 类型表 | `OpenCV threshold type table THRESH_BINARY_INV` |
| **P2** HSV | `COLOR_BGR2HSV`、H 范围、`inRange` | `OpenCV HSV hue 0-179 inRange Scalar` |
| **P3** 形态学 | `MorphTypes`/`MorphShapes`、开闭定义 | `OpenCV morphologyEx MORPH_OPEN MORPH_CLOSE tutorial` |
| **P4** 轮廓 | `RetrievalModes`/`ContourApproximationModes` | `OpenCV findContours RETR_EXTERNAL CHAIN_APPROX_SIMPLE` |
| **P5** 面积 | `contourArea` 单位与 `oriented` | `OpenCV contourArea pixel squared` |
| **P6** 测量 | `RotatedRect`/`Moments` 成员表 | `OpenCV RotatedRect points cv::Moments m00` |
| **P7** 绘制 | 颜色顺序、`thickness`、`imwrite` 返回值 | `OpenCV cv::Scalar BGR rectangle color` |
| **P8** 测试 | `resize` 的 `dsize`/`interpolation` | `OpenCV resize INTER_AREA half size` |

---

## 7. 查文档时的 5 个高效技巧

| 技巧 | 做法 |
|---|---|
| 用页面内搜索代替翻页 | 在 API 页按 `Ctrl+F` 搜函数名（doxygen 页很长，`GaussianBlur` 在 Image Filtering 页中段） |
| 直接跳到签名 | doxygen 页里函数标题形如 `◆ GaussianBlur()`，其下就是参数表 |
| 记 URL 规律 | 教程在 `tutorial_*.html`，API 在 `group__*.html`，**收藏 group 页比收藏单个函数更耐用** |
| 看 `See also` 与 `Python:` 行 | `See also` 给相关函数（如 `erode`↔`morphologyEx`）；`Python:` 行给参数可选性顺序 |
| 对照头文件 | 把 `opencv2/imgproc.hpp` 用编辑器打开搜函数名，能直接看到重载与默认值 |

---

## 8. 常见"文档陷阱"清单

| 陷阱 | 具体表现 | 正确做法 |
|---|---|---|
| Python 教程直接搬 | `cv2.threshold(...)` 返回值顺序与 C++ 不同 | 看 **C++ 签名**，用 doxygen 页 |
| 版本语义漂移 | `RotatedRect::angle`、`findContours` 输入修改 | 核对**你本机版本**对应文档 |
| 混合 3.x/4.x | 常量名/行为不同（如 `CV_LOAD_IMAGE_COLOR` 已废） | 用 `cv::IMREAD_COLOR` 等 4.x 新名 |
| 把参数顺序记成 Python | `medianBlur(src, dst, 5)` vs `cv2.medianBlur(src,5)` | C++ **第 2 参永远是 dst** |
| 忽略"输出尺寸/类型"说明 | 以为 `inRange` 输出 3 通道 | 文档明确：**输出 8 位单通道** |
| 忽略 `empty()` 与返回值 | `imread`/`imwrite` 都不抛异常 | **必须检查** `empty()` 与 `imwrite` 返回值 |
