#include "frame_output.hpp"

#include "armor_visualize.hpp"

#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

namespace task {

// ---------------------------------------------------------------- CSV
bool csv_writer::open(const std::string &path)
{
    out_.open(std::filesystem::u8path(path.c_str()), std::ios::out | std::ios::trunc);
    if (!out_.is_open())
    {
        std::cerr << "[ERROR] cannot create csv: " << path << "\n";
        return false;
    }
    // 固定经典 locale，避免某些系统把小数点写成逗号
    out_.imbue(std::locale::classic());
    out_ << "frame_id,time_s,candidate_id,detect_valid,pnp_valid,"
            "tx_m,ty_m,tz_m,distance_m,rx_rad,ry_rad,rz_rad,reprojection_error_px\n";
    return true;
}

void csv_writer::write(const csv_row &row)
{
    if (!out_.is_open())
    {
        return;
    }

    out_ << row.frame_id << ",";
    out_ << std::fixed << std::setprecision(6) << row.time_s << ",";
    out_ << row.candidate_id << ",";
    out_ << row.detect_valid << ",";
    out_ << row.pnp_valid << ",";

    if (row.has_pose)
    {
        // 位姿字段：解算成功才写数值
        out_ << std::fixed << std::setprecision(6)
             << row.tx_m << "," << row.ty_m << "," << row.tz_m << ","
             << row.distance_m << ","
             << row.rx_rad << "," << row.ry_rad << "," << row.rz_rad << ",";
        out_ << std::fixed << std::setprecision(4) << row.reproj_err_px;
    }
    else
    {
        // 解算失败：留空（连续 7 个逗号表示 8 个空字段中的 7 个分隔）
        out_ << ",,,,,,,";
    }
    out_ << "\n";
}

void csv_writer::close()
{
    if (out_.is_open())
    {
        out_.flush();
        out_.close();
    }
}

// ---------------------------------------------------------------- 视频
bool video_writer::open(const std::string &base_without_ext, double fps, const cv::Size &size)
{
    if (!(fps > 1.0))
    {
        fps = 30.0;
    }

    // 第一选择：MP4V + .mp4（Windows 上兼容性最好）
    {
        const std::string p = base_without_ext + ".mp4";
        cv::VideoWriter w(p, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, size, true);
        if (w.isOpened())
        {
            writer_ = w;
            path_ = p;
            return true;
        }
    }
    // 第二选择：MJPG + .avi
    {
        const std::string p = base_without_ext + ".avi";
        cv::VideoWriter w(p, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, size, true);
        if (w.isOpened())
        {
            writer_ = w;
            path_ = p;
            return true;
        }
    }
    std::cerr << "[ERROR] cannot open video writer for " << base_without_ext << "\n";
    return false;
}

void video_writer::write(const cv::Mat &frame)
{
    if (writer_.isOpened() && !frame.empty())
    {
        writer_.write(frame);
    }
}

void video_writer::close()
{
    if (writer_.isOpened())
    {
        writer_.release();
    }
}

// ---------------------------------------------------------------- 标注
void draw_pose_overlay(cv::Mat &frame,
                       const armor::armor_plate &plate,
                       const pose_result &pose,
                       int candidate_id,
                       bool show_debug_text)
{
    const cv::Scalar box_color(0, 255, 0);      // 绿：外接框
    const cv::Scalar corner_color(0, 128, 255); // 橙：四角点连线
    const cv::Scalar id_color(0, 255, 255);     // 黄：编号/ID

    // 候选框
    cv::rectangle(frame, plate.bbox, box_color, 2, cv::LINE_AA);

    // 四角点连线 + 编号 0-1-2-3
    for (int k = 0; k < 4; ++k)
    {
        const cv::Point a(cvRound(plate.corners[k].x), cvRound(plate.corners[k].y));
        const cv::Point b(cvRound(plate.corners[(k + 1) % 4].x), cvRound(plate.corners[(k + 1) % 4].y));
        cv::line(frame, a, b, corner_color, 1, cv::LINE_AA);
    }
    for (int k = 0; k < 4; ++k)
    {
        const cv::Point p(cvRound(plate.corners[k].x), cvRound(plate.corners[k].y));
        cv::circle(frame, p, 3, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
        char t[8];
        std::snprintf(t, sizeof(t), "%d", k);
        cv::putText(frame, t, p + cv::Point(3, -3), cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }

    // 候选 ID + PnP 结果 + 直线距离
    std::ostringstream oss;
    oss << "#" << candidate_id;
    if (pose.pnp_ok)
    {
        oss << " PnP:ok";
        if (pose.suspect)
        {
            oss << "!";
        }
        oss << " D=" << std::fixed << std::setprecision(2) << pose.distance_m << "m";
        oss << " tz=" << std::fixed << std::setprecision(2) << pose.tvec[2] << "m";
        oss << " e=" << std::fixed << std::setprecision(2) << pose.reproj_err_px << "px";
    }
    else
    {
        oss << " PnP:fail";
    }

    const cv::Point org(plate.bbox.x, std::max(16, plate.bbox.y - 8));
    cv::putText(frame, oss.str(), org, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                pose.pnp_ok ? id_color : cv::Scalar(0, 0, 255), 1, cv::LINE_AA);

    if (show_debug_text && pose.suspect)
    {
        const cv::Point org2(plate.bbox.x, plate.bbox.y + plate.bbox.height + 16);
        cv::putText(frame, "suspect: " + pose.suspect_reason, org2, cv::FONT_HERSHEY_SIMPLEX,
                    0.42, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
    }
}

// ---------------------------------------------------------------- 重投影验证图
cv::Mat make_reprojection_check(const cv::Mat &frame,
                                const std::vector<cv::Point2f> &corners,
                                const pose_result &pose,
                                int frame_id,
                                int candidate_id)
{
    cv::Mat canvas = frame.clone();
    if (corners.size() < 4 || pose.reprojected.size() < 4)
    {
        return canvas;
    }

    // 原始角点：绿色实心；重投影角点：红色空心；两点之间连线段长度 = 该点误差
    for (int i = 0; i < 4; ++i)
    {
        cv::circle(canvas, cv::Point(cvRound(corners[i].x), cvRound(corners[i].y)), 4,
                   cv::Scalar(0, 255, 0), -1, cv::LINE_AA);
    }
    for (int i = 0; i < 4; ++i)
    {
        const cv::Point a(cvRound(corners[i].x), cvRound(corners[i].y));
        const cv::Point b(cvRound(pose.reprojected[i].x), cvRound(pose.reprojected[i].y));
        cv::circle(canvas, b, 4, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
        cv::line(canvas, a, b, cv::Scalar(255, 0, 255), 1, cv::LINE_AA);
    }

    // 局部放大面板：装甲板只有几十像素，不放大根本无法用肉眼判断"重合得多好"
    cv::Point2f center(0.f, 0.f);
    for (int i = 0; i < 4; ++i)
    {
        center += corners[i];
    }
    center *= 0.25f;

    const int half = 90;                       // 裁 180x180 邻域
    const int zoom = 3;                        // 放大 3 倍 -> 540x540
    cv::Rect roi(cvRound(center.x) - half, cvRound(center.y) - half, half * 2, half * 2);
    roi &= cv::Rect(0, 0, canvas.cols, canvas.rows);
    if (roi.width > 8 && roi.height > 8)
    {
        cv::Mat patch = canvas(roi).clone();
        cv::Mat big;
        cv::resize(patch, big, cv::Size(patch.cols * zoom, patch.rows * zoom), 0, 0,
                   cv::INTER_NEAREST);

        // 把放大图贴在右上角
        const int px = canvas.cols - big.cols - 12;
        const int py = 44;
        if (px > 0 && py + big.rows < canvas.rows)
        {
            big.copyTo(canvas(cv::Rect(px, py, big.cols, big.rows)));
            cv::rectangle(canvas, cv::Rect(px - 2, py - 2, big.cols + 4, big.rows + 4),
                          cv::Scalar(0, 255, 255), 2);
            cv::putText(canvas, "x3 zoom", cv::Point(px, py - 8), cv::FONT_HERSHEY_SIMPLEX,
                        0.6, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
        }
    }

    // 顶部信息栏
    std::ostringstream oss;
    oss << "frame " << frame_id << " cand " << candidate_id
        << " | mean reproj = " << std::fixed << std::setprecision(3) << pose.reproj_err_px
        << " px | green=detected  red=reprojected  tz=" << std::fixed << std::setprecision(3)
        << pose.tvec[2] << " m  dist=" << std::fixed << std::setprecision(3) << pose.distance_m << " m";
    cv::putText(canvas, oss.str(), cv::Point(12, 28), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    cv::putText(canvas, oss.str(), cv::Point(12, 28), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 0, 0), 1, cv::LINE_AA);

    return canvas;
}

}  // namespace task
