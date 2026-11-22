#ifndef TASK_CONFIG_H
#define TASK_CONFIG_H

// 本阶段任务的固定常量与兜底值
//
// 说明：
//   * 装甲板物理尺寸来自《参数.md》第二节，这里只是"参数文件解析失败时的兜底默认值"；
//   * fps / 分辨率兜底同样来自《参数.md》，用于视频元数据读取失败时仍能打印合理信息；
//   * 循环帧数一律不用常量：主循环是 while(cap.read(frame))，读到文件尾自然结束。

namespace task {

// ---- 装甲板物理尺寸（米）
constexpr double kArmorWidthM = 0.135;    // 模型宽度 w
constexpr double kArmorHeightM = 0.056;   // 模型高度 h

// ---- 视频元数据兜底（参数.md 第三节/第一节给定的图像尺寸）
constexpr int kFallbackWidth = 1280;
constexpr int kFallbackHeight = 1024;
constexpr double kFallbackFps = 30.0;     // 参数.md 未给帧率，这里仅作打印与写视频兜底

// ---- 校验阈值（第六章"校验判定规则"）
constexpr double kReprojErrThresholdPx = 3.0;    // 平均重投影误差上限
constexpr double kMaxReasonableDistanceM = 20.0; // 直线距离上限（超出视为可疑）
constexpr double kDepthMismatchRatio = 0.35;     // 粗深度与 tz 的相对偏差上限

// ---- 默认路径（相对工程根目录）
constexpr const char *kDefaultVideo = "outpost.avi";
constexpr const char *kDefaultParams = "参数.md";
constexpr const char *kDefaultImage = "images/outpost_frame.png";
constexpr const char *kOutputDir = "output";

}  // namespace task

#endif
