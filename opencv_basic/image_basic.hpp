#pragma once

#include <opencv2/opencv.hpp>
#include<string>

// 读取图片
cv::Mat loadimage(const std::string& path);

// 输出图片宽度、高度、通道数
void outputinformation(const cv::Mat& img);

// 转灰度
cv::Mat toGray(const cv::Mat& img);

// 转HSV
cv::Mat toHSV(const cv::Mat& img);

// 裁剪图片ROI，带边界检查；越界时返回空 Mat
cv::Mat cropROI(const cv::Mat &img, int cx, int cy, int roiWidth, int roiHeight);

// 按比例缩放（scale > 0），返回缩放后的图
cv::Mat resizescale(const cv::Mat& img, double scale);

// 在原图副本上绘制矩形、圆、直线、文字，返回绘制后的图
cv::Mat drawshapes(const cv::Mat& img);

// 保存图片，失败返回 false
bool saveimage(const std::string& path, const cv::Mat& img);

// 创建输出目录，已存在也返回 true
bool outputdirectory(const std::string& dir);
