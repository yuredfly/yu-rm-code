// 第 6 周 main：OpenCV 预处理、阈值与轮廓目标检测（方案一：固定参数处理指定练习图）
// 流程：读图 -> 两条分割路线 -> 四种形态学（独立输入）-> 轮廓与面积过滤 -> 测量与标注
// 命名与注释风格沿用 Day11、Day20 的历史代码
//
// 风格说明：局部变量用 snake_case、函数 snake_case、Allman 花括号、4 空格缩进

#include "week6.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    const std::string input_path = "images/input.png";   // 项目内相对路径
    const std::string output_dir = "output";             // 相对路径，需提前存在

    const int gaussian_ksize = 5;                        // 灰度路线高斯核 5x5
    const double threshold_value = 120.0;                // 灰度阈值
    const int median_ksize = 5;                          // HSV 路线中值核
    const cv::Scalar hsv_lower(35, 80, 80);              // HSV 下界 (H,S,V)
    const cv::Scalar hsv_upper(85, 255, 255);            // HSV 上界

    const int kernel_size3 = 3;                          // 腐蚀/膨胀/开运算核
    const int kernel_size7 = 7;                          // 闭运算核
    const double min_contour_area = 100.0;               // 面积阈值：小于该值丢弃

    
    // 1. 读取并检查图片
    cv::Mat image = load_image(input_path);
    if (image.empty())
    {
        return 1;                                        // 读取失败时提示并结束
    }
    std::cout << "===== 1. Read and check the image =====\n";
    print_image_info(image);

    // 2. 生成两张 mask：两条路线用不同变量保存，互不覆盖

    std::cout << "\n===== 2. Build two masks =====\n";
    cv::Mat blur_gray, mask_gray;
    route_gray_threshold(image, threshold_value, blur_gray, mask_gray);

    cv::Mat blur_color, mask_hsv;
    route_hsv_inrange(image, median_ksize, hsv_lower, hsv_upper, blur_color, mask_hsv);

    std::cout << "gray-route mask white pixels: " << cv::countNonZero(mask_gray) << "\n";
    std::cout << "HSV-route  mask white pixels: " << cv::countNonZero(mask_hsv) << "\n";

    // 3. 四种形态学：全部以 mask_hsv 为输入（见 morphology_four 内的硬性约束）
    
    std::cout << "\n===== 3. Four morphology ops (each takes mask_hsv independently) =====\n";
    cv::Mat eroded, dilated, opened, closed;
    morphology_four(mask_hsv, kernel_size3, kernel_size7, eroded, dilated, opened, closed);

    std::cout << "mask_hsv white pixels: " << cv::countNonZero(mask_hsv)
              << " | eroded: " << cv::countNonZero(eroded)
              << " | dilated: " << cv::countNonZero(dilated)
              << " | opened: " << cv::countNonZero(opened)
              << " | closed: " << cv::countNonZero(closed) << "\n";

    // 4. 轮廓检测与面积过滤
    
    std::cout << "\n===== 4. Contours and area filtering (input: fixed_opened) =====\n";
    const std::vector<std::vector<cv::Point>> targets =
        find_target_contours(opened, min_contour_area, true);

   
    // 5. 测量：面积、两种外接框、中心点
    
    std::cout << "\n===== 5. Target measurement =====\n";
    std::vector<target_info> infos;
    for (std::size_t i = 0; i < targets.size(); ++i)
    {
        const target_info info = measure_target(targets[i]);
        infos.push_back(info);

        const cv::Rect &box = info.box;
        const cv::RotatedRect &rotated = info.rotated;
        std::cout << "target " << (i + 1)
                  << " | area=" << info.area
                  << " | center=(" << info.center.x << ", " << info.center.y << ")"
                  << " | axis-aligned box=[" << box.x << ", " << box.y << ", " << box.width << "x" << box.height << "]"
                  << " | rotated box size=" << rotated.size.width << "x" << rotated.size.height
                  << " angle=" << rotated.angle
                  << (info.has_valid_center ? "" : "  [WARN] m00=0, center falls back to the rotated box center")
                  << "\n";
    }
    std::cout << "final target count: " << infos.size() << "\n";
    if (infos.size() != 6)
    {
        std::cout << "[NOTE] Target count is not 6. Check in this order: mask -> morphology results -> area threshold.\n";
    }

    
    // 6. 标注与保存
    
    std::cout << "\n===== 6. Annotate and save =====\n";
    const cv::Mat result_image = draw_targets(image, targets, infos);

    save_image_checked(output_dir + "/fixed_blur_gray.png", blur_gray);
    save_image_checked(output_dir + "/fixed_mask_gray.png", mask_gray);
    save_image_checked(output_dir + "/fixed_blur_color.png", blur_color);
    save_image_checked(output_dir + "/fixed_mask_hsv.png", mask_hsv);
    save_image_checked(output_dir + "/fixed_eroded.png", eroded);
    save_image_checked(output_dir + "/fixed_dilated.png", dilated);
    save_image_checked(output_dir + "/fixed_opened.png", opened);
    save_image_checked(output_dir + "/fixed_closed.png", closed);
    save_image_checked(output_dir + "/fixed_result.png", result_image);

    
    // 7. 查看中间结果：按引导要求打开检查
    // 设置环境变量 WEEK6_NO_GUI=1 可跳过窗口显示，便于自动化测试
    
    if (std::getenv("WEEK6_NO_GUI") == nullptr)
    {
        show_images({
            {"fixed_mask_gray", mask_gray},
            {"fixed_mask_hsv", mask_hsv},
            {"fixed_opened", opened},
            {"fixed_closed", closed},
            {"fixed_result", result_image},
        });
    }
    else
    {
        std::cout << "\n[INFO] WEEK6_NO_GUI is set, skipping image windows.\n";
    }

    return 0;
}
