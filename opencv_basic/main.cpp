#include"image_basic.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;


int main()
{
    // 1.加载图片
    std::string path = "input.jpg";
    cv::Mat img = loadimage(path);
    if(img.empty())
    {
        std::cerr << "[ERROR] 图片读取失败，程序退出\n";
        return -1;
    }

    // 打印图片基础信息
    outputinformation(img);

    // 确保输出目录存在
    outputdirectory(OUTPUT_DIR);

    // 2.颜色转换
    cv::Mat gray = toGray(img);
    cv::Mat hsv = toHSV(img);

    // 3.ROI裁剪
    int cx = img.cols / 4;
    int cy = img.rows / 4;
    int roiWidth = img.cols / 2;
    int roiHeight = img.rows / 2;
    cv::Mat ROI = cropROI(img, cx, cy, roiWidth, roiHeight);

    // 4.缩放
    double scaleFactor = 0.6;
    cv::Mat scaled = resizescale(img, scaleFactor);

    // 5.绘图（原图副本上画）
    cv::Mat copy1 = drawshapes(img);

    //保存全部到output文件夹
    saveimage(std::string(OUTPUT_DIR) + "/gray.png", gray);
    saveimage(std::string(OUTPUT_DIR) + "/hsv.png", hsv);

    if(!ROI.empty())
    {
        saveimage(std::string(OUTPUT_DIR) + "/roi.png", ROI);
    }
    if(!scaled.empty())
    {
        saveimage(std::string(OUTPUT_DIR) + "/resized.png", scaled);
    }
    saveimage(std::string(OUTPUT_DIR) + "/result.png", copy1);
    
    saveimage(path, img);
    
    // 窗口显示
    std::cout << "\nShowing windows, press any key to close them...\n";
    cv::imshow("Original", img);
    cv::imshow("Gray", gray);
    cv::imshow("HSV", hsv);
    if(!ROI.empty())
    {
        cv::imshow("ROI", ROI);
    }
    if(!scaled.empty())
    {
        cv::imshow("scale", scaled);
    }
    cv::imshow("copy1", copy1);
    cv::waitKey(0);
    cv::destroyAllWindows();
    return 0;
    std::cout << "\n===== Processing finished =====\n";
    std::cout << "Output files are in: " << OUTPUT_DIR << "/\n";
    return 0;
}