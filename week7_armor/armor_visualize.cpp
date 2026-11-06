#include "armor_visualize.hpp"

#include <cstdio>
#include <filesystem>
#include <iostream>

namespace armor {

namespace {

// 不同颜色的可视化配色（BGR 顺序！）
cv::Scalar bar_color_of(color_id c, bool bright)
{
    if (c == color_id::blue)
    {
        return bright ? cv::Scalar(255, 200, 0) : cv::Scalar(180, 120, 0);
    }
    return bright ? cv::Scalar(0, 200, 255) : cv::Scalar(0, 120, 200);  // 橙黄
}

}  // namespace

// ---------------------------------------------------------------- 灯条候选图
cv::Mat draw_candidates(const cv::Mat &bgr, const std::vector<light_bar> &bars)
{
    cv::Mat canvas = bgr.clone();   // 画在副本上，原图保持干净

    for (const auto &bar : bars)
    {
        const cv::Scalar color = bar_color_of(bar.color, true);

        // 旋转矩形的四个顶点
        cv::Point2f p[4];
        bar.rect.points(p);
        for (int i = 0; i < 4; ++i)
        {
            const cv::Point a(cvRound(p[i].x), cvRound(p[i].y));
            const cv::Point b(cvRound(p[(i + 1) % 4].x), cvRound(p[(i + 1) % 4].y));
            cv::line(canvas, a, b, color, 1, cv::LINE_AA);
        }

        // 中心点与脊柱（长轴）方向
        cv::circle(canvas, cv::Point(cvRound(bar.center.x), cvRound(bar.center.y)), 2,
                   cv::Scalar(0, 0, 255), -1);
        cv::line(canvas,
                 cv::Point(cvRound(bar.end_a.x), cvRound(bar.end_a.y)),
                 cv::Point(cvRound(bar.end_b.x), cvRound(bar.end_b.y)),
                 cv::Scalar(255, 0, 255), 1, cv::LINE_AA);

        // 候选编号
        char text[32];
        std::snprintf(text, sizeof(text), "#%d", bar.id);
        cv::putText(canvas, text,
                    cv::Point(cvRound(bar.center.x) + 4, cvRound(bar.center.y) - 4),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
    }
    return canvas;
}

// ---------------------------------------------------------------- 最终结果图
cv::Mat draw_plates(const cv::Mat &bgr,
                    const std::vector<light_bar> &bars,
                    const std::vector<armor_plate> &plates)
{
    cv::Mat canvas = draw_candidates(bgr, bars);

    for (std::size_t i = 0; i < plates.size(); ++i)
    {
        const armor_plate &plate = plates[i];
        const cv::Scalar main_color =
            (plate.color == static_cast<int>(color_id::blue)) ? cv::Scalar(255, 255, 0)
                                                             : cv::Scalar(0, 255, 0);

        // 外接框
        cv::rectangle(canvas, plate.bbox, main_color, 2, cv::LINE_AA);

        // 四角点连成的四边形
        for (int k = 0; k < 4; ++k)
        {
            const cv::Point a(cvRound(plate.corners[k].x), cvRound(plate.corners[k].y));
            const cv::Point b(cvRound(plate.corners[(k + 1) % 4].x),
                              cvRound(plate.corners[(k + 1) % 4].y));
            cv::line(canvas, a, b, cv::Scalar(0, 128, 255), 1, cv::LINE_AA);
        }

        // 四角点编号 0~3（用于人工核对"左上→右上→右下→左下"）
        for (int k = 0; k < 4; ++k)
        {
            const cv::Point pt(cvRound(plate.corners[k].x), cvRound(plate.corners[k].y));
            cv::circle(canvas, pt, 3, cv::Scalar(0, 0, 255), -1);
            char label[8];
            std::snprintf(label, sizeof(label), "%d", k);
            cv::putText(canvas, label, pt + cv::Point(3, -3),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1,
                        cv::LINE_AA);
        }

        // 中心点
        const cv::Point center(cvRound(plate.center.x), cvRound(plate.center.y));
        cv::drawMarker(canvas, center, cv::Scalar(255, 0, 255), cv::MARKER_CROSS, 9, 1);

        // 结果摘要
        char summary[64];
        std::snprintf(summary, sizeof(summary), "P%zu %s", i,
                      plate.valid ? "valid" : "invalid");
        cv::putText(canvas, summary, cv::Point(plate.bbox.x, plate.bbox.y - 6),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, main_color, 1, cv::LINE_AA);
    }
    return canvas;
}

// ---------------------------------------------------------------- I/O
bool ensure_directory(const std::string &dir)
{
    if (dir.empty())
    {
        return false;
    }
    std::error_code ec;
    if (std::filesystem::exists(dir))
    {
        return true;
    }
    std::filesystem::create_directories(dir, ec);
    if (ec)
    {
        std::cerr << "[ERROR] Failed to create directory: " << dir
                  << " (" << ec.message() << ")\n";
        return false;
    }
    std::cout << "[MKDIR] " << dir << "\n";
    return true;
}

bool save_image(const std::string &path, const cv::Mat &image)
{
    if (image.empty())
    {
        std::cerr << "[ERROR] Image is empty, cannot save: " << path << "\n";
        return false;
    }
    if (!cv::imwrite(path, image))
    {
        std::cerr << "[ERROR] Failed to save: " << path << "\n";
        std::cerr << "        Check that the output directory exists and is writable.\n";
        return false;
    }
    std::cout << "[SAVED] " << path << "\n";
    return true;
}

}  // namespace armor
