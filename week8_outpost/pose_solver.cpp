#include "pose_solver.hpp"

#include "task_config.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace task {

namespace {

// 判断一个 double 是否是有限数
bool is_finite(double v)
{
    return std::isfinite(v) != 0;
}

// 由旋转矩阵分解欧拉角（约定 R = Rz(rz) * Ry(ry) * Rx(rx)，rad）
void rotation_matrix_to_euler(const cv::Mat &R, double &rx, double &ry, double &rz)
{
    // R.at<double>(row, col)
    const double r20 = R.at<double>(2, 0);
    const double r21 = R.at<double>(2, 1);
    const double r22 = R.at<double>(2, 2);
    const double r10 = R.at<double>(1, 0);
    const double r00 = R.at<double>(0, 0);

    const double sin_ry = std::max(-1.0, std::min(1.0, -r20));
    ry = std::asin(sin_ry);
    rx = std::atan2(r21, r22);
    rz = std::atan2(r10, r00);
}

}  // namespace

std::vector<cv::Point3f> make_object_points(double armor_w_m, double armor_h_m)
{
    const double hw = armor_w_m * 0.5;
    const double hh = armor_h_m * 0.5;
    std::vector<cv::Point3f> pts;
    pts.reserve(4);
    pts.push_back(cv::Point3f((float)(-hw), (float)(-hh), 0.f));  // 0 左上
    pts.push_back(cv::Point3f((float)(hw), (float)(-hh), 0.f));   // 1 右上
    pts.push_back(cv::Point3f((float)(hw), (float)(hh), 0.f));    // 2 右下
    pts.push_back(cv::Point3f((float)(-hw), (float)(hh), 0.f));   // 3 左下
    return pts;
}

void compute_rough_depth(const std::vector<cv::Point2f> &corners,
                         const camera_params &cam,
                         double armor_w_m,
                         double armor_h_m,
                         double &z_by_height,
                         double &z_by_width)
{
    z_by_height = 0.0;
    z_by_width = 0.0;
    if (corners.size() < 4)
    {
        return;
    }

    const double fx = cam.K.at<double>(0, 0);
    const double fy = cam.K.at<double>(1, 1);

    // 装甲板在图像中的"高度"：左竖边与右竖边的像素长度均值
    const double h_left = cv::norm(corners[3] - corners[0]);   // LB - LT
    const double h_right = cv::norm(corners[2] - corners[1]);  // RB - RT
    const double h_px = 0.5 * (h_left + h_right);

    // 装甲板在图像中的"宽度"：上边与下边的像素长度均值
    const double w_top = cv::norm(corners[1] - corners[0]);    // RT - LT
    const double w_bottom = cv::norm(corners[2] - corners[3]); // RB - LB
    const double w_px = 0.5 * (w_top + w_bottom);

    // 针孔近似：Z ≈ f * 真实尺寸 / 像素尺寸
    z_by_height = (h_px > 1e-6) ? (fy * armor_h_m / h_px) : 0.0;
    z_by_width = (w_px > 1e-6) ? (fx * armor_w_m / w_px) : 0.0;
}

pose_result solve_pose(const std::vector<cv::Point2f> &corners,
                       const camera_params &cam,
                       double armor_w_m,
                       double armor_h_m,
                       double reproj_thresh_px)
{
    pose_result r;
    if (corners.size() != 4 || cam.K.empty() || cam.D.empty())
    {
        r.suspect = true;
        r.suspect_reason = "invalid input";
        return r;
    }

    const std::vector<cv::Point3f> obj = make_object_points(armor_w_m, armor_h_m);

    // 关键：直接传"原始带畸变"的角点 + K + D，畸变由 solvePnP 内部处理，
    // 外部不做 undistort（否则角点与 K/D 不再匹配，深度会系统偏移）。
    bool ok = false;
    try
    {
        // 平面四点优先用 IPPE（专为平面标靶设计，数值更稳）
        ok = cv::solvePnP(obj, corners, cam.K, cam.D, r.rvec, r.tvec, false, cv::SOLVEPNP_IPPE);
    }
    catch (const cv::Exception &)
    {
        ok = false;
    }
    if (!ok)
    {
        // 退化情况回退到迭代法
        try
        {
            ok = cv::solvePnP(obj, corners, cam.K, cam.D, r.rvec, r.tvec, false,
                              cv::SOLVEPNP_ITERATIVE);
        }
        catch (const cv::Exception &)
        {
            ok = false;
        }
    }

    r.pnp_ok = ok;
    if (!ok)
    {
        r.suspect = true;
        r.suspect_reason = "solvePnP returned false";
        return r;
    }

    // Rodrigues 旋转向量 -> 旋转矩阵
    cv::Rodrigues(r.rvec, r.R);
    rotation_matrix_to_euler(r.R, r.rx_rad, r.ry_rad, r.rz_rad);

    // 重投影：projectPoints 内部同样使用 K/D，与 solvePnP 保持一致
    cv::projectPoints(obj, r.rvec, r.tvec, cam.K, cam.D, r.reprojected);

    // 平均重投影误差：4 个角点的像素误差平均值
    double sum = 0.0;
    const std::size_t n = std::min(corners.size(), r.reprojected.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        sum += cv::norm(corners[i] - r.reprojected[i]);
    }
    r.reproj_err_px = (n > 0) ? (sum / static_cast<double>(n)) : 0.0;

    // 直线欧氏距离（不是 tz）
    r.distance_m = std::sqrt(r.tvec[0] * r.tvec[0] + r.tvec[1] * r.tvec[1] + r.tvec[2] * r.tvec[2]);

    // 针孔粗深度（用于与 tz 交叉校验）
    compute_rough_depth(corners, cam, armor_w_m, armor_h_m, r.z_rough_height_m, r.z_rough_width_m);

    // ---- 校验判定规则 ----
    r.finite_values = is_finite(r.tvec[0]) && is_finite(r.tvec[1]) && is_finite(r.tvec[2]) &&
                      is_finite(r.rx_rad) && is_finite(r.ry_rad) && is_finite(r.rz_rad) &&
                      is_finite(r.distance_m) && is_finite(r.reproj_err_px);

    std::ostringstream reasons;
    if (!r.finite_values)
    {
        reasons << "non-finite output;";
    }
    if (!(r.tvec[2] > 0.0))
    {
        reasons << "tz<=0;";
    }
    if (r.reproj_err_px > reproj_thresh_px)
    {
        reasons << "reproj>thr;";
    }
    if (r.distance_m > kMaxReasonableDistanceM)
    {
        reasons << "distance too large;";
    }
    // 粗深度与 tz 的相对偏差过大也视为可疑（说明分割角点不可信）
    if (r.z_rough_height_m > 1e-6)
    {
        const double rel = std::fabs(r.z_rough_height_m - r.tvec[2]) / r.z_rough_height_m;
        if (rel > kDepthMismatchRatio)
        {
            reasons << "rough-depth mismatch;";
        }
    }
    r.suspect_reason = reasons.str();
    r.suspect = !r.suspect_reason.empty();
    return r;
}

