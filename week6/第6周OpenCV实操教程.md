# 第 6 周实操教程：OpenCV 预处理 · 阈值 · 形态学 · 轮廓目标检测

> 对应任务：`10.12-10.18 任务.pdf`（方案一：处理指定练习图 `images/input.png`）
> 本教程代码**已在本机编译并运行通过**：OpenCV **4.14.0** + CMake 4.4.4 + MSVC，实测结果见第 12 节。
> 图例：**【强制】** = 任务硬性要求；**【可选】** = 拓展，不做不扣分。

---

## 1. 项目目录结构

```
week6/                          ← 工程根目录（可执行文件的工作目录基准）
├─ CMakeLists.txt               ← 沿用上周风格（find_package(OpenCV) + file(MAKE_DIRECTORY ...)）
├─ week6.hpp                    ← 本周功能声明
├─ week6.cpp                    ← 本周功能实现
├─ main.cpp                     ← 主流程（参数区 + 8 步串起来）
├─ images/
│  └─ input.png                 ← 【强制】方案一指定练习图（960×640，保持原样）
├─ output/                      ← 【强制】必须提前存在，imwrite 不会自动建目录
│  ├─ fixed_blur_gray.png
│  ├─ fixed_mask_gray.png
│  ├─ fixed_blur_color.png
│  ├─ fixed_mask_hsv.png
│  ├─ fixed_eroded.png
│  ├─ fixed_dilated.png
│  ├─ fixed_opened.png
│  ├─ fixed_closed.png
│  └─ fixed_result.png
└─ build/                       ← CMake 生成目录（不提交）
   └─ Release/
      ├─ week6.exe
      ├─ images/input.png       ← 由 CMake POST_BUILD 自动复制，保证相对路径可用
      └─ output/                ← 程序把 9 张图写在这里
```

**关于相对路径的两种运行方式（二选一，先确认你用的是哪种）**

| 方式 | 工作目录 | 需要的准备 |
|---|---|---|
| **CMake POST_BUILD 复制**（本教程采用） | `build/Release/` | CMake 自动把 `images/` 复制到 exe 旁边，`output/` 也在旁边 |
| **手工把 exe 放到工程根** | `week6/` | 直接把 `week6.exe` 复制到 `week6/` 下运行；`images/`、`output/` 已在原位 |

> **【强制】代码中只写 `images/input.png`、`output/fixed_*.png` 这样的相对路径**，
> 绝对不要写 `C:\...` 或 `D:\...`（任务原文：不要写入依赖特定设备的绝对路径）。
> 相对路径的基准是**程序运行时的当前工作目录**，不是源码所在目录 —— 这是"Failed to read image"最常见的原因。

### 1.1 关于你现有的 `opencv_basic` 工程（重要）

你现有的 `opencv_basic/` 目录**当前是只读的**：NTFS 权限里你的账户只有"读取和执行"，没有"写入"，
因此**无法在其中创建 `build/`、也无法编译**（实测：在其中新建任何目录/文件都被拒绝）。

两个选择，**任选其一**：

| 选择 | 做法 |
|---|---|
| **A（本教程采用的，零风险）** | 用新建的 `week6/` 工程；把 `week6.hpp / week6.cpp / main.cpp` 三个文件 + 第 3 节的 CMake 配置作为提交内容 |
| **B（沿用原工程名）** | 先给 `opencv_basic` 加上写入权限（在文件资源管理器里右键 → 属性 → 安全 → 编辑 → 给你的用户"完全控制"，或让老师/管理员执行一次），再把三个文件放进去编译 |

> 我没有擅自修改你既有目录的权限，避免动到你上周已完成的工程。

---

## 2. 主流程：8 步对应任务的 4 个子任务

```
S1 读取并检查        imread + empty() + 打印 宽/高/通道数        ← 任务 1
S2 生成两张 mask    灰度路线 + HSV 路线，分别保存、互不覆盖      ← 任务 2
S3 四种形态学       全部以 maskHsv 为输入，各存一张             ← 任务 3
S4 轮廓 + 面积过滤  findContours(RETR_EXTERNAL) + area<100      ← 任务 4.1~4.2
S5 测量              boundingRect + minAreaRect + moments       ← 任务 4.3~4.4
S6 标注并保存        原图副本上画绿框/橙框/红点 → fixed_result  ← 任务 4.5
S7 终端输出          每个目标的面积、中心点、最终总数           ← 任务 4.5
S8 查看中间结果      imshow 各中间图（引导要求"打开检查"）       ← 引导第九节
```

