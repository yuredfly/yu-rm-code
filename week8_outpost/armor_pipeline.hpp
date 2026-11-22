#ifndef ARMOR_PIPELINE_H
#define ARMOR_PIPELINE_H

// 单帧检测流水线：颜色 mask → 灯条候选 → 配对 → 装甲板 + 去重
//
// 本文件只负责"算"，不负责"画"，也不负责读写文件。
// 这样下一阶段接视频时，只需循环调用 detect_frame() 即可。

#include "armor_types.hpp"

#include <opencv2/opencv.hpp>
#include <vector>

namespace armor {

// 单帧检测的完整输出（含调试用的中间 mask）
struct frame_result
{
    std::vector<cv::Mat> masks;        // 与 mask_colors 一一对应
    std::vector<color_id> mask_colors;
    std::vector<light_bar> bars;       // 通过筛选的灯条候选（全部颜色）
    std::vector<armor_plate> plates;   // 通过配对与去重的装甲板候选
    double split_ms = 0.0;             // 各阶段耗时，便于后续做实时性评估
    double bar_ms = 0.0;
    double pair_ms = 0.0;
};

// 对一张 BGR 图像做一次完整检测（不依赖任何全局状态）
frame_result detect_frame(const cv::Mat &bgr, const armor_config &cfg);

// ---- 以下函数暴露出来是为了单独调试某一步 ----

// 按给定颜色类别生成 mask（红/橙用两段色相合并）
cv::Mat build_color_mask(const cv::Mat &hsv, color_id color, const armor_config &cfg);

// 从单色 mask 中提取灯条候选
std::vector<light_bar> extract_light_bars(const cv::Mat &mask,
                                          color_id color,
                                          const armor_config &cfg,
                                          int id_start);

// 由左、右两根灯条构造装甲板候选（含四角点排序）
bool build_plate(const light_bar &left,
                 const light_bar &right,
                 const armor_config &cfg,
                 armor_plate &out);

// 两两配对 + 整体比例检查
std::vector<armor_plate> pair_bars(const std::vector<light_bar> &bars,
                                   const armor_config &cfg);

// 同一块装甲板被重复输出时只保留最优的一个
void dedup_plates(std::vector<armor_plate> &plates, const armor_config &cfg);

// 由旋转矩形求灯条脊柱端点（沿长轴），并回填 length/width/tilt
void resolve_bar_geometry(light_bar &bar);

}  // namespace armor

#endif
