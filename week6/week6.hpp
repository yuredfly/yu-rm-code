#ifndef WEEK6_H
#define WEEK6_H

// 第 6 周：预处理 / 阈值分割 / 形态学 / 轮廓目标检测

#include <opencv2/opencv.hpp>
#include <string>
#include <utility>
#include <vector>

// 读取图片；失败时返回空 Mat（imread 本身不抛异常）
cv::Mat load_image(const std::string &path);

// 打印宽、高、通道数（任务要求正常应为 960 x 640、3 通道）
void print_image_info(const cv::Mat &image);

// 路线 A：BGR -> 灰度 -> 高斯滤波 -> 固定阈值
// blur_out 保存 fixed_blur_gray.png 用；mask_out 保存 fixed_mask_gray.png 用
void route_gray_threshold(const cv::Mat &bgr,
                          double threshold_value,
                          cv::Mat &blur_out,
                          cv::Mat &mask_out);

// 路线 B：中值滤波 -> BGR 转 HSV -> inRange 颜色筛选
// blur_out 保存 fixed_blur_color.png 用；mask_out 保存 fixed_mask_hsv.png 用
void route_hsv_inrange(const cv::Mat &bgr,
                       int median_ksize,
                       const cv::Scalar &lower,
                       const cv::Scalar &upper,
                       cv::Mat &blur_out,
                       cv::Mat &mask_out);

// 四种形态学操作
// 【硬性约束】四个输出都必须由 mask_in 直接得到，禁止把上一个输出当输入
void morphology_four(const cv::Mat &mask_in,
                     int kernel_size3,
                     int kernel_size7,
                     cv::Mat &eroded,
                     cv::Mat &dilated,
                     cv::Mat &opened,
                     cv::Mat &closed);

// 轮廓提取与面积过滤
// 【硬性约束】内部先 clone() 再 findContours，保留原 mask 供保存
std::vector<std::vector<cv::Point>> find_target_contours(const cv::Mat &binary_mask,
                                                         double min_area,
                                                         bool print_candidate_areas);

// 单个目标的测量结果
struct target_info
{
    double area;                       // contourArea，单位像素平方
    cv::Rect box;                      // boundingRect：轴对齐框
    cv::RotatedRect rotated;           // minAreaRect：最小旋转框
    cv::Point2f center;                // 质心 (m10/m00, m01/m00)
    bool has_valid_center;             // m00 == 0 时为 false
};

// 测量单个轮廓：面积、两种外接框、中心点
target_info measure_target(const std::vector<cv::Point> &contour);

// 在原图副本上标注：绿=轴对齐框，橙=最小旋转框，红=中心点
cv::Mat draw_targets(const cv::Mat &input_image,
                     const std::vector<std::vector<cv::Point>> &contours,
                     const std::vector<target_info> &infos);

// 显示若干窗口，按任意键关闭
void show_images(const std::vector<std::pair<std::string, cv::Mat>> &items);

// 保存图片，并检查 imwrite 的返回值；失败返回 false
bool save_image_checked(const std::string &path, const cv::Mat &image);

// 打印降序排列的候选面积，便于确定面积阈值
void print_sorted_areas(const std::vector<std::vector<cv::Point>> &contours);

#endif