---

## 3. `CMakeLists.txt`（完整）

```cmake
cmake_minimum_required(VERSION 3.16)
project(week6_opencv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# 沿用上周工程的写法：提前创建输出文件夹
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/images")

add_executable(week6
    week6.cpp
    main.cpp
)

# 源码含中文注释（UTF-8）：MSVC 需要 /utf-8，否则按 GBK 解析会报语法错误
if (MSVC)
    target_compile_options(week6 PRIVATE /utf-8)
endif()

target_include_directories(week6 PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(week6 PRIVATE ${OpenCV_LIBS})

# 代码用相对路径 images/input.png 与 output/xxx.png，
# 因此把 images/ 与 output/ 复制到可执行文件旁边
add_custom_command(TARGET week6 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:week6>/output"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/images"
            "$<TARGET_FILE_DIR:week6>/images"
    COMMENT "Copying images/ next to the week6 executable"
)
```

**【可选·若沿用你原 `opencv_basic` 工程】**：把上面的 `add_executable`/`target_*`/`add_custom_command` 四段
追加到既有 `CMakeLists.txt` 末尾即可，其余（`cmake_minimum_required`、`find_package`、`file(MAKE_DIRECTORY)`）你已经有了。

---

## 4. 代码逐段讲解

> 下表每段都标注：**做什么 → 为什么 → 强制约束**。完整文件在 `week6.hpp / week6.cpp / main.cpp`。

### 4.1 段 1：参数区（先集中定义，便于测试时改一处）

```cpp
// ---------------------------------------------------------------- 参数区（全部为任务给定的固定参数）
const std::string inputPath  = "images/input.png";   // 项目内相对路径
const std::string outputDir  = "output";             // 相对路径；需提前存在

const int    gaussianKsize   = 5;                    // 灰度路线高斯核 5x5
const double thresholdValue  = 120.0;                // 灰度阈值
const int    medianKsize     = 5;                    // HSV 路线中值核
const cv::Scalar hsvLower(35, 80, 80);               // HSV 下界 (H,S,V)
const cv::Scalar hsvUpper(85, 255, 255);             // HSV 上界

const int    kernelSize3     = 3;                    // 腐蚀/膨胀/开运算核
const int    kernelSize7     = 7;                    // 闭运算核
const double minContourArea  = 100.0;                // 面积阈值：小于该值丢弃
```

**【强制】** 这些数值必须与任务一致：`5×5`、`120`、`THRESH_BINARY_INV`、中值核 `5`、`(35,80,80)~(85,255,255)`、核 `3×3`/`7×7`、面积 `100`。
**为什么集中放参数区**：测试②要临时改 HSV 上下界、测试④要改缩放，集中一处才不会漏改。

### 4.2 段 2：读取并检查（任务子任务 1）

```cpp
cv::Mat week6::loadImage(const std::string& path)
{
    cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);   // 失败时不抛异常，返回空 Mat
    if (image.empty()) {
        std::cerr << "[ERROR] Failed to read image. Check the path and file name: " << path << "\n";
        std::cerr << "        常见原因：文件不在工作目录下的相对路径、扩展名写错、运行目录不是项目目录。\n";
    }
    return image;
}

void week6::printImageInfo(const cv::Mat& image)
{
    std::cout << "width=" << image.cols
              << " height=" << image.rows
              << " channels=" << image.channels() << "\n";
    if (image.cols != 960 || image.rows != 640 || image.channels() != 3) {
        std::cout << "[WARN] 尺寸/通道数与任务描述的 960x640、3 通道不一致，请确认读到的是 images/input.png\n";
    }
}
```

```cpp
cv::Mat image = week6::loadImage(inputPath);
if (image.empty()) {
    return 1;                       // 【强制】读取失败时提示并结束
}
week6::printImageInfo(image);
```

**【强制】** ① `imread` 后**必须** `empty()` 检查；② 失败要**提示并结束**，不能继续跑；③ 必须打印宽/高/通道数。
**为什么**：空图会让阈值、轮廓、绘图**连锁出错**，报错位置会离真正原因很远。

### 4.3 段 3：灰度固定阈值路线

