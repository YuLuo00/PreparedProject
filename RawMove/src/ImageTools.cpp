#include <Windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <Windows.h>
#include <opencv2/opencv.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/highgui.hpp>

#include "CommonTool.h"

using namespace cv;
using namespace std;

// ------------------------------------------------------------
// 功能 1: 截取屏幕指定区域并保存为图片
// ------------------------------------------------------------
bool CaptureScreenRect(const RECT& rect, const std::wstring& savePath)
{
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HDC hScreen = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hMemDC, hBitmap);

    // 将屏幕指定区域拷贝到内存 DC 中
    BitBlt(hMemDC, 0, 0, width, height, hScreen, rect.left, rect.top, SRCCOPY);

    // 转换为 OpenCV Mat
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    Mat mat(height, width, CV_8UC4);
    GetDIBits(hMemDC, hBitmap, 0, height, mat.data, (BITMAPINFO*)&bmp, DIB_RGB_COLORS);

    // OpenCV 默认是 BGR，不需要 alpha 通道
    Mat mat_bgr;
    cvtColor(mat, mat_bgr, COLOR_BGRA2BGR);

    bool ok = imwrite(cv::String(savePath.begin(), savePath.end()), mat_bgr);

    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreen);

    return ok;
}

// ------------------------------------------------------------
// 功能 2: 模板匹配（返回所有匹配坐标）
// ------------------------------------------------------------
std::vector<cv::Point> FindTemplateMatches(const cv::Mat& src, const cv::Mat& templ, double threshold = 0.9)
{

    cv::Mat result;
    cv::Mat srcGray, templGray;

    cv::imwrite("check.png", src);
    cv::Mat test = cv::imread("check.png", cv::IMREAD_UNCHANGED);
    std::wcout << L"test.type = " << test.type() << L" channels = " << test.channels() << std::endl;

    // 如果是彩色图，统一转成灰度
    if (src.channels() == 3) {
        cv::cvtColor(src, srcGray, cv::COLOR_BGR2GRAY);
    }
    else if (src.channels() == 4) {
        cv::cvtColor(src, srcGray, cv::COLOR_BGRA2GRAY);
    }
    else {
        srcGray = src;
    }

    if (templ.channels() == 3) {
        cv::cvtColor(templ, templGray, cv::COLOR_BGR2GRAY);
    }
    else if (templ.channels() == 4) {
        cv::cvtColor(templ, templGray, cv::COLOR_BGRA2GRAY);
    }
    else {
        templGray = templ;
    }

    // 转成 8U，保证类型一致
    srcGray.convertTo(srcGray, CV_8U);
    templGray.convertTo(templGray, CV_8U);

    // 现在再调用
    cv::matchTemplate(srcGray, templGray, result, cv::TM_CCOEFF_NORMED);



    std::vector<cv::Point> points;
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;

    while (true)
    {
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        if (maxVal < threshold)
            break;

        points.push_back(maxLoc);

        // 为防止重复匹配，在匹配位置附近填充黑色
        cv::rectangle(result, Rect(maxLoc.x, maxLoc.y, templ.cols, templ.rows), Scalar(0), -1);
    }

    return points;
}

// ------------------------------------------------------------
// 示例入口
// ------------------------------------------------------------
int main1()
{
    RECT rect = { 100, 100, 600, 400 };
    std::wstring path = L"D:\\capture.png";

    if (CaptureScreenRect(rect, path))
        wcout << L"截图保存成功：" << path << endl;
    else
        wcout << L"截图失败" << endl;

    // 读取原图与模板
    cv::Mat src = imread("D:\\capture.png");
    cv::Mat templ = imread("D:\\template.png");

    if (src.empty() || templ.empty()) {
        cerr << "图片加载失败！" << endl;
        return -1;
    }

    auto points = FindTemplateMatches(src, templ, 0.9);
    cout << "找到 " << points.size() << " 个匹配位置：" << endl;

    for (auto& p : points)
        cout << " (" << p.x << ", " << p.y << ")" << endl;

    return 0;
}

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <windows.h>

// 已有函数
bool CaptureScreenRect(const RECT &rect, const std::wstring &savePath);
std::vector<cv::Point> FindTemplateMatches(const cv::Mat &src, const cv::Mat &templ, double threshold);

// ============================================================
// 1️⃣ 基于 HWND 的截图函数重载
// ============================================================

// 直接保存窗口截图到文件
bool CaptureWindow(HWND hWnd, const std::wstring &savePath)
{
    if (!::IsWindow(hWnd))
        return false;

    RECT rect{};
    if (!::GetWindowRect(hWnd, &rect))
        return false;

    return CaptureScreenRect(rect, savePath);
}

#include <fstream>
#include <windows.h>

