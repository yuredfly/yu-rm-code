#include "param_loader.hpp"

#include "task_config.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <vector>

namespace task {

namespace {

// 从文本里抽出所有浮点数
std::vector<double> extract_numbers(const std::string &text)
{
    std::vector<double> out;
    static const std::regex re(R"([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)");
    for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it)
    {
        out.push_back(std::stod(it->str()));
    }
    return out;
}

// 找到 key 之后第一个 '[' 到 ']' 之间的内容
bool find_data_block(const std::string &text, const std::string &key, std::string &block)
{
    const std::size_t kpos = text.find(key);
    if (kpos == std::string::npos)
    {
        return false;
    }
    const std::size_t lb = text.find('[', kpos);
    if (lb == std::string::npos)
    {
        return false;
    }
    const std::size_t rb = text.find(']', lb);
    if (rb == std::string::npos)
    {
        return false;
    }
    block = text.substr(lb + 1, rb - lb - 1);
    return true;
}

// 找到 key 之后、指定窗口内的第一个浮点数
bool find_number_after(const std::string &text, const std::string &key, double &value)
{
    const std::size_t kpos = text.find(key);
    if (kpos == std::string::npos)
    {
        return false;
    }
    const std::string window = text.substr(kpos, std::min<std::size_t>(text.size() - kpos, 200));
    const std::vector<double> nums = extract_numbers(window);
    // 第一个数字通常是"表格里的数值"；若 key 本身含数字（如 w），跳过
    if (nums.empty())
    {
        return false;
    }
    value = nums.front();
    return true;
}

}  // namespace

camera_params load_camera_params(const std::string &path)
{
    camera_params p;

    // 兜底默认值：与《参数.md》完全一致
    p.K = (cv::Mat_<double>(3, 3) << 1781.053454, 0.0, 510.5751561,
                                     0.0, 1781.686873, 472.9735763,
                                     0.0, 0.0, 1.0);
    p.D = (cv::Mat_<double>(1, 5) << -0.04420740074, -0.1081537096,
                                     -0.0006506219039, 0.0002357380215, 0.466737247);
    p.image_width = kFallbackWidth;
    p.image_height = kFallbackHeight;
    p.armor_width_m = kArmorWidthM;
    p.armor_height_m = kArmorHeightM;

    // 中文文件名必须走 filesystem::u8path，否则 Windows 下按 GBK 打开会失败
    std::ifstream fin(std::filesystem::u8path(path.c_str()), std::ios::binary);
    if (!fin.is_open())
    {
        p.note = "cannot open params file, using built-in defaults";
        return p;
    }

    std::ostringstream ss;
    ss << fin.rdbuf();
    const std::string text = ss.str();
    fin.close();

    bool ok_k = false;
    bool ok_d = false;
    bool ok_w = false;
    bool ok_h = false;

    // ---- 相机内参
    std::string block;
    if (find_data_block(text, "camera_matrix", block))
    {
        const std::vector<double> nums = extract_numbers(block);
        if (nums.size() >= 9)
        {
            cv::Mat K = cv::Mat_<double>(3, 3);
            for (int i = 0; i < 9; ++i)
            {
                K.at<double>(i / 3, i % 3) = nums[i];
            }
            p.K = K;
            ok_k = true;
        }
    }

    // ---- 畸变系数
    if (find_data_block(text, "distortion_coefficients", block))
    {
        const std::vector<double> nums = extract_numbers(block);
        if (nums.size() >= 5)
        {
            cv::Mat D = cv::Mat_<double>(1, 5);
            for (int i = 0; i < 5; ++i)
            {
                D.at<double>(0, i) = nums[i];
            }
            p.D = D;
            ok_d = true;
        }
    }

    // ---- 图像尺寸
    double v = 0.0;
    if (find_number_after(text, "image_width", v) && v > 0)
    {
        p.image_width = static_cast<int>(v);
    }
    if (find_number_after(text, "image_height", v) && v > 0)
    {
        p.image_height = static_cast<int>(v);
    }

    // ---- 装甲板尺寸（表格行：| 模型宽度 `w` | 0.135 m（135 mm） |）
    if (find_number_after(text, "模型宽度", v) && v > 0.0 && v < 10.0)
    {
        p.armor_width_m = v;
        ok_w = true;
    }
    if (find_number_after(text, "模型高度", v) && v > 0.0 && v < 10.0)
    {
        p.armor_height_m = v;
        ok_h = true;
    }

    p.from_file = ok_k && ok_d;
    if (p.from_file)
    {
        p.note = "K and D parsed from file";
        if (!ok_w || !ok_h)
        {
            p.note += "; armor size fell back to defaults";
        }
    }
    else
    {
        p.note = "camera_matrix / distortion_coefficients not fully parsed, using defaults";
    }
    return p;
}

void print_camera_params(const camera_params &p)
{
    std::cout << "[PARAM] source: " << (p.from_file ? "参数.md" : "built-in defaults")
              << " (" << p.note << ")\n";
    std::cout << "[PARAM] image size: " << p.image_width << " x " << p.image_height << "\n";
    std::cout << "[PARAM] K = ["
              << p.K.at<double>(0, 0) << ", " << p.K.at<double>(0, 1) << ", " << p.K.at<double>(0, 2)
              << "; " << p.K.at<double>(1, 0) << ", " << p.K.at<double>(1, 1) << ", " << p.K.at<double>(1, 2)
              << "; " << p.K.at<double>(2, 0) << ", " << p.K.at<double>(2, 1) << ", " << p.K.at<double>(2, 2)
              << "]\n";
    std::cout << "[PARAM] D = [";
    for (int i = 0; i < p.D.cols; ++i)
    {
        std::cout << p.D.at<double>(0, i) << (i + 1 < p.D.cols ? ", " : "");
    }
    std::cout << "]\n";
    std::cout << "[PARAM] armor: width=" << p.armor_width_m
              << " m, height=" << p.armor_height_m << " m\n";
}

}  // namespace task