```cpp
void week6::routeGrayThreshold(const cv::Mat& bgr,
                               double thresholdValue,
                               cv::Mat& blurOut,
                               cv::Mat& maskOut)
{
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);   // 注意 OpenCV 是 BGR 顺序

    const int gaussianKsize = 5;                   // 高斯核必须为正奇数
    cv::GaussianBlur(gray, blurOut, cv::Size(gaussianKsize, gaussianKsize), 0);  // sigmaX=0 → 自动推算

    // 本图目标（深绿，灰度约 87）比背景（约 248）暗 -> 必须用 THRESH_BINARY_INV
    cv::threshold(blurOut, maskOut, thresholdValue, 255.0, cv::THRESH_BINARY_INV);
}
```

**【强制】** 顺序必须是 `BGR→灰度 → GaussianBlur(5×5) → threshold(120, INV)`；滤波核 `5×5`。
**坑**：必须把 `blurOut`（滤波结果）送进 `threshold`，**不要用原图 `gray`**。
**方向判断**：`THRESH_BINARY` 是"高于阈值变白"，`INV` 是"低于等于阈值变白"；本图目标比背景**暗** → 用 `INV`。
**❌ 常见错误**：写成 `cv::threshold(gray, ...)`（漏掉滤波）→ 噪点全进 mask。

### 4.4 段 4：HSV 颜色筛选路线

```cpp
void week6::routeHsvInRange(const cv::Mat& bgr,
                            int medianKsize,
                            const cv::Scalar& lower,
                            const cv::Scalar& upper,
                            cv::Mat& blurOut,
                            cv::Mat& maskOut)
{
    cv::medianBlur(bgr, blurOut, medianKsize);     // 核参数是 int（不是 Size），须为 >1 的奇数
    cv::Mat hsv;
    cv::cvtColor(blurOut, hsv, cv::COLOR_BGR2HSV); // 必须用滤波后的图
    cv::inRange(hsv, lower, upper, maskOut);       // 输出天然是单通道二值图，不用再 threshold
}
```

**【强制】** 顺序必须是 `medianBlur(5) → BGR转HSV → inRange`；两条路线**用不同变量保存、互不覆盖**。
**为什么两条路线的滤波器不同**：`medianBlur` 用邻域中间值，对**孤立小点**更有效；`GaussianBlur` 是一般平滑。任务如此规定，**不要互换**。
**坑**：`inRange` 的输出已经是二值图，**不要再套一层 `threshold`**。

### 4.5 段 5：四种形态学 —— ★ 硬性约束核心段

```cpp
void week6::morphologyFour(const cv::Mat& maskIn,
                           int kernelSize3, int kernelSize7,
                           cv::Mat& eroded, cv::Mat& dilated,
                           cv::Mat& opened, cv::Mat& closed)
{
    const cv::Mat kernel3 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize3, kernelSize3));
    const cv::Mat kernel7 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize7, kernelSize7));

    // ★★★【强制】下面四次调用的第一个实参全都是 maskIn ★★★
    //   绝对不能写成 morphologyEx(eroded, opened, ...) 这类链式处理！
    cv::erode(maskIn, eroded, kernel3);                              // 白色区域缩小
    cv::dilate(maskIn, dilated, kernel3);                            // 白色区域扩大
    cv::morphologyEx(maskIn, opened, cv::MORPH_OPEN, kernel3);       // 先腐后膨：去小白噪点
    cv::morphologyEx(maskIn, closed, cv::MORPH_CLOSE, kernel7);      // 先膨后腐：填小黑孔
}
```

**【强制】四种操作都以 `fixed_mask_hsv` 为输入，并分别运行、分别保存，不能把它们串成一条连续处理链。**

| 错误写法 | 实际发生什么 | 你会看到 |
|---|---|---|
| `erode(mask, eroded); dilate(eroded, dilated);` | 这两步合起来**就是开运算** | `fixed_eroded.png` 与 `fixed_opened.png` 几乎一样，无法对比 |
| `morphologyEx(opened, closed, MORPH_CLOSE, k7)` | 在已经去过噪点的图上做闭运算 | **看不到原始小孔被填补**的效果 |
| 四个串成一条 | 只有最后一张是"新的" | 中间四张图**全部失真** |

**自检口诀**：数一遍四个调用的**第一个实参**，必须是**同一个变量名**。

