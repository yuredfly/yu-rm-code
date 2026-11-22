// 前哨站视频 → 装甲板检测 + PnP 位姿解算（静态单帧检测模块复用，接口不改）
//
// 用法：
//   week8_outpost                                  # 处理 outpost.avi（默认）
//   week8_outpost --video outpost.avi              # 显式指定视频
//   week8_outpost --image images/outpost_frame.png # 静态图入口（保留，链路与上周一致）
//   week8_outpost --params 参数.md --outdir output # 指定参数文件与输出目录
//
// 主循环使用 while(cap.read(frame))，帧数不写死。

#include "armor_pipeline.hpp"
#include "armor_types.hpp"
#include "armor_visualize.hpp"
#include "frame_output.hpp"
#include "param_loader.hpp"
#include "pose_solver.hpp"
#include "task_config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

// 打印图片尺寸信息
void print_image_info(const cv::Mat &img)
{
    std::cout << "width=" << img.cols << " height=" << img.rows
              << " channels=" << img.channels() << "\n";
}

// ---------------------------------------------------------------- 静态图片入口（保留）
int run_static_image(const std::string &image_path, const armor::armor_config &cfg)
{
    std::cout << "===== static image mode =====\n";
    std::cout << "input: " << image_path << "\n";

    cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);
    if (image.empty())
    {
        std::cerr << "[ERROR] Failed to read image: " << image_path << "\n";
        return 1;
    }
    print_image_info(image);

    const armor::frame_result result = armor::detect_frame(image, cfg);
    std::cout << "masks: " << result.masks.size() << ", light bars: " << result.bars.size()
              << ", plates: " << result.plates.size() << "\n";

    if (!armor::ensure_directory(task::kOutputDir))
    {
        return 1;
    }
    armor::save_image(std::string(task::kOutputDir) + "/static_armor_result.png",
                      armor::draw_plates(image, result.bars, result.plates));
    return 0;
}

// 从视频流读取元数据，读不到时用《参数.md》的参考值兜底
struct video_meta
{
    int width = 0;
    int height = 0;
    double fps = 0.0;
    double frame_count = 0.0;
    bool width_fb = false;
    bool height_fb = false;
    bool fps_fb = false;
    bool count_fb = false;
};

video_meta read_video_meta(cv::VideoCapture &cap, const task::camera_params &cam)
{
    video_meta m;

    m.width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    if (m.width <= 0)
    {
        m.width = cam.image_width > 0 ? cam.image_width : task::kFallbackWidth;
        m.width_fb = true;
    }

    m.height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    if (m.height <= 0)
    {
        m.height = cam.image_height > 0 ? cam.image_height : task::kFallbackHeight;
        m.height_fb = true;
    }

    m.fps = cap.get(cv::CAP_PROP_FPS);
    if (!(m.fps > 1.0) || !std::isfinite(m.fps))
    {
        m.fps = task::kFallbackFps;
        m.fps_fb = true;
    }

    m.frame_count = cap.get(cv::CAP_PROP_FRAME_COUNT);
    if (!(m.frame_count > 0.0) || !std::isfinite(m.frame_count))
    {
        // 只用于打印与估算时长；绝不用于控制循环次数
        m.frame_count = 0.0;
        m.count_fb = true;
    }
    return m;
}

}  // namespace