std::vector<sensitivity_row> run_sensitivity_test(const std::vector<cv::Point2f> &base_corners,
                                                  const camera_params &cam,
                                                  double armor_w_m,
                                                  double armor_h_m,
                                                  double reproj_thresh_px)
{
    std::vector<sensitivity_row> rows;

    auto add_case = [&](const std::string &name,
                        const std::vector<cv::Point2f> &c,
                        double w,
                        double h) {
        const pose_result r = solve_pose(c, cam, w, h, reproj_thresh_px);
        sensitivity_row row;
        row.case_name = name;
        row.ok = r.pnp_ok;
        row.tz_m = r.tvec[2];
        row.distance_m = r.distance_m;
        row.reproj_err_px = r.reproj_err_px;
        rows.push_back(row);
    };

    if (base_corners.size() != 4)
    {
        return rows;
    }

    // 基准
    add_case("baseline", base_corners, armor_w_m, armor_h_m);

    // 角点整体平移 +1 px（模拟检测框整体偏移）
    {
        std::vector<cv::Point2f> c = base_corners;
        for (auto &p : c)
        {
            p.x += 1.0f;
        }
        add_case("corners dx +1px (all)", c, armor_w_m, armor_h_m);
    }

    // 单个角点（左上）沿 x 偏移 +1 px（模拟角点定位误差）
    {
        std::vector<cv::Point2f> c = base_corners;
        c[0].x += 1.0f;
        add_case("corner0 dx +1px", c, armor_w_m, armor_h_m);
    }

    // 单个角点（右下）沿 y 偏移 +1 px
    {
        std::vector<cv::Point2f> c = base_corners;
        c[2].y += 1.0f;
        add_case("corner2 dy +1px", c, armor_w_m, armor_h_m);
    }

    // 装甲板宽度 +10%（尺寸标定误差）
    add_case("armor width x1.10", base_corners, armor_w_m * 1.10, armor_h_m);

    // 装甲板宽度 -10%
    add_case("armor width x0.90", base_corners, armor_w_m * 0.90, armor_h_m);

    // 装甲板高度 +10%
    add_case("armor height x1.10", base_corners, armor_w_m, armor_h_m * 1.10);

    return rows;
}

void report_sensitivity(const std::vector<sensitivity_row> &rows,
                        const std::string &out_txt_path)
{
    if (rows.empty())
    {
        std::cout << "[SENS] no baseline available, sensitivity test skipped\n";
        return;
    }

    const sensitivity_row &base = rows.front();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);

    auto line = [&](const sensitivity_row &r) {
        std::ostringstream s;
        s << std::left << std::setw(24) << r.case_name << std::right
          << " tz=" << std::setw(9) << r.tz_m
          << " dist=" << std::setw(9) << r.distance_m
          << " reproj=" << std::setw(8) << r.reproj_err_px
          << "  d_tz=" << std::setw(9) << (r.tz_m - base.tz_m)
          << " d_dist=" << std::setw(9) << (r.distance_m - base.distance_m)
          << " d_reproj=" << std::setw(8) << (r.reproj_err_px - base.reproj_err_px);
        return s.str();
    };

    std::cout << "\n===== sensitivity test (single-factor) =====\n";
    for (const auto &r : rows)
    {
        std::cout << line(r) << "\n";
        oss << line(r) << "\n";
    }

    std::ofstream fout(std::filesystem::u8path(out_txt_path.c_str()));
    if (fout.is_open())
    {
        fout << "case,tz_m,distance_m,reproj_err_px,d_tz,d_distance,d_reproj\n";
        fout << std::fixed << std::setprecision(6);
        for (const auto &r : rows)
        {
            fout << r.case_name << "," << r.tz_m << "," << r.distance_m << ","
                 << r.reproj_err_px << "," << (r.tz_m - base.tz_m) << ","
                 << (r.distance_m - base.distance_m) << ","
                 << (r.reproj_err_px - base.reproj_err_px) << "\n";
        }
        fout.close();
        std::cout << "[SAVED] " << out_txt_path << "\n";
    }
}

}  // namespace task