```cpp
cv::Mat eroded, dilated, opened, closed;
week6::morphologyFour(maskHsv, kernelSize3, kernelSize7, eroded, dilated, opened, closed);
//                        ^^^^^^^ 只传了 maskHsv，没有传任何中间结果
```

**【强制】** 后续轮廓检测**必须用开运算结果 `opened`**（因为要去掉小绿点）；闭运算结果**只用于观察和比较**。

### 4.6 段 6：轮廓提取 + 面积过滤

```cpp
std::vector<std::vector<cv::Point>> week6::findTargetContours(const cv::Mat& binaryMask,
                                                              double minArea,
                                                              bool printCandidateAreas)
{
    std::vector<std::vector<cv::Point>> contours;

    // 【强制】传 clone()，保留原 mask 供查看或保存
    cv::Mat contourInput = binaryMask.clone();
    cv::findContours(contourInput, contours,
                     cv::RETR_EXTERNAL,          // 【强制】只取最外层轮廓
                     cv::CHAIN_APPROX_SIMPLE);   // 【强制】压缩连续冗余点

    std::vector<std::vector<cv::Point>> kept;
    double maxFilteredArea = 0.0;
    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);   // 单位：像素平方
        if (printCandidateAreas) {
            std::cout << "candidate area: " << area << "\n";   // 【强制】输出候选面积
        }
        if (area < minArea) {                            // 【强制】面积小于 100 丢弃
            maxFilteredArea = std::max(maxFilteredArea, area);
            continue;
        }
        kept.push_back(contour);
    }
    std::cout << "candidate contours (before filter): " << contours.size() << "\n";
    std::cout << "valid targets (after filter): " << kept.size() << "\n";
    std::cout << "largest filtered-out area: " << maxFilteredArea
              << "（面积阈值 " << minArea << "，可据此判断阈值是否合适）\n";
    return kept;
}
```

```cpp
const std::vector<std::vector<cv::Point>> targets =
    week6::findTargetContours(opened, minContourArea, true);   // ← 用 opened，不是 maskHsv
```

**【强制】** ① 输入必须是 `fixed_opened`；② 必须 `RETR_EXTERNAL` + `CHAIN_APPROX_SIMPLE`；③ 必须输出候选面积；④ 面积 < 100 丢弃。
**坑**：若这里误用 `maskHsv`，6 个小绿点会一起进来 → **目标数变成 12**。

### 4.7 段 7：测量（两种框 + 中心点）

```cpp
week6::TargetInfo week6::measureTarget(const std::vector<cv::Point>& contour)
{
    TargetInfo info;
    info.area    = cv::contourArea(contour);
    info.box     = cv::boundingRect(contour);      // 轴对齐外接矩形
    info.rotated = cv::minAreaRect(contour);       // 最小旋转矩形

    const cv::Moments m = cv::moments(contour);
    // ★★★【强制】m00 为 0 时不能做除法，必须先判断 ★★★
    if (m.m00 != 0.0) {
        info.center.x = static_cast<float>(m.m10 / m.m00);
        info.center.y = static_cast<float>(m.m01 / m.m00);
        info.hasValidCenter = true;
    } else {
        info.center = info.rotated.center;         // 退化轮廓的兜底
    }
    return info;
}
```

**【强制】** ① 每个保留轮廓**同时**用 `boundingRect` 和 `minAreaRect`；② 用 `moments` 算中心；③ **先检查 `m00 != 0`**，再 `cx=m10/m00`、`cy=m01/m00`。
**为什么必须判空**：退化轮廓（一个点/一条线）的 `m00 = 0`，直接除会得到 `inf`/`nan`，画出来的中心点和框会莫名其妙。
**两种框的用途**：`boundingRect` 边与坐标轴平行；`minAreaRect` 是最小旋转矩形，**目标倾斜时更贴合**（本图的菱形和倾斜方块最能体现差别）。

### 4.8 段 8：标注与保存

