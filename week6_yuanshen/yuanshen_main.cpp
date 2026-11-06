// 第 6 周 · 方案二（自选图像）：原神十连截图 → 10 张武器卡片 + 底部星级识别
//
// 主流程：
//   1. 读图并检查
//   2. 路线 A（HSV 颜色）：金色星级条掩膜 → 列投影得到 10 个锚点 → 每张卡星级
//   3. 路线 B（灰度阈值）：整卡分割，观察辉光造成的困难并记录对比结论
//   4. 由锚点 + 几何推出每张卡的区域，用卡内外亮度差验证
//   5. 标注、保存、终端输出
//
//
// 用法：week6_yuanshen.exe [缩放比例]
//   不带参数    -> 使用原图（比例 1.0）
//   带参数 0.5  -> 任务测试④：把图片缩小一半后重新处理

#include "yuanshen.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    // ---------------------------------------------------------------- 参数区
    const std::string input_path = "images/yuanshen.png";   // 项目内相对路径
    const std::string output_dir = "output";

    // 星级条所在的水平条带（金色星星都落在这段高度内）
    const int star_y0 = 810;
    const int star_y1 = 865;
    const int min_group_width = 30;      // 星级条最小宽度（用于过滤噪点），缩放时按比例调整

    // 金色星星彼此相连，用 1x9 的横条结构元素腐蚀把它们切开
    const int star_erode_width = 9;

    // 卡片纵向范围搜索区间
    const int top_limit = 296;
    const int bottom_limit = 1000;
    const double row_thresh = 92.0;      // 行均值高于此值视为"卡片面"
        const int min_height = 300;          // 卡片最小高度
    const double card_aspect = 4.2;      // 卡片高/宽（本图实测约 618/147）
    const double contrast_thresh = 15.0; // 卡内外亮度差的最小值（验证用）

    // 路线 B 的灰度阈值参数
    const int band_y0 = 320;
    const int band_y1 = 900;
    const int gray_thresh = 105;
    const int open_size = 3;
    const int min_card_area = 200000;    // 只有整卡大小的大连通域才算"卡片"

    // ---------------------------------------------------------------- 1. 读图
    cv::Mat image = ys_load(input_path);
    if (image.empty())
    {
        return 1;   // 读取失败时提示并结束
    }
    std::cout << "===== 1. Read and check the image =====\n";
    ys_print_info(image);

    // 任务测试④：可通过命令行参数把图片缩小后重新处理
    // 例：week6_yuanshen.exe 0.5
    double scale = 1.0;
    if (argc >= 2)
    {
        scale = std::atof(argv[1]);
    }
    int star_y0_use = star_y0;
    int star_y1_use = star_y1;
    int top_limit_use = top_limit;
    int bottom_limit_use = bottom_limit;
    int min_area_use = min_card_area;
    int min_group_use = min_group_width;

    if (scale > 0.0 && scale != 1.0)
    {
        cv::Mat scaled;
        cv::resize(image, scaled, cv::Size(), scale, scale, cv::INTER_AREA);
        image = scaled;
        std::cout << "[TEST] Resized by scale " << scale << ", new size: ";
        ys_print_info(image);

        // 与尺寸相关的参数一并按比例缩放（固定像素值不会自动适应新尺寸）
        star_y0_use = static_cast<int>(star_y0 * scale);
        star_y1_use = static_cast<int>(star_y1 * scale);
        top_limit_use = static_cast<int>(top_limit * scale);
        bottom_limit_use = static_cast<int>(bottom_limit * scale);
        min_area_use = static_cast<int>(min_card_area * scale * scale);
        min_group_use = std::max(6, static_cast<int>(min_group_width * scale));
    }

    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    // ---------------------------------------------------------------- 2. 路线 A：HSV 金色星级条
    std::cout << "\n===== 2. Route A: HSV color screening (gold star bars) =====\n";
    const cv::Mat gold_mask = ys_gold_mask(image);
    std::cout << "gold pixels total: " << cv::countNonZero(gold_mask) << "\n";

    const std::vector<cv::Rect> star_groups =
        ys_star_groups(gold_mask, star_y0_use, star_y1_use, min_group_use);
    std::cout << "star-bar groups detected: " << star_groups.size() << "\n";
    for (std::size_t i = 0; i < star_groups.size(); ++i)
    {
        const cv::Rect &g = star_groups[i];
        std::cout << "  star bar " << (i + 1)
                  << " | x=" << g.x << " width=" << g.width << "\n";
    }

    // ---------------------------------------------------------------- 3. 路线 B：灰度阈值整卡分割
    std::cout << "\n===== 3. Route B: gray threshold on whole cards =====\n";
    const std::vector<ys_blob> gray_blobs =
        ys_threshold_band(gray, band_y0, band_y1, gray_thresh, open_size, min_area_use);
    std::cout << "card-sized components from gray threshold: " << gray_blobs.size() << "\n";
    for (std::size_t i = 0; i < gray_blobs.size() && i < 5; ++i)
    {
        const ys_blob &b = gray_blobs[i];
        std::cout << "  component " << (i + 1)
                  << " | area=" << b.area
                  << " | region=[" << b.x0 << ", " << b.y0
                  << ", " << (b.x1 - b.x0 + 1) << "x" << (b.y1 - b.y0 + 1) << "]\n";
    }
    std::cout << "Conclusion: the colored glow behind the cards is connected at pixel level in this image, "
                 "so a gray threshold cannot separate the 10 cards." << "\n";

    // ---------------------------------------------------------------- 4. 由锚点推导卡片并数星级
    std::cout << "\n===== 4. Derive card regions from anchors + read star level =====\n";
    std::vector<ys_card> cards;
    ys_build_cards(gray, star_groups, top_limit_use, bottom_limit_use,
                   row_thresh, min_height, card_aspect,
                   contrast_thresh, cards);

    for (std::size_t i = 0; i < cards.size() && i < star_groups.size(); ++i)
    {
        cards[i].star_count = ys_count_stars(gold_mask, star_groups[i], star_erode_width);
    }

    ys_print_cards(cards);
    std::cout << "final card count: " << cards.size() << "\n";

    // 星级分布统计
    int count5 = 0;
    int count4 = 0;
    int count3 = 0;
    for (const auto &c : cards)
    {
        if (c.star_count >= 5)
        {
            count5++;
        }
        else if (c.star_count == 4)
        {
            count4++;
        }
        else if (c.star_count == 3)
        {
            count3++;
        }
    }
    std::cout << "star distribution: 5-star " << count5 << ", 4-star " << count4
              << ", 3-star " << count3 << "\n";

    // ---------------------------------------------------------------- 5. 标注与保存
    std::cout << "\n===== 5. Annotate and save =====\n";
    const cv::Mat result = ys_draw_result(image, cards);
    const cv::Mat mask_view = ys_visualize_mask(gold_mask);

    ys_save(output_dir + "/ys_gold_mask.png", mask_view);
    ys_save(output_dir + "/ys_result.png", result);

    // ---------------------------------------------------------------- 6. 查看结果
    // 设置环境变量 WEEK6_NO_GUI=1 可跳过窗口显示，便于自动化测试
    if (std::getenv("WEEK6_NO_GUI") == nullptr)
    {
        cv::imshow("gold mask (HSV)", mask_view);
        cv::imshow("yuanshen result", result);
        std::cout << "\nPress any key to close the windows...\n";
        cv::waitKey(0);
        cv::destroyAllWindows();
    }
    else
    {
        std::cout << "\n[INFO] WEEK6_NO_GUI is set, skipping image windows.\n";
    }

    return 0;
}
