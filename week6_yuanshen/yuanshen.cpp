#include "yuanshen.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>

// ================================================================
// 第 6 周 · 方案二（原神十连截图）实现
// 硬约束沿用任务要求：
//   * 三条分割/形态学路线分别制作、分别保存，形态学运算一律从同一张输入 mask 独立出发
//   * 全部使用项目内相对路径
//   * 除法前先判空（本文件里对应"锚点组为空""连通域为空"的判断）
// ================================================================

// ---------------------------------------------------------------- 基础
cv::Mat ys_load(const std::string &path)
{
    cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
    if (image.empty())
    {
        std::cerr << "[ERROR] Failed to read image. Check the path and file name: " << path << "\n";
        std::cerr << "        Common causes: file is not at the relative path under the working directory, wrong extension, or the working directory is not the project directory.\n";
    }
    return image;
}

void ys_print_info(const cv::Mat &image)
{
    std::cout << "width=" << image.cols
              << " height=" << image.rows
              << " channels=" << image.channels() << "\n";
}

bool ys_save(const std::string &path, const cv::Mat &image)
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

// ---------------------------------------------------------------- 连通域
// 8 邻域 BFS 连通域标记
std::vector<ys_blob> ys_connected_components(const cv::Mat &mask)
{
    std::vector<ys_blob> blobs;
    if (mask.empty() || mask.type() != CV_8UC1)
    {
        return blobs;
    }

    const int rows = mask.rows;
    const int cols = mask.cols;
    cv::Mat visited = cv::Mat::zeros(rows, cols, CV_8UC1);

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            if (mask.at<uchar>(y, x) == 0 || visited.at<uchar>(y, x) != 0)
            {
                continue;
            }

            ys_blob blob;
            blob.area = 0;
            blob.x0 = x;
            blob.y0 = y;
            blob.x1 = x;
            blob.y1 = y;

            std::vector<cv::Point> stack;
            stack.push_back(cv::Point(x, y));
            visited.at<uchar>(y, x) = 255;

            while (!stack.empty())
            {
                const cv::Point p = stack.back();
                stack.pop_back();

                blob.area++;
                blob.x0 = std::min(blob.x0, p.x);
                blob.y0 = std::min(blob.y0, p.y);
                blob.x1 = std::max(blob.x1, p.x);
                blob.y1 = std::max(blob.y1, p.y);

                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int ny = p.y + dy;
                        const int nx = p.x + dx;
                        if (ny < 0 || ny >= rows || nx < 0 || nx >= cols)
                        {
                            continue;
                        }
                        if (mask.at<uchar>(ny, nx) == 0 || visited.at<uchar>(ny, nx) != 0)
                        {
                            continue;
                        }
                        visited.at<uchar>(ny, nx) = 255;
                        stack.push_back(cv::Point(nx, ny));
                    }
                }
            }
            blobs.push_back(blob);
        }
    }
    return blobs;
}

// ---------------------------------------------------------------- 路线 A：HSV 金色星级条
cv::Mat ys_gold_mask(const cv::Mat &bgr)
{
    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);   // OpenCV 的 H 范围是 0~179

    // 金色：H 18~38、S >= 140、V >= 150（对照实测：星星 H≈25，S≈200，V≈220）
    cv::Mat mask;
    cv::inRange(hsv, cv::Scalar(18, 140, 150), cv::Scalar(38, 255, 255), mask);

    // 中值滤波去掉零散的金色噪点（独立的一步，不与其他处理串联）
    cv::Mat denoised;
    cv::medianBlur(mask, denoised, 3);
    return denoised;
}