```cpp
cv::Mat week6::drawTargets(...)
{
    cv::Mat resultImage = inputImage.clone();       // 【强制】画在副本上，避免改动原图
    const cv::Scalar axisAlignedColor(0, 255, 0);   // BGR：绿色
    const cv::Scalar rotatedColor(0, 165, 255);     // BGR：橙色
    const cv::Scalar centerColor(0, 0, 255);        // BGR：红色

    for (std::size_t i = 0; i < contours.size() && i < infos.size(); ++i) {
        cv::rectangle(resultImage, infos[i].box, axisAlignedColor, 2);   // 轴对齐框

        cv::Point2f corners[4];                                          // 旋转框：四角连线
        infos[i].rotated.points(corners);
        for (int k = 0; k < 4; ++k) {
            cv::line(resultImage,
                     cv::Point(cvRound(corners[k].x), cvRound(corners[k].y)),
                     cv::Point(cvRound(corners[(k + 1) % 4].x), cvRound(corners[(k + 1) % 4].y)),
                     rotatedColor, 2);
        }

        if (infos[i].hasValidCenter) {                                   // 中心点
            cv::circle(resultImage,
                       cv::Point(cvRound(infos[i].center.x), cvRound(infos[i].center.y)),
                       4, centerColor, -1);
        }
    }
    return resultImage;
}
```

**【强制】** ① 画在**原图副本**上；② 两种框用**不同颜色**；③ 保存为 `output/fixed_result.png`。
**颜色坑**：`cv::Scalar` 在 BGR 图上是 **`Scalar(B, G, R)`**！`Scalar(0,255,0)` 才是绿色，写成 `Scalar(255,0,0)` 会变蓝色。

### 4.9 段 9：保存并检查返回值

```cpp
bool week6::saveImageChecked(const std::string& path, const cv::Mat& image)
{
    if (image.empty()) { std::cerr << "[ERROR] 图像为空，无法保存：" << path << "\n"; return false; }
    if (!cv::imwrite(path, image)) {                    // 【强制】检查 imwrite 返回值
        std::cerr << "[ERROR] 保存失败：" << path << "\n";
        std::cerr << "        请确认 output/ 目录存在且可写，扩展名受支持。\n";
        return false;
    }
    std::cout << "[SAVED] " << path << "\n";
    return true;
}
```

```cpp
week6::saveImageChecked(outputDir + "/fixed_blur_gray.png",  blurGray);
week6::saveImageChecked(outputDir + "/fixed_mask_gray.png",  maskGray);
week6::saveImageChecked(outputDir + "/fixed_blur_color.png", blurColor);
week6::saveImageChecked(outputDir + "/fixed_mask_hsv.png",   maskHsv);
week6::saveImageChecked(outputDir + "/fixed_eroded.png",     eroded);
week6::saveImageChecked(outputDir + "/fixed_dilated.png",    dilated);
week6::saveImageChecked(outputDir + "/fixed_opened.png",     opened);
week6::saveImageChecked(outputDir + "/fixed_closed.png",     closed);
week6::saveImageChecked(outputDir + "/fixed_result.png",     resultImage);
```

**【强制】** 9 个文件名**一字不差**；`output/` 必须提前存在（CMake 里 `file(MAKE_DIRECTORY ...)` 已保证）；**必须检查 `imwrite` 返回值**。
**❌ 若整个程序没有 `imshow`**：引导要求"打开两张 mask 后……"、"逐张比较"，所以要**真的看图**，不能只看数字。

### 4.10 段 10：终端输出（任务要求的三项）

```cpp
std::cout << "目标 " << (i + 1)
          << " | 面积=" << info.area
          << " | 中心=(" << info.center.x << ", " << info.center.y << ")"
          << " | 轴对齐框=[" << box.x << ", " << box.y << ", " << box.width << "x" << box.height << "]"
          << " | 旋转框尺寸=" << rot.size.width << "x" << rot.size.height
          << " 角度=" << rot.angle << "\n";
...
std::cout << "final target count: " << infos.size() << "\n";
```

**【强制】** 终端必须输出**每个目标的面积**、**中心点坐标**、**最终目标总数**。

---

## 5. ★ 三条硬性约束对照表（提交前逐条自查）

