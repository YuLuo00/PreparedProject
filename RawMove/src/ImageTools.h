#ifndef IMAGETOOLS_H
#define IMAGETOOLS_H

#include <Windows.h>

#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

bool CaptureScreenRect(const RECT &rect, const std::wstring &savePath);
std::vector<cv::Point> FindTemplateMatches(const cv::Mat &src, const cv::Mat &templ, double threshold = 0.9);

// ============================================================
// 基于 HWND 的扩展接口声明
// ============================================================

// 1️⃣ 基于窗口句柄截图
bool CaptureWindow(HWND hWnd, const std::wstring &savePath);
bool CaptureWindowToMat(HWND hWnd, cv::Mat &outImage);

// 2️⃣ 基于窗口句柄截图并做模板匹配
std::vector<cv::Point> FindTemplateInWindow(HWND hWnd, const cv::Mat &templ, double threshold = 0.9);
std::vector<cv::Point> FindTemplateInWindow(HWND hWnd, const std::wstring &templPath, double threshold = 0.9);
inline std::vector<cv::Point> FindTemplateInWindow(HWND hWnd, const wchar_t *templPath, double threshold = 0.9)
{
    return FindTemplateInWindow(hWnd, std::wstring(templPath), threshold);
}

#endif // !IMAGETOOLS_H