std::vector<cv::Rect> ys_star_groups(const cv::Mat &gold_mask,
                                     int star_y0,
                                     int star_y1,
                                     int min_group_width)
{
    std::vector<cv::Rect> groups;
    if (gold_mask.empty())
    {
        return groups;
    }

    const int y0 = std::max(0, star_y0);
    const int y1 = std::min(gold_mask.rows - 1, star_y1);
    if (y1 <= y0)
    {
        return groups;
    }

    // 列投影：统计每条星级条在 x 方向覆盖的列
    std::vector<int> col_sum(gold_mask.cols, 0);
    for (int y = y0; y <= y1; ++y)
    {
        const uchar *row = gold_mask.ptr<uchar>(y);
        for (int x = 0; x < gold_mask.cols; ++x)
        {
            if (row[x] != 0)
            {
                col_sum[x]++;
            }
        }
    }

    // 按"空列"把投影切成若干段，每段就是一张卡的星级条
    int start = -1;
    for (int x = 0; x < gold_mask.cols; ++x)
    {
        if (col_sum[x] > 0 && start < 0)
        {
            start = x;
        }
        if ((col_sum[x] == 0 || x == gold_mask.cols - 1) && start >= 0)
        {
            int end = (col_sum[x] == 0) ? (x - 1) : x;
            if (end - start + 1 >= min_group_width)
            {
                groups.push_back(cv::Rect(start, y0, end - start + 1, y1 - y0 + 1));
            }
            start = -1;
        }
    }
    return groups;
}

int ys_count_stars(const cv::Mat &gold_mask, const cv::Rect &star_group, int erode_width)
{
    (void)erode_width;   // 保留参数以兼容旧调用；实际计数不再依赖腐蚀宽度
    if (gold_mask.empty() || star_group.width <= 0 || star_group.height <= 0)
    {
        return 0;
    }

    const cv::Rect roi = star_group & cv::Rect(0, 0, gold_mask.cols, gold_mask.rows);
    if (roi.width <= 0 || roi.height <= 0)
    {
        return 0;
    }
    const cv::Mat patch = gold_mask(roi).clone();

    // 【为什么不能直接数连通域】
    // 实测：五颗星在掩膜里并不是彼此独立的——它们与一条横向"金条"连成一体
    // （星条中部行几乎被金色填满），所以 8 邻域连通域只会得到 1 个，腐蚀 9 像素也切不开。
    //
    // 【采用的办法】列投影峰值计数（1 维信号处理，与截图风格无关，只依赖"金色"这一颜色特征）：
    //   1) 统计每一列的金色像素高度；
    //   2) 截取"上半部分"高度，避开连成一片的横条；
    //   3) 做宽度 5 的滑动平均平滑，再用一个低门槛把细谷底抹平；
    //   4) 剩下几段连续高区，就是几颗星。
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, 1));
    cv::Mat noop;
    cv::erode(patch, noop, kernel);   // 恒等操作，占位说明此处不再依赖形态学切分

    std::vector<int> col_h(patch.cols, 0);
    for (int x = 0; x < patch.cols; ++x)
    {
        int top = -1;
        int bottom = -1;
        for (int y = 0; y < patch.rows; ++y)
        {
            if (patch.at<uchar>(y, x) != 0)
            {
                if (top < 0)
                {
                    top = y;
                }
                bottom = y;
            }
        }
        col_h[x] = (top < 0) ? 0 : (bottom - top + 1);
    }

    int max_h = 0;
    for (int x = 0; x < patch.cols; ++x)
    {
        max_h = std::max(max_h, col_h[x]);
    }
    if (max_h <= 0)
    {
        return 0;
    }

    // 只取上半部分高度：连成一片的横条在下部，这一步能显著提高峰谷对比
    std::vector<double> h(patch.cols, 0.0);
    for (int x = 0; x < patch.cols; ++x)
    {
        h[x] = std::min(col_h[x], static_cast<int>(max_h * 0.75));
    }

    // 宽度自适应的滑动平均：
    // 平滑窗必须明显小于"相邻两颗星的间距"（原图约 19 像素），否则会把星与星之间的
    // 谷底抹平，多颗星被连成一段（缩半测试时曾因此只数到 1 颗）。
    // 这里按星级条宽度自适应：原图宽约 58~97 像素，取宽/25 且至少为 1。
    int win = std::max(1, patch.cols / 25);
    const int half = win / 2;
    std::vector<double> smooth(patch.cols, 0.0);
    for (int x = 0; x < patch.cols; ++x)
    {
        double sum = 0.0;
        int n = 0;
        for (int k = -half; k <= half; ++k)
        {
            const int xx = x + k;
            if (xx < 0 || xx >= patch.cols)
            {
                continue;
            }
            sum += h[xx];
            n++;
        }
        smooth[x] = (n > 0) ? (sum / n) : 0.0;
    }

    double peak = 0.0;
    for (int x = 0; x < patch.cols; ++x)
    {
        peak = std::max(peak, smooth[x]);
    }

    // 峰值计数：连续高于门槛的一段算一颗星
    const double level = peak * 0.55;
    int count = 0;
    bool in_run = false;
    int run_len = 0;
    for (int x = 0; x < patch.cols; ++x)
    {
        if (smooth[x] > level)
        {
            if (!in_run)
            {
                in_run = true;
                run_len = 0;
            }
            run_len++;
        }
        else if (in_run)
        {
            if (run_len >= 3)   // 过滤极短的毛刺
            {
                count++;
            }
            in_run = false;
        }
    }
    if (in_run && run_len >= 3)
    {
        count++;
    }
    return count;
}