| # | 约束（任务/引导原文） | 代码里怎么落地 | 违反了会怎样 |
|---|---|---|---|
| **1** | 四种形态学"**不能把它们串成一条连续处理链**" | `morphologyFour()` 里四次调用的第一个实参**都是 `maskIn`** | eroded 与 opened 输出雷同，四张图无法对比，**直接判违规** |
| **2** | 后续轮廓检测**使用开运算结果** `fixed_opened` | `findTargetContours(opened, ...)` | 6 个小绿点变成目标 → **数量 12 ≠ 6** |
| **3** | `moments` 计算中心点，**先检查 `m00` 不为 0** | `if (m.m00 != 0.0) { cx = m10/m00; cy = m01/m00; }` | 退化轮廓除零 → `inf/nan` → 中心点/绘图异常 |
| **4** | **相对路径**，不写依赖设备的绝对路径 | `"images/input.png"`、`"output/..."` | 换机器跑不了，**任务明文禁止** |
| **5** | 两条 mask 路线**用不同变量保存，不互相覆盖** | `maskGray` / `maskHsv`、`blurGray` / `blurColor` 四个独立变量 | 后一张覆盖前一张，两张 mask 无法比较 |
| **6** | 读取失败**提示并结束** | `empty()` 检查 + `return 1` | 空图连锁报错，定位困难 |
| **7** | 面积**小于 100** 必须过滤，且要**输出候选面积** | `if (area < minArea) continue;` + 无条件打印 | 小绿点被计入；无法解释阈值为何是 100 |
| **8** | 两种框用**不同颜色** | 绿 `Scalar(0,255,0)` / 橙 `Scalar(0,165,255)` | 违反任务 4.3 |

---

## 6. 编译与运行（Windows / MSVC 实测命令）

```powershell
# 1) 配置（CMake 与 OpenCV 的实际路径按你本机调整）
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/RM/opencv/build"

# 2) 编译
cmake --build build --config Release

# 3) 运行：必须让 exe 旁边有 images/ 与 output/
cd build\Release
.\week6.exe
```

**运行前确认 `images/input.png` 在 exe 旁边**（CMake POST_BUILD 会自动复制；若手工运行，请直接 `cd` 到 exe 目录）。

**DLL 找不到（缺少 `opencv_world4140.dll` 之类）**：把 OpenCV 的 bin 目录加入 PATH 再运行，例如
`$env:Path = "C:\RM\opencv\build\x64\vc16\bin;$env:Path"`；或把该目录下的 DLL 复制到 exe 旁边。

**【可选】自动化跑（不开窗口）**：本教程代码支持环境变量 `WEEK6_NO_GUI=1` 跳过 `imshow`，便于批量测试与截图前跑通：
```powershell
$env:WEEK6_NO_GUI = "1"
.\week6.exe
```

**Linux / macOS**：
```bash
cmake -S . -B build && cmake --build build -j
cd build && ./week6
```

---

## 7. 编译期 / 运行期常见错误对照表

| 报错或现象 | 原因 | 解决 |
|---|---|---|
| `'threshold': no matching overloaded function` | 参数顺序/类型写错（如把 `maxval` 与 `type` 位置调换） | 对照签名：`threshold(src, dst, thresh, maxval, type)` |
| `medianBlur` 断言失败 / 报 ksize 错 | 第 3 个参数传了 `Size(5,5)` 或偶数 | `medianBlur` 第 3 参是 **`int` 且为 >1 的奇数**：`5` |
| 中文注释导致语法错误（MSVC） | 文件是 UTF-8 但 MSVC 按 GBK 解析 | CMake 里加 `target_compile_options(... /utf-8)` |
| 程序启动即退出，无输出 | exe 旁边没有 `images/input.png` | 用 POST_BUILD 复制，或 `cd` 到正确目录 |
| `[ERROR] Failed to read image` | 路径/扩展名/工作目录不对 | 打印当前工作目录确认；确认文件确实在该相对路径下 |
| `[ERROR] 保存失败` | `output/` 不存在或不可写 | CMake 的 `file(MAKE_DIRECTORY)`；或先手工建目录 |
| 目标数是 12 而不是 6 | 轮廓输入用了 `maskHsv` 而非 `opened` | 改用 `opened`（约束 2） |
| `fixed_eroded.png` 与 `fixed_opened.png` 几乎一样 | 形态学写成链式了 | 四次调用第一个实参都改成 `maskHsv`（约束 1） |
| 中心点是 `nan` 或位置离谱 | 没判 `m00` | 加 `if (m.m00 != 0.0)`（约束 3） |
| 框/点位置整体偏移 | 行列与 x/y 写反，或用了另一张图的坐标 | 点是 `(x,y)`、原点左上角；坐标必须来自同一张图 |
| 结果图没有绿框 | 颜色写成 RGB 顺序 | `cv::Scalar(0,255,0)` 才是绿色 |

---

## 8. 【强制】测试流程（任务第四节，逐条照做）

> **测试纪律**：每项测试前**记录改了什么**，测试后**恢复原值**；全部结束后**用原图原参数再跑一次**。

