#include "week6.hpp"

#include <algorithm>
#include <functional>
#include <iostream>

// 读取图片，并检查是否为空
cv::Mat load_image(const std::string &path)
{
    // 相对路径从"程序运行时的当前工作目录"开始计算
    cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
    if (image.empty())
    {
        std::cerr << "[ERROR] Failed to read image. Check the path and file name: " << path << "\n";
        std::cerr << "        Common causes: file is not at the relative path under the working directory, wrong extension, or the working directory is not the project directory.\n";
    }
    return image;
}

// 打印图片宽度、高度、通道数
void print_image_info(const cv::Mat &image)
{
    std::cout << "width=" << image.cols
              << " height=" << image.rows
              << " channels=" << image.channels() << "\n";

    // 方案一验收标准：960 x 640、3 通道
    if (image.cols != 960 || image.rows != 640 || image.channels() != 3)
    {
        std::cout << "[WARN] size/channels differ from the expected 960x640 x 3ch. Please make sure images/input.png was loaded.\n";
    }
}

// 路线 A：灰度固定阈值
void route_gray_threshold(const cv::Mat &bgr,
                          double threshold_value,
                          cv::Mat &blur_out,
                          cv::Mat &mask_out)
{
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);   

    // 高斯核必须为正奇数；sigmaX 传 0 表示由核大小自动推算
    const int gaussian_ksize = 5;
    cv::GaussianBlur(gray, blur_out, cv::Size(gaussian_ksize, gaussian_ksize), 0);

    // 本图目标（深绿，灰度约 87）比背景（约 248）暗
    cv::threshold(blur_out, mask_out, threshold_value, 255.0, cv::THRESH_BINARY_INV);
}

// 路线 B：HSV 颜色筛选
void route_hsv_inrange(const cv::Mat &bgr,
                       int median_ksize,
                       const cv::Scalar &lower,
                       const cv::Scalar &upper,
                       cv::Mat &blur_out,
                       cv::Mat &mask_out)
{
    cv::medianBlur(bgr, blur_out, median_ksize);

    cv::Mat hsv;
    cv::cvtColor(blur_out, hsv, cv::COLOR_BGR2HSV);   // 必须用滤波后的图

    cv::inRange(hsv, lower, upper, mask_out);
}

// 四种形态学操作：全部以同一张 mask_in 为输入
void morphology_four(const cv::Mat &mask_in,
                     int kernel_size3,
                     int kernel_size7,
                     cv::Mat &eroded,
                     cv::Mat &dilated,
                     cv::Mat &opened,
                     cv::Mat &closed)
{
    const cv::Mat kernel3 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernel_size3, kernel_size3));
    const cv::Mat kernel7 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernel_size7, kernel_size7));

    // 【硬性约束】下面四次调用的第一个实参全部是 mask_in。
    // 绝对不能写成 morphologyEx(eroded, opened, ...) 这类链式处理：
    // 四种操作串成一条链后，变化会叠加，无法判断是哪种操作造成的。
    cv::erode(mask_in, eroded, kernel3);                                    // 白色区域缩小，小白点可能消失
    cv::dilate(mask_in, dilated, kernel3);                                  // 白色区域扩大
    cv::morphologyEx(mask_in, opened, cv::MORPH_OPEN, kernel3);             // 先腐蚀再膨胀：去掉小白噪点
    cv::morphologyEx(mask_in, closed, cv::MORPH_CLOSE, kernel7);            // 先膨胀再腐蚀：填补小黑孔
}

// 轮廓提取与面积过滤
std::vector<std::vector<cv::Point>> find_target_contours(const cv::Mat &binary_mask,
                                                         double min_area,
                                                         bool print_candidate_areas)
{
    std::vector<std::vector<cv::Point>> contours;

    // 【硬性约束】传 clone()，保留原 mask 供查看或保存
    cv::Mat contour_input = binary_mask.clone();
    cv::findContours(contour_input, contours,
                     cv::RETR_EXTERNAL,           // 只提取最外层轮廓
                     cv::CHAIN_APPROX_SIMPLE);    // 压缩轮廓上连续的冗余点

    std::vector<std::vector<cv::Point>> kept;
    double max_filtered_area = 0.0;

    for (const auto &contour : contours)
    {
        const double area = cv::contourArea(contour);   // 单位是像素平方
        if (print_candidate_areas)
        {
            std::cout << "candidate area: " << area << "\n";   // 任务要求输出候选面积
        }
        if (area < min_area)                             // 面积小于阈值则丢弃
        {
            max_filtered_area = std::max(max_filtered_area, area);
            continue;
        }
        kept.push_back(contour);
    }

    std::cout << "candidate contours (before filter): " << contours.size() << "\n";
    std::cout << "valid targets (after filter): " << kept.size() << "\n";
    std::cout << "largest filtered-out area: " << max_filtered_area
              << " (area threshold " << min_area << ", use this to judge whether the threshold is appropriate)\n";
    return kept;
}

