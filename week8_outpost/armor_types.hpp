#ifndef ARMOR_TYPES_H
#define ARMOR_TYPES_H

// 传统视觉装甲板识别 —— 数据结构与参数
//
// 设计目标：
//   1) 单帧检测的输入输出完全由结构体表达，便于下一阶段逐帧接入视频；
//   2) 所有可调参数集中在一个 config 结构体里，不在函数内部硬编码魔数；
//   3) 灯条、装甲板的字段覆盖任务要求的全部输出项（颜色/框/中心/四角/有效状态）。

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace armor {

// ---------------------------------------------------------------- 颜色类别
enum class color_id : int
{
    red = 0,   // 红色灯条：色相跨 0 附近，必须两段 inRange 再合并（详见 armor_pipeline.cpp）
    blue = 1   // 蓝（同一套流程复用，本帧没有蓝色目标时会得到空 mask）
};

inline const char *color_name(color_id c)
{
    return (c == color_id::blue) ? "blue" : "red";
}

// ---------------------------------------------------------------- 灯条候选
struct light_bar
{
    int id = -1;                 // 候选编号（结果图上前缀，便于人工核对配对）
    color_id color = color_id::red;
    cv::RotatedRect rect;        // 最小外接旋转矩形
    cv::Point2f center{0.f, 0.f};
    float length = 0.f;          // 长边（沿灯条轴向）
    float width = 0.f;           // 短边（灯条粗细）
    float tilt_deg = 0.f;        // 长轴与竖直方向的夹角（度）
    double area = 0.0;           // 轮廓面积（像素平方）
    double fill_ratio = 0.0;     // 轮廓面积 / 旋转矩形面积，用于剔除稀疏噪点

    // 灯条脊柱的两个端点（沿长轴），配对与四角点生成都依赖它
    cv::Point2f end_a;
    cv::Point2f end_b;
};

// ---------------------------------------------------------------- 装甲板候选
struct armor_plate
{
    int color = 0;                            // 颜色类别（color_id）
    cv::Rect bbox;                            // 覆盖装甲板的外接框
    cv::Point2f center{0.f, 0.f};             // 装甲板中心（两根灯条中心的中点）
    cv::Point2f corners[4];                   // 固定顺序：左上→右上→右下→左下
    bool valid = false;                       // 是否通过全部配对与比例检查
    int left_bar_id = -1;                     // 左灯条候选编号
    int right_bar_id = -1;                    // 右灯条候选编号
    double score = 0.0;                       // 几何评分，用于同一块板去重时择优
    float plate_width = 0.f;                  // 两灯条中心距（沿图像 x）
    float plate_height = 0.f;                 // 两灯条平均长度
};

// ---------------------------------------------------------------- 参数
// 所有阈值集中在此，便于按不同赛场环境整体调参；
// 命名统一带前缀（split_/bar_/pair_/dedup_），一眼能看出属于哪一步。
struct armor_config
{
    // ---- 颜色分割（HSV）
    // 目标本质是"红色"灯条，只是暗光环境 + 过曝光晕会让它在视觉上偏橙，
    // 实测灯条框内色相分布为 p5=0.7 / p50=19.7 / p75=28.8 / p95=177.6，
    // 因此第一段取 0~30（覆盖红光与橙色光晕），第二段 155~179（覆盖跨环的那部分红）。
    int red_h_lo_1 = 0;        // 第一段色相下界（红色主段）
    int red_h_hi_1 = 30;       // 第一段色相上界：放宽到 30 以吸收过曝形成的橙色光晕
    int red_h_lo_2 = 155;      // 第二段色相：红色跨 0 环的另一侧
    int red_h_hi_2 = 179;
    int blue_h_lo = 100;
    int blue_h_hi = 130;
    int split_sat_min = 100;   // 饱和度下限：过滤灰白背景
    int split_val_min = 80;    // 亮度下限：过滤暗处噪声
    bool detect_blue = true;   // 本帧即使没有蓝色目标也保留该分支，供后续帧复用

    // ---- 形态学（顺序固定为"先闭后开"，原因见 armor_pipeline.cpp 中的注释）
    // 闭运算核略大：发光灯条的过曝核心饱和度低，mask 内部有暗缝，需要先压实成实心块；
    // 开运算核必须小：仅用于去掉孤立亮点，核太大（如 15x15）会把细灯条打断。
    int morph_close_size = 5;  // 闭运算：先把灯条压实（核半径 2 像素 ≪ 两灯条 68 像素间距）
    int morph_open_size = 3;   // 开运算：再去孤立噪点（核过大将打断细灯条）

    // ---- 灯条筛选
    double bar_min_area = 100.0;     // 面积下限（像素平方）
    double bar_max_area = 20000.0;   // 面积上限：排除大面积光斑
    double bar_min_aspect = 2.0;     // 长宽比下限：灯条是细长的
    double bar_max_aspect = 12.0;    // 长宽比上限
    double bar_min_fill = 0.35;      // 填充比下限：排除镂空/稀疏噪点
    double bar_max_tilt_deg = 35.0;  // 长轴与竖直方向的最大夹角

    // ---- 灯条配对（全部用相对比例，避免依赖固定像素值）
    double pair_len_ratio_lo = 0.60;   // 两根灯条长度比下限
    double pair_len_ratio_hi = 1.70;   // 长度比上限
    double pair_max_dy_ratio = 0.50;   // 中心上下偏差 / 平均灯条长度
    double pair_gap_lo = 1.00;         // 中心水平间距 / 平均灯条长度，下限
    double pair_gap_hi = 4.00;         // 上限
    double pair_max_angle_diff = 12.0; // 两根灯条倾角差（度）

    // ---- 去重
    double dedup_center_ratio = 0.60;  // 两候选中心距 / 较大板宽，低于该值视为同一块板
};

}  // namespace armor

#endif