| # | 测试 | 怎么改 | 期望结果 | 记录什么 |
|---|---|---|---|---|
| ① | 基线 | 不改 | 最终目标数 **6** | 截图终端 + `fixed_result.png` |
| ② | 无目标 | HSV 改 `hsvLower(0,80,80)`、`hsvUpper(10,255,255)` | **正常结束，输出 0**（exit code 0） | 终端"final target count: 0"；**测试后恢复 (35,80,80)/(85,255,255)** |
| ③ | 读取失败 | 把 `inputPath` 改成 `images/input_wrong.png` | **提示并结束** | 终端 `[ERROR] Failed to read image...`；**测试后改回** |
| ④ | 缩半 | 在 `loadImage` 之后对 `image` 做半尺寸 `resize` | 观察**轮廓面积变化**，说明**面积阈值是否需要调整** | 原来 5030~19792 → 缩半后约 1258~4948，**仍远大于 100，阈值不需要改** |
| ⑤ | 恢复 | 去掉缩放与所有临时改动 | 重新生成 `fixed_result.png`，**对应原始图片** | 最终提交的就是这一次的结果图 |

**测试④的代码骨架（【可选·仅测试时用】）**：

```cpp
// 测试④：缩半后重新处理（测完必须删掉这两行）
cv::Mat scaled;
cv::resize(image, scaled, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
image = scaled;
// 注意：此时 printImageInfo 会打印 480x320，属于预期
```

**测试④要写进说明的结论**：面积按 `0.5² = 0.25` 倍缩小，主体面积从约 5030~19792 降到约 1258~4948，**仍然远大于面积阈值 100，所以阈值不需要调整**；但如果继续缩小（如 1/8），小目标就会接近 100 而需要下调阈值。

---

## 9. 【强制】提交内容清单

| 类别 | 内容 |
|---|---|
| 代码 | `week6.hpp`、`week6.cpp`、`main.cpp`、`CMakeLists.txt` |
| 图片 | `images/input.png`（保持原样） |
| 输出（9 个） | `fixed_blur_gray.png`、`fixed_mask_gray.png`、`fixed_blur_color.png`、`fixed_mask_hsv.png`、`fixed_eroded.png`、`fixed_dilated.png`、`fixed_opened.png`、`fixed_closed.png`、`fixed_result.png` |
| 终端输出/截图 | 含"候选面积""每个目标面积与中心点""final target count: 6" |
| 简短说明 | ① **两张 mask 的差别**；② **四种形态学操作的效果**（下面第 10 节给了写法） |

### 9.1 说明文档写作要点（**任务明文要求**）

**两张 mask 的差别**（用实测数字写）：
- 灰度路线用 `120 + THRESH_BINARY_INV`：本图主体灰度约 87、背景约 248、黄色约 204，所以**主体变白、背景与黄色变黑**；白色像素数实测 **67789**。
- HSV 路线用 `(35,80,80)~(85,255,255)`：主体 H=59（区间内）、黄色 H=25（区间外），**靠色相把黄色排除得更彻底**；白色像素数实测 **70180**。
- 差别本质：**灰度靠明暗差，HSV 靠色相**。本图两者都能保留 6 个主体；若换成"与目标亮度相近但颜色不同"的干扰图，灰度路线会误选，HSV 仍能分开。

**四种形态学操作的效果**（用 `countNonZero` 实测）：
- 腐蚀 3×3：白色像素 70180 → **67014**，主体各缩一圈，**6 个小绿点消失**。
- 膨胀 3×3：→ **73442**，主体各胖一圈，小点变粗。
- 开运算 3×3：→ **70152**，**去掉了小绿点而主体基本不变**（与腐蚀的差值说明主体被"长回来"了）→ 因此**后续轮廓用它**。
- 闭运算 7×7：→ **70238**，**主体内部浅色小孔被填补**，面积轻微上升。

---

## 10. 【可选】拓展方向（不影响验收）

