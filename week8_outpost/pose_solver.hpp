#ifndef POSE_SOLVER_H
#define POSE_SOLVER_H

// 位姿解算模块：solvePnP + 重投影验证 + 针孔粗深度校验 + 敏感性测试
//
// 坐标约定（严格遵循 OpenCV 相机坐标系）：
//   原点在光心，x 向右，y 向下，z 向前（沿光轴指向场景）。
//   tvec 是装甲板中心在相机坐标系下的平移；tz 即沿光轴的深度。
//   rvec 是 Rodrigues 旋转向量，本模块额外把 R 分解成欧拉角输出，
//   但绝不把 rvec 直接当成欧拉角使用。

#include "param_loader.hpp"

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace task {

// 一个候选的位姿解算结果
struct pose_result
{
    bool pnp_ok = false;              // solvePnP 是否成功返回
    cv::Vec3d rvec{0, 0, 0};          // Rodrigues 旋转向量（弧度）
    cv::Vec3d tvec{0, 0, 0};          // 平移（米）
    cv::Mat R;                        // 3x3 旋转矩阵
    double rx_rad = 0.0;              // 由 R 分解的欧拉角（rad），R = Rz*Ry*Rx
    double ry_rad = 0.0;
    double rz_rad = 0.0;
    double distance_m = 0.0;          // 直线欧氏距离 sqrt(tx^2+ty^2+tz^2)
    double reproj_err_px = 0.0;       // 4 个角点重投影误差平均值（像素）
    double z_rough_height_m = 0.0;    // 由装甲板"高度"估算的粗深度
    double z_rough_width_m = 0.0;     // 由装甲板"宽度"估算的粗深度
    std::vector<cv::Point2f> reprojected;   // 重投影后的 4 个像素点
    bool finite_values = false;       // 全部输出是否为有限数
    bool suspect = false;             // 是否被校验规则标记为可疑
    std::string suspect_reason;       // 可疑原因（多条用 ; 分隔）
};

// 构造装甲板三维模型点，顺序固定为 左上、右上、右下、左下
//   0: (-w/2, -h/2, 0)   1: ( w/2, -h/2, 0)
//   2: ( w/2,  h/2, 0)   3: (-w/2,  h/2, 0)
std::vector<cv::Point3f> make_object_points(double armor_w_m, double armor_h_m);

// 解算单个装甲板位姿
//   corners : 4 个图像角点，顺序必须是 左上、右上、右下、左下（由 detect_frame 保证）
//   注意：这里直接传原始（带畸变的）像素坐标 + K + D，不做外部 undistort
pose_result solve_pose(const std::vector<cv::Point2f> &corners,
                       const camera_params &cam,
                       double armor_w_m,
                       double armor_h_m,
                       double reproj_thresh_px);

// 针孔近似粗深度：Z ≈ f * H_real / h_pixel
//   返回值同时给出"按高度估算"和"按宽度估算"两种，便于与 tz 交叉验证
void compute_rough_depth(const std::vector<cv::Point2f> &corners,
                         const camera_params &cam,
                         double armor_w_m,
                         double armor_h_m,
                         double &z_by_height,
                         double &z_by_width);

// 敏感性测试的一行结果
struct sensitivity_row
{
    std::string case_name;   // 用例名
    bool ok = false;         // 该用例是否解算成功
    double tz_m = 0.0;
    double distance_m = 0.0;
    double reproj_err_px = 0.0;
};

// 单因素敏感性测试：扰动角点坐标 / 装甲物理尺寸，观察 tz、距离、重投影误差
std::vector<sensitivity_row> run_sensitivity_test(const std::vector<cv::Point2f> &base_corners,
                                                  const camera_params &cam,
                                                  double armor_w_m,
                                                  double armor_h_m,
                                                  double reproj_thresh_px);

// 打印并保存敏感性测试表格
void report_sensitivity(const std::vector<sensitivity_row> &rows,
                        const std::string &out_txt_path);

}  // namespace task

#endif
