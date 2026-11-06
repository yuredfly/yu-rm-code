// 传统视觉装甲板识别 —— 单帧检测主程序
//
// 用法：
//   week7_armor                              # 默认读 images/outpost_frame.png
//   week7_armor images/outpost_frame.png     # 命令行指定输入图片（相对路径）
//
// 流程：读图 → 颜色 mask → 灯条候选 → 配对 → 装甲板 → 绘制与保存 → 终端输出

#include "armor_pipeline.hpp"
#include "armor_types.hpp"
#include "armor_visualize.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

// 打印一张图像的尺寸信息
void print_image_info(const cv::Mat &img)
{
    std::cout << "width=" << img.cols
              << " height=" << img.rows
              << " channels=" << img.channels() << "\n";
}

// 输出单个装甲板候选的全部字段（任务要求的五项：颜色/框/中心/四角/有效性）
void print_plate(const armor::armor_plate &plate, std::size_t index)
{
    std::cout << "plate " << index
              << " | color=" << armor::color_name(static_cast<armor::color_id>(plate.color))
              << " | bars=[" << plate.left_bar_id << "," << plate.right_bar_id << "]"
              << " | valid=" << (plate.valid ? "true" : "false") << "\n";
    std::cout << "        bbox=[" << plate.bbox.x << ", " << plate.bbox.y
              << ", " << plate.bbox.width << " x " << plate.bbox.height << "]\n";
    std::cout << "        center=(" << plate.center.x << ", " << plate.center.y << ")\n";
    const char *names[4] = {"LT", "RT", "RB", "LB"};
    for (int k = 0; k < 4; ++k)
    {
        std::cout << "        corner" << k << " " << names[k]
                  << "=(" << plate.corners[k].x << ", " << plate.corners[k].y << ")"
                  << "\n";
    }
    std::cout << "        plate_width=" << plate.plate_width
              << " plate_height=" << plate.plate_height
              << " score=" << plate.score << "\n";
}

}  // namespace

int main(int argc, char **argv)
{
    // ---------------------------------------------------------------- 输入路径
    // 只在这里出现一次输入路径；命令行给了就用命令行的
    std::string input_path = "images/outpost_frame.png";
    if (argc >= 2)
    {
        input_path = argv[1];
    }
    const std::string output_dir = "output";

    std::cout << "===== 1. load image =====\n";
    std::cout << "input: " << input_path << "\n";

    cv::Mat image = cv::imread(input_path, cv::IMREAD_COLOR);
    if (image.empty())
    {
        std::cerr << "[ERROR] Failed to read image: " << input_path << "\n";
        std::cerr << "        Check the relative path and file name "
                     "(the working directory must be the project root).\n";
        return 1;   // 读取失败：提示并正常结束，不对空图像继续调用 OpenCV
    }
    print_image_info(image);

    // ---------------------------------------------------------------- 检测参数
    armor::armor_config cfg;
    // 本张实拍图整体很暗、而且有地面补光灯的强光，这里把饱和度/亮度下限略微调高，
    // 用来压掉墙面上偏暗的红色海报；其余参数保持默认（全部集中在 armor_types.hpp）
    cfg.split_sat_min = 100;
    cfg.split_val_min = 80;
    cfg.bar_min_area = 100.0;

    // ---------------------------------------------------------------- 单帧检测
    std::cout << "\n===== 2. detect (single frame) =====\n";
    const armor::frame_result result = armor::detect_frame(image, cfg);

    std::cout << "masks: " << result.masks.size() << "\n";
    for (std::size_t i = 0; i < result.masks.size(); ++i)
    {
        std::cout << "  " << armor::color_name(result.mask_colors[i])
                  << " mask white pixels: " << cv::countNonZero(result.masks[i]) << "\n";
    }
    std::cout << "light bars: " << result.bars.size() << "\n";
    for (const auto &bar : result.bars)
    {
        std::cout << "  bar #" << bar.id
                  << " color=" << armor::color_name(bar.color)
                  << " center=(" << bar.center.x << ", " << bar.center.y << ")"
                  << " len=" << bar.length << " wid=" << bar.width
                  << " tilt=" << bar.tilt_deg << " fill=" << bar.fill_ratio << "\n";
    }

    std::cout << "plates: " << result.plates.size() << "\n";
    for (std::size_t i = 0; i < result.plates.size(); ++i)
    {
        print_plate(result.plates[i], i);
    }
    if (result.plates.empty())
    {
        std::cout << "[NOTE] No armored plate candidate found. "
                     "Check mask -> light bars -> pairing in this order.\n";
    }

    std::cout << "\ntiming: split=" << result.split_ms
              << "ms bars=" << result.bar_ms
              << "ms pair=" << result.pair_ms << "ms\n";

    // ---------------------------------------------------------------- 保存结果
    std::cout << "\n===== 3. save results =====\n";
    if (!armor::ensure_directory(output_dir))
    {
        std::cerr << "[ERROR] Cannot create output directory.\n";
        return 1;
    }

    // 颜色 mask（白=保留的目标像素，黑=排除）
    for (std::size_t i = 0; i < result.masks.size(); ++i)
    {
        const std::string name =
            output_dir + "/mask_" + armor::color_name(result.mask_colors[i]) + ".png";
        armor::save_image(name, result.masks[i]);
    }

    // 灯条候选图
    armor::save_image(output_dir + "/light_bars.png",
                      armor::draw_candidates(image, result.bars));

    // 最终装甲板结果图
    armor::save_image(output_dir + "/armor_result.png",
                      armor::draw_plates(image, result.bars, result.plates));

    std::cout << "\nDONE\n";
    return 0;
}