| 方向 | 说明 |
|---|---|
| 核形状对比 | 把 `MORPH_RECT` 换成 `MORPH_ELLIPSE` / `MORPH_CROSS`，比较对圆/方形边界的影响 |
| 额外画轮廓 | `cv::drawContours(resultImage, targets, -1, Scalar(255,0,255), 1)` |
| 漏斗式日志 | 同时打印"过滤前/过滤后"数量与"被过滤的最大面积"，一眼看出阈值余量 |
| 旋转框角度 | 输出 `rotated.angle` 与 `size`，观察倾斜方块/菱形与轴对齐目标的差异（注意 4.5+ 版本 `angle` 语义为 `[0,90]`） |
| 方案二 | 自选一张有纹理/阴影/近色干扰的图，重复全流程并独立调参（与方案一**互斥**，做了就不必做方案一） |

---

## 11. 本工程文件索引

| 文件 | 说明 |
|---|---|
| `week6.hpp` | 功能声明（读图、两条路线、形态学、轮廓、测量、绘制、辅助） |
| `week6.cpp` | 上述功能的实现，**三条硬性约束都在这里落地** |
| `main.cpp` | 参数区 + 8 步主流程 + 终端输出 |
| `CMakeLists.txt` | 本周构建配置（含 images/ 自动复制） |

---

## 12. 本机实测验证记录（OpenCV 4.14.0 / CMake 4.4.4 / MSVC，Windows x64）

```
===== 1. Read and check the image =====
width=960 height=640 channels=3

===== 2. Build two masks =====
gray-route mask white pixels: 67789
HSV-route  mask white pixels: 70180

===== 3. Four morphology ops (each takes mask_hsv independently) =====
mask_hsv white pixels: 70180 | eroded: 67014 | dilated: 73442 | opened: 70152 | closed: 70238

===== 4. Contours and area filtering (input: fixed_opened) =====
candidate area: 4
candidate area: 16
candidate area: 5030
candidate area: 10319.5
candidate area: 10509
candidate area: 16
candidate area: 4
candidate area: 4
candidate area: 4
candidate area: 19792
candidate area: 11326
candidate area: 11986
candidate contours (before filter): 12
valid targets (after filter): 6
largest filtered-out area: 16 (area threshold 100, use this to judge whether the threshold is appropriate)

===== 5. Target measurement =====
target 1 | area=5030 | center=(775, 390) | axis-aligned box=[735, 350, 81x81] | rotated box size=80x80 angle=-90
target 2 | area=10319.5 | center=(455.278, 383.435) | axis-aligned box=[410, 330, 121x126] | rotated box size=125x120 angle=-90
target 3 | area=10509 | center=(235, 380) | axis-aligned box=[166, 317, 139x127] | rotated box size=111.789x98.4185 angle=-21.8014
target 4 | area=19792 | center=(390, 150) | axis-aligned box=[300, 95, 181x111] | rotated box size=110x180 angle=-90
target 5 | area=11326 | center=(145, 150) | axis-aligned box=[85, 90, 121x121] | rotated box size=120x120 angle=-90
target 6 | area=11986 | center=(680, 160) | axis-aligned box=[601, 86, 159x149] | rotated box size=113.137x113.137 angle=-45
final target count: 6

===== 6. Annotate and save =====
[SAVED] output/fixed_blur_gray.png   ... （9 个文件全部保存成功）
exit code = 0
```

> **输出语言说明**：所有 `std::cout` / `std::cerr` 的输出字符串已全部改为**英文**，
> 以避免某些 Windows 控制台代码页下中文显示为乱码；**代码注释仍保持中文**（注释不参与运行）。

| 验证项 | 结果 |
|---|---|
| 编译 | ✅ 成功（MSVC，无警告级错误） |
| 读图 | ✅ 960×640×3 |
| candidate contours | ✅ **12**（6 主体 + 6 小绿点） |
| valid targets | ✅ **6**（与任务验收一致） |
| 黄色干扰 | ✅ 全部排除（未被计入） |
| 9 个输出文件 | ✅ 全部生成 |
| 结果图目视 | ✅ 6 个主体各有绿框 + 橙旋转框 + 红中心点；黄色与绿点无标注 |
| 测试②（H=0–10） | ✅ 白色像素 0、候选 0、有效 0、**exit code 0**（正常结束） |
| 测试③（错文件名） | ✅ 打印 `[ERROR] Failed to read image...`，**exit code 1** |
| 恢复原参数重跑 | ✅ 目标数回到 **6** |

> 实测小贴士：本图 6 个小绿点的面积只有 **4~16** 像素平方，与主体的 **5030~19792** 相差三个数量级，
> 所以面积阈值取 100 非常安全（上界余量大，下界离噪点也很远）。