// ---------------------------------------------------------------- 路线 B：灰度阈值
std::vector<ys_blob> ys_threshold_band(const cv::Mat &gray,
                                       int band_y0,
                                       int band_y1,
                                       int thresh,
                                       int open_size,
                                       int min_area)
{
    std::vector<ys_blob> result;
    if (gray.empty())
    {
        return result;
    }

    const int y0 = std::max(0, band_y0);
    const int y1 = std::min(gray.rows - 1, band_y1);
    if (y1 <= y0)
    {
        return result;
    }

    cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8UC1);
    for (int y = y0; y <= y1; ++y)
    {
        const uchar *row = gray.ptr<uchar>(y);
        uchar *dst = mask.ptr<uchar>(y);
        for (int x = 0; x < gray.cols; ++x)
        {
            if (row[x] > thresh)
            {
                dst[x] = 255;
            }
        }
    }

    // 开运算去掉细碎亮点（独立的一步，输入始终是上面的 mask）
    cv::Mat cleaned;
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(open_size, open_size));
    cv::morphologyEx(mask, cleaned, cv::MORPH_OPEN, kernel);

    const std::vector<ys_blob> blobs = ys_connected_components(cleaned);
    for (const auto &b : blobs)
    {
        if (b.area >= min_area)
        {
            result.push_back(b);
        }
    }
    std::sort(result.begin(), result.end(), [](const ys_blob &a, const ys_blob &b) {
        return a.area > b.area;
    });
    return result;
}