int main(int argc, char **argv)
{
    // ---------------------------------------------------------------- 命令行
    std::string video_path = task::kDefaultVideo;
    std::string params_path = task::kDefaultParams;
    std::string image_path;
    std::string out_dir = task::kOutputDir;
    bool image_mode = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&](const char *name) -> std::string {
            if (i + 1 >= argc)
            {
                std::cerr << "[WARN] missing value for " << name << "\n";
                return std::string();
            }
            return std::string(argv[++i]);
        };
        if (a == "--video")
        {
            video_path = next("--video");
        }
        else if (a == "--params")
        {
            params_path = next("--params");
        }
        else if (a == "--image")
        {
            image_path = next("--image");
            image_mode = true;
        }
        else if (a == "--outdir")
        {
            out_dir = next("--outdir");
        }
        else
        {
            std::cout << "[WARN] unknown arg: " << a << "\n";
        }
    }

    // ---------------------------------------------------------------- 参数文件
    std::cout << "===== 1. load camera parameters =====\n";
    const task::camera_params cam = task::load_camera_params(params_path);
    task::print_camera_params(cam);

    // 装甲板检测参数：沿用第 7 周静态检测的同一套配置（暗光 + 红光场景调好的值）
    armor::armor_config cfg;
    cfg.split_sat_min = 100;
    cfg.split_val_min = 80;
    cfg.bar_min_area = 100.0;

    // 静态图片入口（保留，不改 detect_frame）
    if (image_mode)
    {
        return run_static_image(image_path, cfg);
    }

    // ---------------------------------------------------------------- 视频元数据
    std::cout << "\n===== 2. open video =====\n";
    std::cout << "input: " << video_path << "\n";

    cv::VideoCapture cap(video_path, cv::CAP_FFMPEG);
    if (!cap.isOpened())
    {
        cap.open(video_path);   // 换默认后端再试一次
    }
    if (!cap.isOpened())
    {
        std::cerr << "[ERROR] Failed to open video: " << video_path << "\n";
        std::cerr << "        Fallback metadata from 参数.md would be: "
                  << cam.image_width << " x " << cam.image_height
                  << " @ " << task::kFallbackFps << " fps\n";
        return 1;
    }

    const video_meta meta = read_video_meta(cap, cam);
    std::cout << "video width  = " << meta.width << (meta.width_fb ? "  (fallback)" : "") << "\n";
    std::cout << "video height = " << meta.height << (meta.height_fb ? "  (fallback)" : "") << "\n";
    std::cout << "video fps    = " << meta.fps << (meta.fps_fb ? "  (fallback)" : "") << "\n";
    if (meta.count_fb)
    {
        std::cout << "video frames = unknown (will read until EOF)\n";
    }
    else
    {
        std::cout << "video frames = " << meta.frame_count << " (metadata only; loop reads until EOF)\n";
        std::cout << "video duration ~ " << (meta.frame_count / meta.fps) << " s\n";
    }

    if (!armor::ensure_directory(out_dir))
    {
        return 1;
    }

    // ---------------------------------------------------------------- 输出
    std::cout << "\n===== 3. process frames =====\n";
    task::csv_writer csv;
    if (!csv.open(out_dir + "/pnp_frames.csv"))
    {
        return 1;
    }

    task::video_writer vwriter;
    if (!vwriter.open(out_dir + "/outpost_annotated", meta.fps,
                      cv::Size(meta.width, meta.height)))
    {
        csv.close();
        return 1;
    }
    std::cout << "annotated video -> " << vwriter.path() << "\n";

    // ---------------------------------------------------------------- 主循环
    cv::Mat frame;
    long frame_id = 0;
    long frames_read = 0;
    long frames_with_detect = 0;
    long candidates_total = 0;
    long pnp_ok_total = 0;
    long pnp_valid_total = 0;
    long suspect_total = 0;
    double sum_reproj = 0.0;
    double min_dist = 1e9;
    double max_dist = -1e9;
    double min_tz = 1e9;
    double max_tz = -1e9;

    // 用于重投影验证图：保存前若干帧 + 重投影误差最小的一帧
    int saved_reproj_count = 0;
    double best_reproj = 1e9;
    cv::Mat best_frame_for_reproj;
    std::vector<cv::Point2f> best_corners;
    task::pose_result best_pose;
    int best_frame_id = -1;
    int best_candidate_id = -1;

    while (cap.read(frame))   // 读到文件尾自然结束，不写死帧数
    {
        ++frames_read;

        // 空帧：按异常处理，记录一行但 pnp_valid=0，且绝不复用上一帧位姿
        if (frame.empty())
        {
            task::csv_row row;
            row.frame_id = static_cast<int>(frame_id);
            row.time_s = static_cast<double>(frame_id) / meta.fps;
            row.candidate_id = -1;
            row.detect_valid = 0;
            row.pnp_valid = 0;
            row.has_pose = false;
            csv.write(row);
            ++frame_id;
            continue;
        }

        const double t_s = static_cast<double>(frame_id) / meta.fps;

        // 复用第 7 周的静态单帧检测（接口未改动）
        const armor::frame_result det = armor::detect_frame(frame, cfg);

        cv::Mat annotated = frame.clone();

        if (det.plates.empty())
        {
            // 无候选帧也要记录一行（位姿字段留空）
            task::csv_row row;
            row.frame_id = static_cast<int>(frame_id);
            row.time_s = t_s;
            row.candidate_id = -1;
            row.detect_valid = 0;
            row.pnp_valid = 0;
            row.has_pose = false;
            csv.write(row);
        }
        else
        {
            ++frames_with_detect;
            for (std::size_t i = 0; i < det.plates.size(); ++i)
            {
                const armor::armor_plate &plate = det.plates[i];
                ++candidates_total;

                // 装甲板四角点 -> 位姿（顺序由 detect_frame 保证：左上/右上/右下/左下）
                std::vector<cv::Point2f> corners(plate.corners, plate.corners + 4);
                const task::pose_result pose =
                    task::solve_pose(corners, cam, cam.armor_width_m, cam.armor_height_m,
                                     task::kReprojErrThresholdPx);

                task::csv_row row;
                row.frame_id = static_cast<int>(frame_id);
                row.time_s = t_s;
                row.candidate_id = static_cast<int>(i);
                row.detect_valid = plate.valid ? 1 : 0;

                if (pose.pnp_ok && pose.finite_values)
                {
                    row.pnp_valid = 1;
                    row.has_pose = true;
                    row.tx_m = pose.tvec[0];
                    row.ty_m = pose.tvec[1];
                    row.tz_m = pose.tvec[2];
                    row.distance_m = pose.distance_m;
                    row.rx_rad = pose.rx_rad;
                    row.ry_rad = pose.ry_rad;
                    row.rz_rad = pose.rz_rad;
                    row.reproj_err_px = pose.reproj_err_px;

                    ++pnp_ok_total;
                    if (!pose.suspect)
                    {
                        ++pnp_valid_total;
                        sum_reproj += pose.reproj_err_px;
                        min_dist = std::min(min_dist, pose.distance_m);
                        max_dist = std::max(max_dist, pose.distance_m);
                        min_tz = std::min(min_tz, pose.tvec[2]);
                        max_tz = std::max(max_tz, pose.tvec[2]);
                    }
                    else
                    {
                        ++suspect_total;
                    }

                    // 记录重投影误差最小的一帧，用于最终的重投影验证图
                    if (!pose.suspect && pose.reproj_err_px < best_reproj)
                    {
                        best_reproj = pose.reproj_err_px;
                        best_frame_for_reproj = frame.clone();
                        best_corners = corners;
                        best_pose = pose;
                        best_frame_id = static_cast<int>(frame_id);
                        best_candidate_id = static_cast<int>(i);
                    }

                    // 前 3 个成功帧各存一张重投影验证图
                    if (saved_reproj_count < 3)
                    {
                        char name[96];
                        std::snprintf(name, sizeof(name),
                                      "%s/reproj_check_f%04d_c%d.png", out_dir.c_str(),
                                      static_cast<int>(frame_id), static_cast<int>(i));
                        cv::imwrite(name, task::make_reprojection_check(
                                              frame, corners, pose,
                                              static_cast<int>(frame_id),
                                              static_cast<int>(i)));
                        std::cout << "[SAVED] " << name << "\n";
                        ++saved_reproj_count;
                    }
                }
                // pnp_ok 为假时 has_pose 保持 false -> CSV 位姿字段留空

                csv.write(row);

                task::draw_pose_overlay(annotated, plate, pose, static_cast<int>(i), false);
            }
        }

        vwriter.write(annotated);
        ++frame_id;

        // 进度打印（每 500 帧，避免刷屏；属于工程优化的可观测性做法）
        if (frame_id % 500 == 0)
        {
            std::cout << "  processed " << frame_id << " frames, candidates so far "
                      << candidates_total << "\n";
        }
    }

    cap.release();
    vwriter.close();
    csv.close();

    // ---------------------------------------------------------------- 汇总
    std::cout << "\n===== 4. summary =====\n";
    std::cout << "frames read        : " << frames_read << "\n";
    std::cout << "frames with detect : " << frames_with_detect << "\n";
    std::cout << "candidates total   : " << candidates_total << "\n";
    std::cout << "pnp ok (numeric)   : " << pnp_ok_total << "\n";
    std::cout << "pnp valid (passed) : " << pnp_valid_total << "\n";
    std::cout << "suspect (flagged)  : " << suspect_total << "\n";
    if (pnp_valid_total > 0)
    {
        std::cout << "mean reproj err    : " << (sum_reproj / pnp_valid_total) << " px\n";
        std::cout << "distance range     : [" << min_dist << ", " << max_dist << "] m\n";
        std::cout << "tz range           : [" << min_tz << ", " << max_tz << "] m\n";
    }
    std::cout << "csv                : " << out_dir << "/pnp_frames.csv\n";
    std::cout << "annotated video    : " << vwriter.path() << "\n";

    // ---------------------------------------------------------------- 重投影验证图（最佳帧）
    if (best_frame_id >= 0)
    {
        const std::string p = out_dir + "/reproj_check_best.png";
        cv::imwrite(p, task::make_reprojection_check(best_frame_for_reproj, best_corners,
                                                     best_pose, best_frame_id, best_candidate_id));
        std::cout << "[SAVED] " << p << " (frame " << best_frame_id
                  << ", reproj " << best_reproj << " px)\n";
    }
    else
    {
        std::cout << "[NOTE] no valid pose, reprojection check image not generated\n";
    }

    // ---------------------------------------------------------------- 敏感性测试
    if (!best_corners.empty())
    {
        // 先打印"针孔粗深度 vs PnP tz"的交叉校验结果（第六章校验项 ①）
        std::cout << "\n===== rough depth cross-check (best frame " << best_frame_id << ") =====\n";
        std::cout << "  tz (PnP)           = " << best_pose.tvec[2] << " m\n";
        std::cout << "  Z_rough (by height)= " << best_pose.z_rough_height_m << " m\n";
        std::cout << "  Z_rough (by width) = " << best_pose.z_rough_width_m << " m\n";
        if (best_pose.z_rough_height_m > 1e-6)
        {
            std::cout << "  rel err (height)   = "
                      << std::fabs(best_pose.z_rough_height_m - best_pose.tvec[2]) /
                             best_pose.z_rough_height_m
                      << "\n";
        }

        const std::vector<task::sensitivity_row> rows = task::run_sensitivity_test(
            best_corners, cam, cam.armor_width_m, cam.armor_height_m,
            task::kReprojErrThresholdPx);
        task::report_sensitivity(rows, out_dir + "/sensitivity.csv");
    }
    else
    {
        std::cout << "[NOTE] no baseline corners, sensitivity test skipped\n";
    }

    std::cout << "\nDONE\n";
    return 0;
}
