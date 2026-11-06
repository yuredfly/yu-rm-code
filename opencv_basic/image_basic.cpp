#include "image_basic.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

cv::Mat loadimage(const std::string &path)
{
    cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
    if (img.empty())
    {
        std::cerr << "[ERROR] Cannot read image: " << path << "\n";
        std::cerr << "        Check the file name, extension and current working directory.\n";
    }
    return img;
}

void outputinformation(const cv::Mat &img)
{
    std::cout << "Image width:    " << img.cols << "\n";
    std::cout << "Image height:   " << img.rows << "\n";
    std::cout << "Channels:       " << img.channels() << "\n";
    std::cout << "Size (w x h):   " << img.size() << "\n";
}

cv::Mat toGray(const cv::Mat &img)
{
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat toHSV(const cv::Mat &img)
{
    cv::Mat hsv;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    return hsv;
}

cv::Mat cropROI(const cv::Mat &img, int x, int y, int roiWidth, int roiHeight)
{
    int w = std::min(roiWidth, img.cols);
    int h = std::min(roiHeight, img.rows);
    if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
        x + w > img.cols || y + h > img.rows)
    {
        std::cerr << "[ERROR] ROI out of image bounds: x=" << x << " y=" << y
                  << " w=" << w << " h=" << h << "\n";
        return cv::Mat();
    }

    cv::Rect roi(x, y, w, h);
    return img(roi).clone();
}

cv::Mat resizescale(const cv::Mat& img, double scale)
{
    if (scale <= 0)
    {
        std::cerr << "[ERROR] Scale factor must be > 0, current scale=" << scale << "\n";
        return cv::Mat();
    }
    cv::Mat resized;
    int interp = (scale < 1.0) ? cv::INTER_AREA : cv::INTER_LINEAR;
    cv::resize(img, resized, cv::Size(), scale, scale, interp);
    return resized;
}

cv::Mat drawshapes(const cv::Mat& img)
{
    cv::Mat clone1 = img.clone();
    
    const cv::Point center(img.cols / 2, img.rows / 2);
    int minDim = std::min(img.cols, img.rows);
    
    cv::rectangle
    (
        clone1,
        cv::Rect(img.cols / 8, img.rows / 8,
                 img.cols * 3 / 4, img.rows * 3 / 4),
        cv::Scalar(0, 255, 0),  //BGR；绿色
        2
    );

    int radius = std::max(2, minDim / 12);
    cv::circle
    (   
        clone1,
        center,
        radius,
        cv::Scalar(0, 0, 255),  // BGR: 红色
        2
    );

    cv::line
    (
        clone1,
        cv::Point(img.cols / 8, img.rows / 8),
        cv::Point(img.cols * 7 / 8, img.rows * 7 / 8),
        cv::Scalar(255, 0, 0),  // BGR: 蓝色
        2
    );

    int textY = std::max(20, img.rows / 10);
    cv::putText
    (
        clone1,
        "OpenCV Basic",
        cv::Point(10, textY),
        cv::FONT_HERSHEY_SIMPLEX,
        0.8,
        cv::Scalar(255, 255, 255),  // BGR: 白色
        2
    );

    return clone1;
}

bool saveimage(const std::string& path, const cv::Mat& img)
{
    if (img.empty())
    {
        std::cerr << "[ERROR] Image is empty, cannot save: " << path << "\n";
        return false;
    }
    if (!cv::imwrite(path, img))
    {
        std::cerr << "[ERROR] Failed to save: " << path << "\n";
        std::cerr << "        Check that the output directory exists, is writable, and the extension is supported.\n";
        return false;
    }
    std::cout << "[SAVED] " << path << "\n";
    return true;
}

bool outputdirectory(const std::string& dir)
{
    if (fs::exists(dir))
    {
        return true;
    }
    if (fs::create_directories(dir))
    {
        std::cout << "[MKDIR] " << dir << "\n";
        return true;
    }
    std::cerr << "[ERROR] Cannot create output directory: " << dir << "\n";
    return false;
}