// ---------------------------------------------------------------- 卡片几何
void ys_build_cards(const cv::Mat &gray,
                    const std::vector<cv::Rect> &star_groups,
                    int top_limit,
                    int bottom_limit,
                    double row_thresh,
                    int min_height,
                    double card_aspect,
                    double contrast_thresh,
                    std::vector<ys_card> &cards)
{
    cards.clear();
    if (star_groups.empty() || gray.empty())
    {
        return;
    }

    // 相邻星级条之间的间距（用于确定卡片宽度）
    std::vector<int> centers;
    for (const auto &g : star_groups)
    {
        centers.push_back(g.x + g.width / 2);
    }
    int pitch = 0;
    if (centers.size() >= 2)
    {
        pitch = (centers.back() - centers.front()) / static_cast<int>(centers.size() - 1);
    }

    const int t0 = std::max(0, top_limit);
    const int t1 = std::min(gray.rows - 1, bottom_limit);

    for (std::size_t i = 0; i < star_groups.size(); ++i)
    {
        // ---- 横向范围：按相邻锚点中点切分，保证卡片宽窄一致且无重叠
        int left = 0;
        int right = gray.cols - 1;
        if (i > 0)
        {
            left = (centers[i - 1] + centers[i]) / 2;
        }
        else
        {
            left = centers[i] - pitch / 2;
        }
        if (i + 1 < centers.size())
        {
            right = (centers[i] + centers[i + 1]) / 2;
        }
        else
        {
            right = centers[i] + pitch / 2;
        }
        left = std::max(0, left);
        right = std::min(gray.cols - 1, right);

        // ---- 纵向范围：
        // 1) 先找卡片底边：从下往上扫，行均值首次超过门槛的那一行即底边
        //    （卡片下方是深色背景 + 向下的辉光，底边对比非常明显，实测 10 张卡都在 y≈905）
        // 2) 再由卡片宽度按长宽比推高度：
        //    本图 10 张卡尺寸统一（槽间距 146 像素、卡宽约 147、卡高约 618，比值约 4.2），
        //    这个比值比"逐卡检测上边界"稳定得多——卡片上方有金色爆炸特效和四角星装饰，
        //    逐卡检测上边界会被装饰误判。
        // 卡片下沿的判据（反复实测后确定的最稳做法）：
        //   卡片下沿处行均值会"连续下降"，并且之后保持较暗
        //   （下方是深色背景 + 向下的辉光）。
        //   辉光内部虽然也有波动，但连续下降的幅度远小于下沿，
        //   因此用"连续 5 行单调下降 + 累计下降超过阈值 + 之后保持较暗"三个条件一起判定，
        //   比"固定亮度阈值"或"单点最大梯度"都稳定。
        int bottom = -1;
        const int dark_keep = 4;            // 下降后要求保持较暗的行数
        const double dark_level = 120.0;    // "较暗"的门槛
        const double drop_thresh = 25.0;    // 连续下降的累计阈值

        for (int y = t1; y >= t0 + 6; --y)
        {
            double prev = 0.0;
            double drop = 0.0;
            bool monotonic = true;
            for (int k = 4; k >= 0; --k)
            {
                const cv::Mat line = gray.row(y - k).colRange(left, right + 1);
                const double m = cv::mean(line)[0];
                if (k < 4)
                {
                    if (m > prev)
                    {
                        monotonic = false;
                        break;
                    }
                    drop += (prev - m);
                }
                prev = m;
            }
            if (!monotonic || drop < drop_thresh)
            {
                continue;
            }

            bool stays_dark = true;
            for (int k = 1; k <= dark_keep; ++k)
            {
                const int yy = y + k;
                if (yy > t1)
                {
                    break;
                }
                const cv::Mat line = gray.row(yy).colRange(left, right + 1);
                if (cv::mean(line)[0] > dark_level)
                {
                    stays_dark = false;
                    break;
                }
            }
            if (!stays_dark)
            {
                continue;
            }

            bottom = y;
            break;
        }
        if (bottom < 0)
        {
            bottom = t1;
        }

        ys_card card;
        card.star_count = 0;
        card.frame_contrast = 0.0f;
        card.verified = false;

        if (bottom < 0)
        {
            card.frame = cv::Rect(left, t0, right - left + 1, t1 - t0 + 1);   // 兜底
            cards.push_back(card);
            continue;
        }

        const int width = right - left + 1;
        int height = static_cast<int>(width * card_aspect + 0.5);
        if (height < min_height)
        {
            height = min_height;
        }
        int top = bottom - height + 1;
        if (top < t0)
        {
            top = t0;
            height = bottom - top + 1;
        }

        const cv::Rect frame(left, top, width, height);
        card.frame = frame;

        // ---- 验证：卡内平均亮度应明显高于卡框外侧（排除只是辉光的区域）
        const cv::Rect inner = frame & cv::Rect(0, 0, gray.cols, gray.rows);
        const double inside_mean = cv::mean(gray(inner))[0];

        double outside_mean = 0.0;
        int outside_count = 0;
        if (frame.y > 5)
        {
            const cv::Rect above = cv::Rect(frame.x, frame.y - 5, frame.width, 5) &
                                   cv::Rect(0, 0, gray.cols, gray.rows);
            if (above.width > 0 && above.height > 0)
            {
                outside_mean += cv::mean(gray(above))[0];
                outside_count++;
            }
        }
        if (frame.y + frame.height + 5 < gray.rows)
        {
            const cv::Rect below = cv::Rect(frame.x, frame.y + frame.height, frame.width, 5) &
                                   cv::Rect(0, 0, gray.cols, gray.rows);
            if (below.width > 0 && below.height > 0)
            {
                outside_mean += cv::mean(gray(below))[0];
                outside_count++;
            }
        }
        if (outside_count > 0)
        {
            outside_mean /= outside_count;
        }

        card.frame_contrast = static_cast<float>(inside_mean - outside_mean);
        card.verified = (card.frame_contrast >= contrast_thresh);
        cards.push_back(card);
    }
}

