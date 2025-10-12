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

// =======================================================
// 新函数：返回匹配区域中心点
// =======================================================
std::vector<cv::Point> FindTemplateCentersInWindow(HWND hWnd, const wchar_t *templPath, double threshold = 0.9);

void FlashRedRect(const cv::Rect &rect, int duration_ms = 2000);


// =======================================================
// 将字符串拷贝到剪切板
// =======================================================
void CopyTextToClipboard(const std::wstring &text);

// =======================================================
// 模拟 Ctrl+V 粘贴
// =======================================================
void SimulateCtrlV();
void SimulateCtrlW();
void SimulateKey(std::vector<DWORD> keys);
    // =======================================================
// 高层封装：拷贝 + 模拟粘贴
// =======================================================
void PasteText(const std::wstring &text);


#endif // !IMAGETOOLS_H