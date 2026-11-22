#ifndef FRAME_OUTPUT_H
#define FRAME_OUTPUT_H

// 逐帧输出层：CSV 逐帧数据表、标注视频、重投影验证图、帧内标注绘制
//
// 这一层只负责"写出去"，不含任何检测/解算逻辑，便于后续替换输出格式。

#include "armor_types.hpp"
#include "pose_solver.hpp"

#include <fstream>
#include <opencv2/opencv.hpp>
#include <string>

namespace task {

// ---------------------------------------------------------------- CSV
// 字段严格按任务要求：
// frame_id,time_s,candidate_id,detect_valid,pnp_valid,tx_m,ty_m,tz_m,distance_m,
// rx_rad,ry_rad,rz_rad,reprojection_error_px
struct csv_row
{
    int frame_id = 0;
    double time_s = 0.0;
    int candidate_id = -1;
    int detect_valid = 0;
    int pnp_valid = 0;
    bool has_pose = false;   // false 时位姿相关字段留空（禁止填 0）
    double tx_m = 0.0;
    double ty_m = 0.0;
    double tz_m = 0.0;
    double distance_m = 0.0;
    double rx_rad = 0.0;
    double ry_rad = 0.0;
    double rz_rad = 0.0;
    double reproj_err_px = 0.0;
};

class csv_writer
{
public:
    bool open(const std::string &path);
    void write(const csv_row &row);
    void close();
    bool is_open() const { return out_.is_open(); }

private:
    std::ofstream out_;
};

// ---------------------------------------------------------------- 视频
class video_writer
{
public:
    // 优先 mp4v(.mp4)，失败回退 MJPG(.avi)；成功返回 true 并回填实际路径
    bool open(const std::string &preferred_path_without_ext,
              double fps,
              const cv::Size &size);
    void write(const cv::Mat &frame);
    void close();
    bool is_open() const { return writer_.isOpened(); }
    const std::string &path() const { return path_; }

private:
    cv::VideoWriter writer_;
    std::string path_;
};

// ---------------------------------------------------------------- 标注
// 在帧上绘制候选：外接框、四角点编号 0-1-2-3、候选 ID、PnP 标记、直线距离
void draw_pose_overlay(cv::Mat &frame,
                       const armor::armor_plate &plate,
                       const pose_result &pose,
                       int candidate_id,
                       bool show_debug_text);

// ---------------------------------------------------------------- 重投影验证图
// 左半：原始图上的原始角点（绿）与重投影角点（红）；右半：局部放大对比
cv::Mat make_reprojection_check(const cv::Mat &frame,
                                const std::vector<cv::Point2f> &corners,
                                const pose_result &pose,
                                int frame_id,
                                int candidate_id);

}  // namespace task

#endif
