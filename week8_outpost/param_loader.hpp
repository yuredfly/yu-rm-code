#ifndef PARAM_LOADER_H
#define PARAM_LOADER_H

// 从《参数.md》解析相机内参与装甲板尺寸
//
// 解析失败时的处理策略（工程优化，不改变任务要求）：
//   仍使用与参数.md 完全一致的默认值，并在 from_file=false 时打印警告，
//   避免"参数文件路径写错"直接把整个流程卡死，同时让调用者一眼看出用的是什么。

#include <opencv2/opencv.hpp>
#include <string>

namespace task {

struct camera_params
{
    cv::Mat K;                  // 3x3 CV_64F 内参矩阵
    cv::Mat D;                  // 1x5 CV_64F 畸变系数 [k1,k2,p1,p2,k3]
    int image_width = 0;        // 标定图像宽度
    int image_height = 0;       // 标定图像高度
    double armor_width_m = 0.0; // 装甲板宽度（米）
    double armor_height_m = 0.0;// 装甲板高度（米）
    bool from_file = false;     // 是否成功从文件解析
    std::string note;           // 解析过程说明（缺失了哪些字段等）
};

// 读取并解析参数文件；失败返回带默认值的结果（from_file=false）
camera_params load_camera_params(const std::string &path);

// 打印参数（终端可见，便于核对）
void print_camera_params(const camera_params &p);

}  // namespace task

#endif
