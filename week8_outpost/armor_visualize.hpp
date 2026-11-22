#ifndef ARMOR_VISUALIZE_H
#define ARMOR_VISUALIZE_H

// 结果绘制与图像保存（只负责"画"和"存"，不含任何检测逻辑）

#include "armor_pipeline.hpp"

#include <opencv2/opencv.hpp>
#include <string>

namespace armor {

// 灯条候选图：画出旋转矩形、中心、脊柱方向与候选编号
cv::Mat draw_candidates(const cv::Mat &bgr,
                        const std::vector<light_bar> &bars);

// 最终结果图：装甲板外接框、中心点、固定顺序的四角点及编号 0~3
cv::Mat draw_plates(const cv::Mat &bgr,
                    const std::vector<light_bar> &bars,
                    const std::vector<armor_plate> &plates);

// 保存图像并检查 imwrite 返回值；失败返回 false 并打印原因
bool save_image(const std::string &path, const cv::Mat &image);

// 创建目录（已存在返回 true）
bool ensure_directory(const std::string &dir);

}  // namespace armor

#endif
