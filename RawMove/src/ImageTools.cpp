#include <Windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <regex>

#include <opencv2/opencv.hpp>

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

    std::wcout << L"src.empty = " << src.empty() << std::endl;
    std::wcout << L"src.type = " << src.type() << std::endl;
    std::wcout << L"src.channels = " << src.channels() << std::endl;
    std::wcout << L"src.depth = " << src.depth() << std::endl;
    std::wcout << L"src.isContinuous = " << src.isContinuous() << std::endl;
    std::wcout << L"src.step = " << src.step << L" expected = " << src.cols * src.elemSize() << std::endl;

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

// 截图到 cv::Mat（不保存文件）
bool CaptureWindowToMat(HWND hWnd, cv::Mat &outImage)
{
    if (!::IsWindow(hWnd))
        return false;

    RECT rect{};
    if (!::GetWindowRect(hWnd, &rect))
        return false;

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

    //cv::Mat mat(bmp.bmHeight, bmp.bmWidth, CV_8UC4);
    //GetBitmapBits(hBitmap, bmp.bmHeight * bmp.bmWidthBytes, mat.data);
    //cv::cvtColor(mat, outImage, cv::COLOR_BGRA2BGR);

    cv::Mat mat(bmp.bmHeight, bmp.bmWidth, CV_8UC4);
    std::vector<uchar> buffer(bmp.bmHeight * bmp.bmWidthBytes);
    GetBitmapBits(hBitmap, buffer.size(), buffer.data());

    // 注意：Mat 的 step 要和 bmWidthBytes 对齐
    cv::Mat tmp(bmp.bmHeight, bmp.bmWidth, CV_8UC4, buffer.data(), bmp.bmWidthBytes);
    tmp.copyTo(mat); // 拷贝成连续的 Mat
    mat = mat.clone();
    cv::imwrite("./_temp_tmppng.png", tmp);
    cv::imwrite("./_temp_matpng.png", mat);
    outImage = mat;
    if (false)
    {
        cv::Mat reloaded = cv::imread("./_temp_matpng.png", cv::IMREAD_UNCHANGED);
        if (reloaded.empty()) {
            std::cerr << "Failed to read image!" << std::endl;
            //return;
        }

        // 确保连续
        if (!reloaded.isContinuous()) {
            reloaded = reloaded.clone();
        }

        

        string msg = string("reloaded.size = ") + std::to_string(reloaded.cols) + "x" + std::to_string(reloaded.rows)
            +"reloaded.channels = " + std::to_string(reloaded.channels()) +
            "reloaded.type = " + std::to_string(reloaded.type());
        std::cout << msg << std::endl;
        cv::Mat outImage;
        cv::cvtColor(reloaded, outImage, cv::COLOR_RGB2BGR);
        ColorConversionCodes;
    }


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
    cv::Mat templ = cv::imread(std::string(templPath.begin(), templPath.end()), cv::IMREAD_COLOR);
    if (templ.empty())
        return {};

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