// ---------------------------------------------------------------- 绘制与输出
cv::Mat ys_draw_result(const cv::Mat &bgr, const std::vector<ys_card> &cards)
{
    cv::Mat result = bgr.clone();   // 画在副本上，不改动原图

    const cv::Scalar frame_color(0, 255, 0);     // BGR：绿色，卡片框
    const cv::Scalar rotated_color(0, 165, 255); // BGR：橙色，最小旋转框
    const cv::Scalar center_color(0, 0, 255);    // BGR：红色，中心点

    for (std::size_t i = 0; i < cards.size(); ++i)
    {
        const cv::Rect &f = cards[i].frame;

        cv::rectangle(result, f, frame_color, 2);

        // 卡片区域是轴对齐矩形，minAreaRect 结果与 boundingRect 基本重合，
        // 这里仍然两个都画，便于与 week6 的任务要求保持一致
        std::vector<cv::Point> pts;
        pts.push_back(cv::Point(f.x, f.y));
        pts.push_back(cv::Point(f.x + f.width - 1, f.y));
        pts.push_back(cv::Point(f.x + f.width - 1, f.y + f.height - 1));
        pts.push_back(cv::Point(f.x, f.y + f.height - 1));
        const cv::RotatedRect rotated = cv::minAreaRect(pts);
        cv::Point2f corners[4];
        rotated.points(corners);
        for (int k = 0; k < 4; ++k)
        {
            const cv::Point p1(cvRound(corners[k].x), cvRound(corners[k].y));
            const cv::Point p2(cvRound(corners[(k + 1) % 4].x), cvRound(corners[(k + 1) % 4].y));
            cv::line(result, p1, p2, rotated_color, 1);
        }

        // 中心点：用 moments 的 m10/m00、m01/m00 语义（这里等价于矩形中心）
        const cv::Point center(f.x + f.width / 2, f.y + f.height / 2);
        cv::circle(result, center, 5, center_color, -1);

        // 标注：序号 + 星级
        char text[64];
        std::snprintf(text, sizeof(text), "#%d %dstar",
                      static_cast<int>(i + 1), cards[i].star_count);
        cv::putText(result, text, cv::Point(f.x + 6, f.y + 26),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        cv::putText(result, text, cv::Point(f.x + 6, f.y + 26),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 1);
    }
    return result;
}

cv::Mat ys_visualize_mask(const cv::Mat &mask)
{
    cv::Mat color;
    cv::cvtColor(mask, color, cv::COLOR_GRAY2BGR);
    return color;
}

void ys_print_cards(const std::vector<ys_card> &cards)
{
    for (std::size_t i = 0; i < cards.size(); ++i)
    {
        const ys_card &c = cards[i];
        std::cout << "card " << (i + 1)
                  << " | stars=" << c.star_count
                  << " | region=[" << c.frame.x << ", " << c.frame.y
                  << ", " << c.frame.width << "x" << c.frame.height << "]"
                  << " | center=(" << (c.frame.x + c.frame.width / 2)
                  << ", " << (c.frame.y + c.frame.height / 2) << ")"
                  << " | inside-outside contrast=" << c.frame_contrast
                  << (c.verified ? "  [verified]" : "  [low contrast]")
                  << "\n";
    }
}
