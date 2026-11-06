#ifndef YUANSHEN_H
#define YUANSHEN_H

// 第 6 周 · 方案二（自选图像）：原神十连截图 → 检测 10 张武器卡片 + 卡片底部星级

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// ---------------------------------------------------------------- 基础
// 读取图片；失败返回空 Mat
cv::Mat ys_load(const std::string &path);

// 打印尺寸与通道数
void ys_print_info(const cv::Mat &image);

// 保存图片并检查 imwrite 返回值
bool ys_save(const std::string &path, const cv::Mat &image);

// ---------------------------------------------------------------- 连通域
// 一个连通域的信息
struct ys_blob
{
    int area;          // 像素个数
    int x0, y0;        // 左上角
    int x1, y1;        // 右下角
};

// 8 邻域连通域标记（按出现顺序返回）
std::vector<ys_blob> ys_connected_components(const cv::Mat &mask);

// ---------------------------------------------------------------- 路线 A：HSV 颜色（金色星级条锚点）
// 金色掩膜：HSV 中 H 18~38、S>=140、V>=150
cv::Mat ys_gold_mask(const cv::Mat &bgr);

// 由金色掩膜的列投影分组，得到每张卡的锚点（10 组）
// groups 输出每组在 x 方向的 [x0, x1]
std::vector<cv::Rect> ys_star_groups(const cv::Mat &gold_mask,
                                     int star_y0,
                                     int star_y1,
                                     int min_group_width);

// 数出每条星级条里到底有几颗星
// 做法：用 1x7 横条结构元素做【腐蚀】，把相连的星星分开，再数连通域个数
int ys_count_stars(const cv::Mat &gold_mask, const cv::Rect &star_group, int erode_width);

// ---------------------------------------------------------------- 路线 B：灰度阈值
// 对指定水平条带做灰度阈值 + 开运算，返回卡片大小的大连通域
std::vector<ys_blob> ys_threshold_band(const cv::Mat &gray,
                                       int band_y0,
                                       int band_y1,
                                       int thresh,
                                       int open_size,
                                       int min_area);

// ---------------------------------------------------------------- 卡片几何
// 一张卡片的检测结果
struct ys_card
{
    cv::Rect frame;          // 由锚点推出的卡片区域
    int star_count;          // 底部星星个数（星级）
    float frame_contrast;    // 卡内平均亮度 - 卡外平均亮度（用于验证确实是卡片）
    bool verified;           // 对比度是否达到阈值
};

// 由锚点推导卡片区域：
//   横向按相邻锚点中点切分；纵向先找卡片底边，再由宽度按 card_aspect 推高度
void ys_build_cards(const cv::Mat &gray,
                    const std::vector<cv::Rect> &star_groups,
                    int top_limit,
                    int bottom_limit,
                    double row_thresh,
                    int min_height,
                    double card_aspect,
                    double contrast_thresh,
                    std::vector<ys_card> &cards);

// ---------------------------------------------------------------- 绘制与输出
// 绘制结果：绿=卡片框，橙=旋转框（最小外接），红=中心点，并标注星级
cv::Mat ys_draw_result(const cv::Mat &bgr, const std::vector<ys_card> &cards);

// 把金色掩膜可视化（白=金色像素）
cv::Mat ys_visualize_mask(const cv::Mat &mask);

// 在终端打印每张卡的信息
void ys_print_cards(const std::vector<ys_card> &cards);

#endif
