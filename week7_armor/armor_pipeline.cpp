#include "armor_pipeline.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <iostream>

namespace armor {

namespace {

// 两点的中点
cv::Point2f midpoint(const cv::Point2f &a, const cv::Point2f &b)
{
    return cv::Point2f((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
}

// 把角度归一化到 [0, 90]，便于比较"与竖直方向的夹角"
float clamp_to_vertical_deg(float deg)
{
    float d = std::fabs(deg);
    if (d > 90.0f)
    {
        d = 180.0f - d;
    }
    return d;
}

// 调试开关：设置环境变量 ARMOR_DEBUG=1 时打印每个轮廓被哪条条件过滤
bool debug_on()
{
    static const bool on = (std::getenv("ARMOR_DEBUG") != nullptr);
    return on;
}

}  // namespace

// ---------------------------------------------------------------- 灯条几何
void resolve_bar_geometry(light_bar &bar)
{
    // RotatedRect::points() 给出四个顶点（顺序固定为顺时针）。
    // 直接比较两条邻边长度即可确定哪一对是"短边"：
    //   - 长边方向上的两个端點 = 两条短边的中点；
    //   - 这样得到的是灯条"脊柱"的两个端点，与 OpenCV 版本对 angle 的语义无关。
    cv::Point2f p[4];
    bar.rect.points(p);

    const float e01 = static_cast<float>(cv::norm(p[1] - p[0]));
    const float e12 = static_cast<float>(cv::norm(p[2] - p[1]));

    if (e01 >= e12)
    {
        bar.length = e01;
        bar.width = e12;
        bar.end_a = midpoint(p[1], p[2]);
        bar.end_b = midpoint(p[3], p[0]);
    }
    else
    {
        bar.length = e12;
        bar.width = e01;
        bar.end_a = midpoint(p[0], p[1]);
        bar.end_b = midpoint(p[2], p[3]);
    }

    bar.center = bar.rect.center;

    // 长轴单位向量 → 与竖直方向的夹角
    cv::Point2f axis(bar.end_b.x - bar.end_a.x, bar.end_b.y - bar.end_a.y);
    const float n = static_cast<float>(cv::norm(axis));
    if (n > 1e-6f)
    {
        axis.x /= n;
        axis.y /= n;
    }
    else
    {
        axis = cv::Point2f(0.f, 1.f);
    }
    // 竖直方向为 (0,1)；acos(|axis.y|) 即长轴与竖直方向的夹角
    const float cos_v = std::min(1.0f, std::fabs(axis.y));
    bar.tilt_deg = static_cast<float>(std::acos(cos_v) * 180.0 / CV_PI);
    bar.tilt_deg = clamp_to_vertical_deg(bar.tilt_deg);
}

// ---------------------------------------------------------------- 颜色 mask
cv::Mat build_color_mask(const cv::Mat &hsv, color_id color, const armor_config &cfg)
{
    cv::Mat mask;
    if (hsv.empty() || hsv.type() != CV_8UC3)
    {
        return mask;
    }

    if (color == color_id::red)
    {
        // 目标本质是红色灯条，但暗光 + 过曝会在视觉上偏橙，色相因此被"拉宽"到 0~30；
        // 同时红色本身又跨 0 环（另一侧落在 155~179）。
        // 所以必须两段 inRange 再按位或合并，缺任何一段都会让灯条变得残缺、进而被面积阈值滤掉。
        cv::Mat m1;
        cv::Mat m2;
        cv::inRange(hsv,
                    cv::Scalar(cfg.red_h_lo_1, cfg.split_sat_min, cfg.split_val_min),
                    cv::Scalar(cfg.red_h_hi_1, 255, 255),
                    m1);
        cv::inRange(hsv,
                    cv::Scalar(cfg.red_h_lo_2, cfg.split_sat_min, cfg.split_val_min),
                    cv::Scalar(cfg.red_h_hi_2, 255, 255),
                    m2);
        cv::bitwise_or(m1, m2, mask);
    }
    else
    {
        cv::inRange(hsv,
                    cv::Scalar(cfg.blue_h_lo, cfg.split_sat_min, cfg.split_val_min),
                    cv::Scalar(cfg.blue_h_hi, 255, 255),
                    mask);
    }
    return mask;
}

// ---------------------------------------------------------------- 灯条候选
std::vector<light_bar> extract_light_bars(const cv::Mat &mask,
                                          color_id color,
                                          const armor_config &cfg,
                                          int id_start)
{
    std::vector<light_bar> bars;
    if (mask.empty())
    {
        return bars;
    }

    // findContours 会改写入参，传副本
    cv::Mat work = mask.clone();
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(work, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    int next_id = id_start;
    for (const auto &contour : contours)
    {
        const double area = cv::contourArea(contour);
        const cv::RotatedRect rect = cv::minAreaRect(contour);
        const float rw = rect.size.width;
        const float rh = rect.size.height;
        const float long_side = std::max(rw, rh);
        const float short_side = std::min(rw, rh);
        const double aspect = (short_side > 1e-6f) ? (long_side / static_cast<double>(short_side)) : 0.0;
        const double rect_area = static_cast<double>(rw) * static_cast<double>(rh);
        const double fill = (rect_area > 1e-6) ? (area / rect_area) : 0.0;

        if (area < cfg.bar_min_area || area > cfg.bar_max_area)
        {
            if (debug_on())
            {
                std::cout << "  [skip area] area=" << area
                          << " at (" << rect.center.x << "," << rect.center.y << ")\n";
            }
            continue;
        }
        if (rw < 1.0f || rh < 1.0f)
        {
            continue;
        }
        if (aspect < cfg.bar_min_aspect || aspect > cfg.bar_max_aspect)
        {
            if (debug_on())
            {
                std::cout << "  [skip aspect] area=" << area << " aspect=" << aspect
                          << " at (" << rect.center.x << "," << rect.center.y << ")\n";
            }
            continue;
        }

        // 填充比：细长但中空的轮廓（反光边、碎斑）在这里被剔除
        if (fill < cfg.bar_min_fill)
        {
            if (debug_on())
            {
                std::cout << "  [skip fill] area=" << area << " fill=" << fill
                          << " at (" << rect.center.x << "," << rect.center.y << ")\n";
            }
            continue;
        }

        light_bar bar;
        bar.id = next_id;
        bar.color = color;
        bar.rect = rect;
        bar.area = area;
        bar.fill_ratio = fill;
        resolve_bar_geometry(bar);

        if (bar.tilt_deg > cfg.bar_max_tilt_deg)
        {
            if (debug_on())
            {
                std::cout << "  [skip tilt] tilt=" << bar.tilt_deg
                          << " at (" << bar.center.x << "," << bar.center.y << ")\n";
            }
            continue;
        }

        if (debug_on())
        {
            std::cout << "  [keep] area=" << area << " aspect=" << aspect
                      << " fill=" << fill << " tilt=" << bar.tilt_deg
                      << " center=(" << bar.center.x << "," << bar.center.y << ")\n";
        }

        next_id++;
        bars.push_back(bar);
    }
    return bars;
}

// ---------------------------------------------------------------- 装甲板构造
bool build_plate(const light_bar &left,
                 const light_bar &right,
                 const armor_config &cfg,
                 armor_plate &out)
{
    (void)cfg;
    out = armor_plate();
    out.color = static_cast<int>(left.color);

    // 每根灯条各取"上端"和"下端"：图像坐标 y 越小越靠上
    const bool left_flip = left.end_a.y > left.end_b.y;
    const cv::Point2f left_top = left_flip ? left.end_b : left.end_a;
    const cv::Point2f left_bottom = left_flip ? left.end_a : left.end_b;

    const bool right_flip = right.end_a.y > right.end_b.y;
    const cv::Point2f right_top = right_flip ? right.end_b : right.end_a;
    const cv::Point2f right_bottom = right_flip ? right.end_a : right.end_b;

    // 固定顺序：左上 → 右上 → 右下 → 左下
    out.corners[0] = left_top;
    out.corners[1] = right_top;
    out.corners[2] = right_bottom;
    out.corners[3] = left_bottom;

    out.center = midpoint(left.center, right.center);
    out.left_bar_id = left.id;
    out.right_bar_id = right.id;
    out.plate_width = std::fabs(right.center.x - left.center.x);
    out.plate_height = (left.length + right.length) * 0.5f;

    // 用四个角点求轴对齐外接框
    std::vector<cv::Point> pts;
    for (int i = 0; i < 4; ++i)
    {
        pts.push_back(cv::Point(cvRound(out.corners[i].x), cvRound(out.corners[i].y)));
    }
    out.bbox = cv::boundingRect(pts);

    // 退化检查：四角点共线或框为空时判为无效
    const bool ok = out.bbox.width > 2 && out.bbox.height > 2;
    out.valid = ok;
    return ok;
}

// ---------------------------------------------------------------- 配对
std::vector<armor_plate> pair_bars(const std::vector<light_bar> &bars,
                                   const armor_config &cfg)
{
    std::vector<armor_plate> plates;
    const int n = static_cast<int>(bars.size());

    for (int i = 0; i < n; ++i)
    {
        for (int j = i + 1; j < n; ++j)
        {
            const light_bar &a = bars[i];
            const light_bar &b = bars[j];

            // 条件 1：颜色必须一致（后续多色场景靠这一条隔开红蓝两组）
            if (a.color != b.color)
            {
                continue;
            }

            // 条件 2：长度相近
            const double len_ratio = static_cast<double>(std::min(a.length, b.length)) /
                                     static_cast<double>(std::max(a.length, b.length));
            if (len_ratio < cfg.pair_len_ratio_lo || len_ratio > cfg.pair_len_ratio_hi)
            {
                continue;
            }

            // 条件 3：倾角协调
            if (std::fabs(a.tilt_deg - b.tilt_deg) > cfg.pair_max_angle_diff)
            {
                continue;
            }

            // 条件 4/5：中心上下偏差与水平间距都要相对"灯条长度"来衡量
            const float avg_len = (a.length + b.length) * 0.5f;
            if (avg_len < 1.0f)
            {
                continue;
            }
            const float dy = std::fabs(a.center.y - b.center.y);
            const float dx = std::fabs(a.center.x - b.center.x);
            const double dy_ratio = dy / avg_len;
            const double gap_ratio = dx / avg_len;

            if (dy_ratio > cfg.pair_max_dy_ratio)
            {
                continue;
            }
            if (gap_ratio < cfg.pair_gap_lo || gap_ratio > cfg.pair_gap_hi)
            {
                continue;
            }

            // 两根灯条不能重叠：水平间距要大于灯条宽度
            const float max_width = std::max(a.width, b.width);
            if (dx < max_width * 1.2f)
            {
                continue;
            }

            // 按 x 决定左右
            const light_bar &left = (a.center.x <= b.center.x) ? a : b;
            const light_bar &right = (a.center.x <= b.center.x) ? b : a;

            armor_plate plate;
            if (!build_plate(left, right, cfg, plate))
            {
                continue;
            }

            // 评分：间距比例越接近 2.0、上下偏差越小，越像一块正面装甲板
            plate.score = -(std::fabs(gap_ratio - 2.0) + 2.0 * dy_ratio);
            plates.push_back(plate);
        }
    }

    // 先按评分从优到劣排序，便于后续去重时"保留最优"
    std::sort(plates.begin(), plates.end(),
              [](const armor_plate &x, const armor_plate &y) { return x.score > y.score; });
    return plates;
}

// ---------------------------------------------------------------- 去重
void dedup_plates(std::vector<armor_plate> &plates, const armor_config &cfg)
{
    std::vector<bool> removed(plates.size(), false);

    for (std::size_t i = 0; i < plates.size(); ++i)
    {
        if (removed[i])
        {
            continue;
        }
        for (std::size_t j = i + 1; j < plates.size(); ++j)
        {
            if (removed[j])
            {
                continue;
            }
            const float d = static_cast<float>(cv::norm(plates[i].center - plates[j].center));
            const float w = std::max(plates[i].plate_width, plates[j].plate_width);
            if (w > 1.0f && d < cfg.dedup_center_ratio * w)
            {
                // j 的评分不可能高于 i（已排序），直接抑制 j
                removed[j] = true;
            }
        }
    }

    std::vector<armor_plate> kept;
    for (std::size_t i = 0; i < plates.size(); ++i)
    {
        if (!removed[i])
        {
            kept.push_back(plates[i]);
        }
    }
    plates.swap(kept);
}

// ---------------------------------------------------------------- 单帧入口
frame_result detect_frame(const cv::Mat &bgr, const armor_config &cfg)
{
    frame_result result;
    if (bgr.empty() || bgr.type() != CV_8UC3)
    {
        return result;
    }

    const int64 t0 = cv::getTickCount();

    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

    std::vector<color_id> colors;
    colors.push_back(color_id::red);
    if (cfg.detect_blue)
    {
        colors.push_back(color_id::blue);
    }

    std::vector<cv::Mat> masks;
    for (color_id c : colors)
    {
        // 清理顺序固定为"先闭运算、后开运算"，这个顺序是实测出来的，不是随手写的：
        //   本帧灯条本体只有约 12x35 像素，而且过曝核心饱和度低，mask 内部留有暗缝；
        //   若先做 3x3 开运算，细灯条会被切成两块（实测 224px 的灯条被拆成 98px + 36px，
        //   两块都低于面积阈值，导致最终 0 个候选）。
        //   反过来先做 5x5 闭运算，把灯条压实成 336px 的实心连通域，再用 3x3 开运算
        //   去掉孤立亮点，灯条保持完整。
        //   两处安全性约束：
        //     * 闭运算核半径 2 像素，远小于两根灯条之间 68 像素的间距，不会把左右灯条粘连；
        //     * 开运算核保持 3x3，不会把 11~12 像素宽的灯条打断。
        cv::Mat raw = build_color_mask(hsv, c, cfg);
        cv::Mat closed;
        cv::Mat cleaned;
        const cv::Mat k_close = cv::getStructuringElement(
            cv::MORPH_RECT, cv::Size(cfg.morph_close_size, cfg.morph_close_size));
        const cv::Mat k_open = cv::getStructuringElement(
            cv::MORPH_RECT, cv::Size(cfg.morph_open_size, cfg.morph_open_size));
        cv::morphologyEx(raw, closed, cv::MORPH_CLOSE, k_close);
        cv::morphologyEx(closed, cleaned, cv::MORPH_OPEN, k_open);
        masks.push_back(cleaned);
        result.mask_colors.push_back(c);
    }
    result.split_ms = (cv::getTickCount() - t0) * 1000.0 / cv::getTickFrequency();

    const int64 t1 = cv::getTickCount();
    int id_cursor = 0;
    for (std::size_t i = 0; i < masks.size(); ++i)
    {
        const std::vector<light_bar> bars =
            extract_light_bars(masks[i], result.mask_colors[i], cfg, id_cursor);
        id_cursor += static_cast<int>(bars.size());
        for (const auto &b : bars)
        {
            result.bars.push_back(b);
        }
    }
    result.masks = masks;
    result.bar_ms = (cv::getTickCount() - t1) * 1000.0 / cv::getTickFrequency();

    const int64 t2 = cv::getTickCount();
    result.plates = pair_bars(result.bars, cfg);
    dedup_plates(result.plates, cfg);
    result.pair_ms = (cv::getTickCount() - t2) * 1000.0 / cv::getTickFrequency();

    return result;
}

}  // namespace armor