// 测量单个轮廓
target_info measure_target(const std::vector<cv::Point> &contour)
{
    target_info info;

    info.area = cv::contourArea(contour);
    info.box = cv::boundingRect(contour);        // 轴对齐外接矩形
    info.rotated = cv::minAreaRect(contour);     // 最小旋转矩形
    info.center = cv::Point2f(0.f, 0.f);
    info.has_valid_center = false;

    const cv::Moments m = cv::moments(contour);

    if (m.m00 != 0.0)
    {
        info.center.x = static_cast<float>(m.m10 / m.m00);
        info.center.y = static_cast<float>(m.m01 / m.m00);
        info.has_valid_center = true;
    }
    else
    {
        info.center = info.rotated.center;       // 退化轮廓的兜底处理
    }
    return info;
}

// 在原图副本上标注目标
cv::Mat draw_targets(const cv::Mat &input_image,
                     const std::vector<std::vector<cv::Point>> &contours,
                     const std::vector<target_info> &infos)
{
    // 画在副本上，避免改动原图
    cv::Mat result_image = input_image.clone();

    const cv::Scalar axis_aligned_color(0, 255, 0);    // BGR：绿色
    const cv::Scalar rotated_color(0, 165, 255);       // BGR：橙色
    const cv::Scalar center_color(0, 0, 255);          // BGR：红色

    for (std::size_t i = 0; i < contours.size() && i < infos.size(); ++i)
    {
        // 轴对齐框
        cv::rectangle(result_image, infos[i].box, axis_aligned_color, 2);

        // 最小旋转框：取四个角点后依次连线（第 5 条边回到第 1 点以闭合）
        cv::Point2f corners[4];
        infos[i].rotated.points(corners);
        for (int k = 0; k < 4; ++k)
        {
            const cv::Point p1(cvRound(corners[k].x), cvRound(corners[k].y));
            const cv::Point p2(cvRound(corners[(k + 1) % 4].x), cvRound(corners[(k + 1) % 4].y));
            cv::line(result_image, p1, p2, rotated_color, 2);
        }

        // 中心点
        if (infos[i].has_valid_center)
        {
            const cv::Point center(cvRound(infos[i].center.x), cvRound(infos[i].center.y));
            cv::circle(result_image, center, 4, center_color, -1);   // thickness=-1 表示实心
        }
    }
    return result_image;
}

// 显示中间结果窗口
void show_images(const std::vector<std::pair<std::string, cv::Mat>> &items)
{
    for (const auto &item : items)
    {
        if (!item.second.empty())
        {
            cv::imshow(item.first, item.second);
        }
    }
    std::cout << "\nOpened " << items.size() << " windows. Press any key to close...\n";
    cv::waitKey(0);
    cv::destroyAllWindows();
}

// 保存图片并检查返回值
bool save_image_checked(const std::string &path, const cv::Mat &image)
{
    if (image.empty())
    {
        std::cerr << "[ERROR] Image is empty, cannot save: " << path << "\n";
        return false;
    }
    if (!cv::imwrite(path, image))
    {
        std::cerr << "[ERROR] Failed to save: " << path << "\n";
        std::cerr << "        Check that the output/ directory exists and is writable, and the extension is supported.\n";
        return false;
    }
    std::cout << "[SAVED] " << path << "\n";
    return true;
}

// 打印降序排列的候选面积
void print_sorted_areas(const std::vector<std::vector<cv::Point>> &contours)
{
    std::vector<double> areas;
    for (const auto &c : contours)
    {
        areas.push_back(cv::contourArea(c));
    }
    std::sort(areas.begin(), areas.end(), std::greater<double>());

    std::cout << "candidate areas (descending): ";
    for (double a : areas)
    {
        std::cout << a << "  ";
    }
    std::cout << "\n(Observe the boundary between targets and noise, then decide the area threshold)\n";
}