bool CaptureWindowToBMP(HWND hWnd, const wchar_t *filename)
{
    RECT rect;
    if (!GetWindowRect(hWnd, &rect))
        return false;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
    HGDIOBJ old = SelectObject(hdcMem, hBmp);

    // 把窗口内容拷贝到内存 DC
    PrintWindow(hWnd, hdcMem, PW_CLIENTONLY);

    // 准备 DIB 信息
    BITMAP bmp;
    GetObject(hBmp, sizeof(bmp), &bmp);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = -bmp.bmHeight; // 负数表示 top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    int imageSize = bmp.bmWidth * bmp.bmHeight * 4;
    std::vector<BYTE> pixels(imageSize);

    GetDIBits(hdcMem, hBmp, 0, bmp.bmHeight, pixels.data(), reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS);

    // 写 BMP 文件
    BITMAPFILEHEADER bfh{};
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imageSize;


    {
        std::wcout << L"Use OpenCL: " << (cv::ocl::useOpenCL() ? L"true" : L"false") << std::endl;

        width = 1320;
        height = 824;
        cv::Mat mat(height, width, CV_8UC4, cv::Scalar(255, 255, 255, 255));
        //cv::imwrite("_iii---.png", mat);
        std::wcout << L"isContinuous: " << (mat.isContinuous() ? L"true" : L"false") << std::endl;
        std::wcout << L"step: " << mat.step << std::endl;
        std::wcout << L"elemSize: " << mat.elemSize() << std::endl;
        std::wcout << L"total bytes: " << (mat.total() * mat.elemSize()) << std::endl;

        cv::Mat bgr;
            cv::cvtColor(mat.clone(), bgr, cv::COLOR_RGBA2BGR);
        //cv::imwrite("_iii.png", bgr);
    }



    std::ofstream ofs(filename, std::ios::binary);
    ofs.write(reinterpret_cast<const char *>(&bfh), sizeof(bfh));
    ofs.write(reinterpret_cast<const char *>(&bi), sizeof(bi));
    ofs.write(reinterpret_cast<const char *>(pixels.data()), imageSize);
    ofs.close();
    // 清理
    SelectObject(hdcMem, old);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return true;
}


// 截图到 cv::Mat（不保存文件）
bool CaptureWindowToMat(HWND hWnd, cv::Mat &outImage)
{

    RECT rect{};
    if (hWnd == nullptr) {
        rect.left = 0;
        rect.top = 0;
        rect.right = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);

            DEVMODE dm{};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
            rect.left = 0;
            rect.top = 0;
            rect.right = dm.dmPelsWidth;
            rect.bottom = dm.dmPelsHeight;
        }
    }
    else {
        if (!::IsWindow(hWnd))
            return false;
        if (!::GetWindowRect(hWnd, &rect))
            return false;
    }

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HDC hdcScreen = ::GetDC(nullptr);
    HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = ::CreateCompatibleBitmap(hdcScreen, width, height);
    HGDIOBJ oldObj = ::SelectObject(hdcMem, hBitmap);

    if (!::PrintWindow(hWnd, hdcMem, PW_CLIENTONLY)) {
        // 如果 PrintWindow 失败，尝试 BitBlt
        HDC hdcWnd = ::GetDC(hWnd);
        ::BitBlt(hdcMem, 0, 0, width, height, hdcWnd, 0, 0, SRCCOPY);
        ::ReleaseDC(hWnd, hdcWnd);
    }

    BITMAP bmp{};
    ::GetObject(hBitmap, sizeof(BITMAP), &bmp);

    cv::Mat mat(bmp.bmHeight, bmp.bmWidth, CV_8UC4);
    GetBitmapBits(hBitmap, bmp.bmHeight * bmp.bmWidthBytes, mat.data);
    cv::cvtColor(mat, outImage, cv::COLOR_BGRA2BGR);

    ::SelectObject(hdcMem, oldObj);
    ::DeleteObject(hBitmap);
    ::DeleteDC(hdcMem);
    ::ReleaseDC(nullptr, hdcScreen);

    return true;
}

// ============================================================
// 2️⃣ 基于 HWND 的截图 + 模板匹配函数
// ============================================================

// 模板参数是 cv::Mat
std::vector<cv::Point> FindTemplateInWindow(HWND hWnd, const cv::Mat &templ, double threshold)
{
    cv::Mat windowImage;
    if (!CaptureWindowToMat(hWnd, windowImage))
        return {};

    try {
        cv::imwrite("./temp_img.png", windowImage);
    }
    catch (const cv::Exception &e) {
        std::cerr << "保存图像失败: " << e.what() << std::endl;
    }

    return FindTemplateMatches(windowImage, templ, threshold);
}

// 模板参数是图片路径
std::vector<cv::Point> FindTemplateInWindow(HWND hWnd, const std::wstring &templPath, double threshold)
{
    std::string pathUtf8 = CommonTool::Wstr2Local(templPath);
    cv::Mat templ = cv::imread(pathUtf8, cv::IMREAD_COLOR);
    if (templ.empty())
        return {};
    cv::Mat bgr;
    cv::cvtColor(templ, bgr, cv::COLOR_BGRA2BGR);
    return FindTemplateInWindow(hWnd, templ, threshold);
}

// ============================================================
// 用法示例
// ============================================================
/*
HWND hwnd = FindWindow(nullptr, L"记事本");
CaptureWindow(hwnd, L"C:\\temp\\notepad.png");

cv::Mat templ = cv::imread("C:\\temp\\button.png");
auto points = FindTemplateInWindow(hwnd, templ, 0.95);

for (auto &pt : points)
    std::wcout << L"Found at: (" << pt.x << L"," << pt.y << L")\n";
*/

std::vector<cv::Point> FindTemplateCentersInWindow(HWND hWnd, const wchar_t *templPath, double threshold = 0.9)
{
    std::vector<cv::Point> topLeftPoints = FindTemplateInWindow(hWnd, templPath, threshold);

    // 读取模板尺寸
   cv::Mat templ = cv::imread(CommonTool::Wstr2Utf8(templPath), cv::IMREAD_UNCHANGED);
    if (templ.empty()) {
        std::wcerr << L"无法读取模板图像：" << templPath << std::endl;
        return {};
    }

    std::vector<cv::Point> centers;
    for (const auto &pt : topLeftPoints) {
        // 计算模板中心点
        //cv::Point center(pt.x + templ.cols / 2, pt.y + templ.rows / 2);
        cv::Point center(pt.x + 10, pt.y + 10);
        centers.push_back(center);
    }

    return centers;
}

void FlashRedRect(const cv::Rect &rect, int duration_ms)
{
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // 创建一个全屏黑色背景的窗口
    cv::Mat canvas(screenH, screenW, CV_8UC3, cv::Scalar(0, 0, 0));

    // 绘制红色矩形
    cv::rectangle(canvas, rect, cv::Scalar(0, 0, 255), 3);

    // 显示窗口
    cv::namedWindow("Flash", cv::WINDOW_NORMAL);
    cv::setWindowProperty("Flash", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
    cv::imshow("Flash", canvas);

    // 保持一段时间
    cv::waitKey(duration_ms);

    // 关闭窗口
    cv::destroyWindow("Flash");
}


// =======================================================
// 将字符串拷贝到剪切板
// =======================================================
void CopyTextToClipboard(const std::wstring &text)
{
    if (!OpenClipboard(nullptr))
        return;
    EmptyClipboard();

    size_t sizeInBytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
    if (!hGlobal) {
        CloseClipboard();
        return;
    }

    void *pData = GlobalLock(hGlobal);
    memcpy(pData, text.c_str(), sizeInBytes);
    GlobalUnlock(hGlobal);

    SetClipboardData(CF_UNICODETEXT, hGlobal);
    CloseClipboard();
}

// =======================================================
// 模拟 Ctrl+V 粘贴
// =======================================================
void SimulateCtrlV()
{
    INPUT inputs[4] = {};

    // Ctrl 按下
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    // V 按下
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    // V 抬起
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // Ctrl 抬起
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}

// =======================================================
// 模拟 Ctrl+V 粘贴
// =======================================================
void SimulateKey(std::vector<DWORD> keys)
{
    int size = static_cast<int>(keys.size());
    std::vector<INPUT> inputVec(size * 2);

    // 按下
    for (int i = 0; i < size; i++) {
        inputVec[i].type = INPUT_KEYBOARD;
        inputVec[i].ki.wVk = keys[i];
        inputVec[i].ki.dwFlags = 0;
    }

    // 抬起（反向）
    for (int i = 0; i < size; i++) {
        inputVec[size + i].type = INPUT_KEYBOARD;
        inputVec[size + i].ki.wVk = keys[size - 1 - i];
        inputVec[size + i].ki.dwFlags = KEYEVENTF_KEYUP;
    }

    // 发送输入
    SendInput(static_cast<UINT>(inputVec.size()), inputVec.data(), sizeof(INPUT));
}

void SimulateCtrlW()
{
    INPUT inputs[4] = {};

    // Ctrl 按下
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    // W 按下
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'W';

    // W 抬起
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'W';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // Ctrl 抬起
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}

// =======================================================
// 高层封装：拷贝 + 模拟粘贴
// =======================================================
void PasteText(const std::wstring &text)
{
    CopyTextToClipboard(text);
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 等剪切板稳定
    SimulateCtrlV();
}