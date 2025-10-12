#include "AutoWins.h"
#include <iostream>
#include <shlobj.h>
#include <string>
#include <windows.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <windows.h>

#include <UIAutomation.h>
#include <Windows.h>
#include <atlbase.h> // CComBSTR
#include <comdef.h>  // For _bstr_t
#include <iostream>

#include <fcntl.h>
#include <io.h>

#include <UIAutomation.h>
#include <Windows.h>
#include <atlbase.h>   // for CComPtr
#include <atlcomcli.h> // for CComQIPtr, etc.
#include <comdef.h>    // for _bstr_t
#include <iostream>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uiautomationcore.lib")

#include "ImageTools.h"

bool FillUIAElementInfo(IUIAutomation *pAutomation, IUIAutomationElement *pElement, UIAElementInfo &info)
{
    if (!pAutomation || !pElement)
        return false;

    HRESULT hr;

    // Name
    CComBSTR bstrName;
    if (SUCCEEDED(pElement->get_CurrentName(&bstrName)))
        info.Name = bstrName ? bstrName.m_str : L"";

    // ControlType
    hr = pElement->get_CurrentControlType(&info.ControlType);

    // LocalizedControlType
    CComBSTR bstrLocalizedType;
    if (SUCCEEDED(pElement->get_CurrentLocalizedControlType(&bstrLocalizedType)))
        info.LocalizedControlType = bstrLocalizedType ? bstrLocalizedType.m_str : L"";

    // BoundingRectangle
    RECT rect;
    if (SUCCEEDED(pElement->get_CurrentBoundingRectangle(&rect))) {
        info.BoundingRectangle.l = rect.left;
        info.BoundingRectangle.t = rect.top;
        info.BoundingRectangle.r = rect.right;
        info.BoundingRectangle.b = rect.bottom;
    }

    // Basic bool properties
    pElement->get_CurrentIsEnabled(&info.IsEnabled);
    pElement->get_CurrentIsKeyboardFocusable(&info.IsKeyboardFocusable);
    pElement->get_CurrentHasKeyboardFocus(&info.HasKeyboardFocus);

    // ProcessId
    pElement->get_CurrentProcessId(&info.ProcessId);

    // RuntimeId
    SAFEARRAY *pRuntimeId = nullptr;
    if (SUCCEEDED(pElement->GetRuntimeId(&pRuntimeId)) && pRuntimeId) {
        LONG *pData = nullptr;
        SafeArrayAccessData(pRuntimeId, (void **)&pData);
        if (pData)
            info.RuntimeId = (double)pData[0]; // 简化示例，只取第一个元素
        SafeArrayUnaccessData(pRuntimeId);
        SafeArrayDestroy(pRuntimeId);
    }

    // FrameworkId
    CComBSTR bstrFramework;
    if (SUCCEEDED(pElement->get_CurrentFrameworkId(&bstrFramework)))
        info.FrameworkId = bstrFramework ? bstrFramework.m_str : L"";

    // ClassName
    CComBSTR bstrClass;
    if (SUCCEEDED(pElement->get_CurrentClassName(&bstrClass)))
        info.ClassName = bstrClass ? bstrClass.m_str : L"";

    // NativeWindowHandle
    info.NativeWindowHandle = nullptr;
    pElement->get_CurrentNativeWindowHandle((UIA_HWND *)&info.NativeWindowHandle);

    // IsControlElement, IsContentElement
    pElement->get_CurrentIsControlElement(&info.IsControlElement);
    pElement->get_CurrentIsContentElement(&info.IsContentElement);

    // Window pattern flags
    CComPtr<IUIAutomationWindowPattern> pWindowPattern;
    if (SUCCEEDED(pElement->GetCurrentPatternAs(UIA_WindowPatternId, IID_PPV_ARGS(&pWindowPattern))) &&
        pWindowPattern) {
        pWindowPattern->get_CurrentCanMaximize(&info.CanMaximize);
        pWindowPattern->get_CurrentCanMinimize(&info.CanMinimize);
        pWindowPattern->get_CurrentIsModal(&info.IsModal);
        pWindowPattern->get_CurrentIsTopmost(&info.IsTopmost);
        pWindowPattern->get_CurrentWindowInteractionState((WindowInteractionState *)&info.WindowInteractionState);
        pWindowPattern->get_CurrentWindowVisualState((WindowVisualState *)&info.WindowVisualState);
    }

    // IsDialog
    CComVariant v;
    //info.IsDialog = FALSE;
    if (SUCCEEDED(pElement->GetCurrentPropertyValue(UIA_IsDialogPropertyId, &v))) {
        info.IsDialog = (v.boolVal != VARIANT_FALSE);
    }

    // Pattern availability flags
    CComPtr<IUIAutomationInvokePattern> pInvoke;
    info.IsInvokePatternAvailable =
        SUCCEEDED(pElement->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&pInvoke))) && pInvoke != nullptr;

    CComPtr<IUIAutomationValuePattern> pValue;
    info.IsValuePatternAvailable =
        SUCCEEDED(pElement->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pValue))) && pValue != nullptr;

    // 其他 Pattern 同理
    // PatternAvailable Flags
    auto CheckPattern = [&](auto patternId, auto &outFlag) {
        CComPtr<IUnknown> pPattern;
        outFlag = SUCCEEDED(pElement->GetCurrentPattern(patternId, &pPattern)) && pPattern != nullptr;
    };

    CheckPattern(UIA_InvokePatternId, info.IsInvokePatternAvailable);
    CheckPattern(UIA_ValuePatternId, info.IsValuePatternAvailable);
    CheckPattern(UIA_SelectionPatternId, info.IsSelectionPatternAvailable);
    CheckPattern(UIA_LegacyIAccessiblePatternId, info.IsLegacyIAccessiblePatternAvailable);
    CheckPattern(UIA_WindowPatternId, info.IsWindowPatternAvailable);
    CheckPattern(UIA_TextPatternId, info.IsTextPatternAvailable);
    CheckPattern(UIA_TextPattern2Id, info.IsTextPattern2Available);
    CheckPattern(UIA_TransformPatternId, info.IsTransformPatternAvailable);
    //CheckPattern(UIA_Transform2PatternId, info.IsTransform2PatternAvailable);
    CheckPattern(UIA_TogglePatternId, info.IsTogglePatternAvailable);
    CheckPattern(UIA_RangeValuePatternId, info.IsRangeValuePatternAvailable);
    CheckPattern(UIA_ScrollPatternId, info.IsScrollPatternAvailable);
    CheckPattern(UIA_SelectionItemPatternId, info.IsSelectionItemPatternAvailable);
    CheckPattern(UIA_MultipleViewPatternId, info.IsMultipleViewPatternAvailable);
    CheckPattern(UIA_GridPatternId, info.IsGridPatternAvailable);
    CheckPattern(UIA_GridItemPatternId, info.IsGridItemPatternAvailable);
    CheckPattern(UIA_TablePatternId, info.IsTablePatternAvailable);
    CheckPattern(UIA_TableItemPatternId, info.IsTableItemPatternAvailable);
    CheckPattern(UIA_DockPatternId, info.IsDockPatternAvailable);
    CheckPattern(UIA_DropTargetPatternId, info.IsDropTargetPatternAvailable);
    CheckPattern(UIA_ExpandCollapsePatternId, info.IsExpandCollapsePatternAvailable);
    CheckPattern(UIA_SynchronizedInputPatternId, info.IsSynchronizedInputPatternAvailable);
    CheckPattern(UIA_StylesPatternId, info.IsStylesPatternAvailable);
    CheckPattern(UIA_SpreadsheetPatternId, info.IsSpreadsheetPatternAvailable);
    CheckPattern(UIA_SpreadsheetItemPatternId, info.IsSpreadsheetItemPatternAvailable);
    CheckPattern(UIA_ItemContainerPatternId, info.IsItemContainerPatternAvailable);
    CheckPattern(UIA_TextChildPatternId, info.IsTextChildPatternAvailable);
    CheckPattern(UIA_TextEditPatternId, info.IsTextEditPatternAvailable);
    CheckPattern(UIA_AnnotationPatternId, info.IsAnnotationPatternAvailable);
    CheckPattern(UIA_CustomNavigationPatternId, info.IsCustomNavigationPatternAvailable);
    CheckPattern(UIA_VirtualizedItemPatternId, info.IsVirtualizedItemPatternAvailable);
    return true;
}

std::wstring TakeVariantWstr(VARIANT &var)
{
    std::wstring wstr;
    if (var.bstrVal != nullptr) {
        wstr = var.vt == VT_BSTR ? var.bstrVal : L"";
        VariantClear(&var);
    }
    return wstr;
}

// 递归打印控件信息
void PrintElementInfo(IUIAutomation *pAutomation, IUIAutomationElement *pElement, int indent, UIAElementInfo &info)
{
    if (!pElement)
        return;

    FillUIAElementInfo(pAutomation, pElement, info);

    VARIANT var;
    HRESULT hr;

    std::wstring indentStr(indent * 2, L' ');

    // 获取 Name
    hr = pElement->GetCurrentPropertyValue(UIA_NamePropertyId, &var);
    std::wstring name = TakeVariantWstr(var);
    VariantClear(&var);

    // 获取 AutomationId
    hr = pElement->GetCurrentPropertyValue(UIA_AutomationIdPropertyId, &var);
    std::wstring automationId = TakeVariantWstr(var);
    VariantClear(&var);

    // 获取 ControlType
    hr = pElement->GetCurrentPropertyValue(UIA_ControlTypePropertyId, &var);
    std::wstring controlType = L"";
    if (hr == S_OK && var.vt == VT_I4) {
        long typeId = var.lVal;
        switch (typeId) {
            case UIA_ButtonControlTypeId:
                controlType = L"Button";
                break;
            case UIA_EditControlTypeId:
                controlType = L"Edit";
                break;
            case UIA_WindowControlTypeId:
                controlType = L"Window";
                break;
            case UIA_PaneControlTypeId:
                controlType = L"Pane";
                break;
            case UIA_MenuControlTypeId:
                controlType = L"Menu";
                break;
            case UIA_MenuItemControlTypeId:
                controlType = L"MenuItem";
                break;
            case UIA_ToolBarControlTypeId:
                controlType = L"Toolbar";
                break;
            case UIA_TextControlTypeId:
                controlType = L"Text";
                break;
            default:
                controlType = L"Other(" + std::to_wstring(typeId) + L")";
                break;
        }
    }
    VariantClear(&var);

    // 获取可见状态
    hr = pElement->GetCurrentPropertyValue(UIA_IsOffscreenPropertyId, &var);
    bool isOffscreen = (hr == S_OK && var.vt == VT_BOOL) ? (var.boolVal == VARIANT_TRUE) : false;
    VariantClear(&var);

    std::wcout << indentStr << L"[Type] " << controlType << L" | [Name] " << name << L" | [AutomationId] "
               << automationId << L" | [Visible] " << (isOffscreen ? L"false" : L"true") << std::endl;

    // 遍历子元素
    CComPtr<IUIAutomationTreeWalker> pWalker;
    hr = pAutomation->get_RawViewWalker(&pWalker);
    if (FAILED(hr) || !pWalker)
        return;

    CComPtr<IUIAutomationElement> pChild;
    pWalker->GetFirstChildElement(pElement, &pChild);
    while (pChild) {
        UIAElementInfo *subInfo = new UIAElementInfo();
        info.subItems.insert(subInfo);

        PrintElementInfo(pAutomation, pChild, indent + 1, *subInfo);

        CComPtr<IUIAutomationElement> pNext;
        pWalker->GetNextSiblingElement(pChild, &pNext);
        pChild = pNext;
    }
}

// 主函数：从窗口句柄开始打印 UI 树
void EnumUIAElements(HWND hwnd, UIAElementInfo &info)
{
    {
        if (!IsWindow(hwnd)) {
            std::wcerr << L"无效的窗口句柄。" << std::endl;
            return;
        }

        CoInitialize(NULL);

        CComPtr<IUIAutomation> pAutomation;
        HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pAutomation));
        if (FAILED(hr)) {
            std::wcerr << L"无法创建 UIAutomation 实例。" << std::endl;
            CoUninitialize();
            return;
        }

        CComPtr<IUIAutomationElement> pRoot;
        hr = pAutomation->ElementFromHandle(hwnd, &pRoot);
        if (FAILED(hr)) {
            std::wcerr << L"获取根元素失败。" << std::endl;
            CoUninitialize();
            return;
        }

        std::wcout << L"=== 开始枚举 UI 控件 ===" << std::endl;
        PrintElementInfo(pAutomation, pRoot, 0, info);
        std::wcout << L"=== 枚举完成 ===" << std::endl;
    }

    CoUninitialize();
}

struct WinItem
{
    std::set<WinItem *> subItems;
    std::wstring className;
    std::wstring text;
};

BOOL CALLBACK EnumChildProcCommon(HWND hwnd, LPARAM lParam)
{
    auto *param = reinterpret_cast<std::tuple<LPARAM, std::function<BOOL(HWND, LPARAM)>> *>(lParam);
    LPARAM lp = std::get<0>(*param);
    std::function<BOOL(HWND, LPARAM)> func = std::get<1>(*param);
    return func(hwnd, lp);
}

BOOL EnumChildProcCommon(HWND hwnd, std::function<BOOL(HWND, LPARAM)> func)
{
    std::tuple<LPARAM, std::function<BOOL(HWND, LPARAM)>> cbParam;
    {
        std::get<0>(cbParam) = reinterpret_cast<LPARAM>(hwnd);
        std::get<1>(cbParam) = func;
    }
    return EnumChildProcCommon(hwnd, reinterpret_cast<LPARAM>(&cbParam));
}

// 枚举子窗口时的回调函数
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    WinItem *pParentItem = reinterpret_cast<WinItem *>(lParam);

    WinItem *item = new WinItem();
    wchar_t className[256];
    wchar_t windowText[256];

    GetClassNameW(hwnd, className, sizeof(className) / sizeof(wchar_t));
    item->className = className;
    GetWindowTextW(hwnd, windowText, sizeof(windowText) / sizeof(wchar_t));
    item->text = windowText;

    pParentItem->subItems.insert(item);
    if (IsWindowVisible(hwnd) == TRUE) {
        std::wcout << L"HWND: " << hwnd << L",\n\t Class: " << className << L",\n\t Text: " << windowText << std::endl;
    }
    if (item->text.empty() == false) {
        int i = 0;
    }

    EnumChildWindows(hwnd, EnumChildProc, reinterpret_cast<LPARAM>(item));

    return TRUE; // 继续枚举
}

//// 获取某个窗口的控件信息
//void EnumControlsInfo(HWND hWndParent, WinItem &item)
//{
//    if (!IsWindow(hWndParent)) {
//        std::wcerr << L"Invalid window handle." << std::endl;
//        return;
//    }
//
//    std::wcout << L"Enumerating child windows of: " << hWndParent << std::endl;
//    EnumChildWindows(hWndParent, EnumChildProc, reinterpret_cast<LPARAM>(&item));
//    return;
//}
//

bool Find(const UIAElementInfo &pRoot, std::function<bool(const UIAElementInfo &)> func, UIAElementInfo &ret)
{
    if (func(pRoot)) {
        ret = pRoot;
        return true;
    }

    for (UIAElementInfo *child : pRoot.subItems) {
        if (Find(*child, func, ret)) {
            return true;
        }
    }

    return false; // 没找到
}

void ClickButton(IUIAutomationElement &pButton)
{
    CComPtr<IUIAutomationInvokePattern> pInvoke;
    HRESULT hr = pButton.GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&pInvoke));

    if (SUCCEEDED(hr) && pInvoke) {
        // 调用 Invoke 方法模拟点击
        pInvoke->Invoke();
        std::wcout << L"按钮点击成功" << std::endl;
    }
    else {
        std::wcout << L"按钮不支持 InvokePattern" << std::endl;
    }
}

#include <opencv2/core/base.hpp>
#include <opencv2/core/ocl.hpp>

#include <chrono>
#include <thread>

// =====================================================
// 给定 PID，获取该进程所有窗口的 UIA 信息
// =====================================================
std::vector<UIAElementInfo> GetAllUIAInfoByPid(DWORD pid)
{
    std::vector<UIAElementInfo> result;

    // 1️⃣ 枚举属于该 PID 的所有可见窗口
    std::vector<HWND> windows;
    struct EnumData
    {
        DWORD pid;
        std::vector<HWND> *pResult;
    } data{pid, &windows};

    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            EnumData *pData = reinterpret_cast<EnumData *>(lParam);
            DWORD windowPid = 0;
            GetWindowThreadProcessId(hWnd, &windowPid);
            if (windowPid == pData->pid && IsWindowVisible(hWnd)) {
                pData->pResult->push_back(hWnd);
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&data));

    // 2️⃣ 遍历窗口，枚举 UIA 元素
    for (HWND hWnd : windows) {
        wchar_t title[256];
        GetWindowTextW(hWnd, title, 256);
        std::wcout << L"分析窗口: " << title << L" (HWND=" << hWnd << L")" << std::endl;

        UIAElementInfo info;
        EnumUIAElements(hWnd, info);

        result.push_back(std::move(info));
    }

    return result;
}

void RomoveNoisePS(const std::wstring &file)
{
    {
        std::wstring openButton = L"./_button.png";
        std::wstring fileButton = L"file_button.png";
        std::wstring fileOpenButton = L"button_file-open.png";
        std::wstring bOpenO = L"open-O.png";
        std::wstring bRemoveNoise = L"removeNoise.png";
        std::wstring bEnhance = L"Enhance.png";
        std::wstring bOpenDNG = L"openDNG.png";
        std::wstring bNoN = L"No-N.png";
        std::wstring filename_input = L"filename_input.png";
        std::wstring bExport = L"bExport.png";
        std::wstring targetProcess = L"Photoshop.exe";
        DWORD pid = GetProcessIdByName(targetProcess);
        HWND winEnhance = nullptr;
        HWND winCameraRaw = nullptr;
        HWND winExportAs = nullptr;
        HWND winSaveAs = nullptr;
        auto windows = GetVisibleWindowsByPid(pid);
        HWND winBuffer = windows[0];
        cv::Point ptBuffer;
        Tasks tasks = {
            Tasks([&]() {
                BringWindowToFront(winBuffer);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }),
            Tasks([&]() {
                SimulateKey({VK_CONTROL, 'O'});
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }),
            Tasks([&]() {
                winBuffer = nullptr;
                winBuffer = WaitForWindow(pid, L".*打开.*");
                if (winBuffer == nullptr) {
                    throw std::runtime_error("winBuffer == nullptr"); // 转成 std::string
                }
            }),
            Tasks([&]() {
                PasteText(file);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }),
            Tasks([&]() {
                SimulateKey({VK_RETURN});
                WaitForLowCpu(pid, 5);
                winCameraRaw = nullptr;
                winCameraRaw = WaitForWindow(pid, L".*Camera Raw .*");
                if (winCameraRaw == nullptr) {
                    throw std::runtime_error("winCameraRaw == nullptr"); // 转成 std::string
                }
            }),
            Tasks([&]() {
                auto pts = FindTemplateInWindow(nullptr, bRemoveNoise.c_str());
                ptBuffer = pts[0] + cv::Point(15, 15);
                ClickPoint(ptBuffer);
                winEnhance = nullptr;
                winEnhance = WaitForWindow(pid, L".*增强.*");
                if (winEnhance == nullptr) {
                    throw std::runtime_error("winEnhance == nullptr"); // 转成 std::string
                }
            }),
            Tasks([&]() {
                SimulateKey({VK_RETURN});
                std::this_thread::sleep_for(std::chrono::seconds(1));
                WaitForLowCpu(pid, 2, 2000, 200, 30 * 1000);
                WaitNoWindow(winEnhance);
            }),
            Tasks([&]() {
                SimulateKey({VK_RETURN});
                std::this_thread::sleep_for(std::chrono::seconds(1));
                WaitForLowCpu(pid, 2, 2000, 200, 30 * 1000);
                WaitNoWindow(winCameraRaw);

                HWND winSaveStatus = WaitForWindow(pid, L".*存储状态.*", 3000);
                if (winSaveStatus) {
                    WaitNoWindow(winSaveStatus, 30 * 1000);
                    WaitForLowCpu(pid, 2, 2000, 200, 30 * 1000);
                }
            }),
            Tasks([&]() {
                SimulateKey({VK_MENU, VK_SHIFT, VK_CONTROL, 'W'});

                winExportAs = nullptr;
                winExportAs = WaitForWindow(pid, L".*导出为.*");
                if (winExportAs == nullptr) {
                    throw std::runtime_error("winExportAs == nullptr"); // 转成 std::string
                }
                WaitForLowCpu(pid, 0.5, 2000, 200, 30 * 1000);
            }),
            Tasks([&]() {
                auto pts = FindTemplateInWindow(nullptr, bExport.c_str());
                ptBuffer = pts[0] + cv::Point(15, 15);
                ClickPoint(ptBuffer);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                WaitForLowCpu(pid, 0.5, 2000, 200, 30 * 1000);
                WaitNoWindow(winCameraRaw);
            }),
            Tasks([&]() {
                winSaveAs = nullptr;
                winSaveAs = WaitForWindow(pid, L".*另存为.*");
                if (winSaveAs == nullptr) {
                    throw std::runtime_error("winSaveAs == nullptr"); // 转成 std::string
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));

                SimulateKey({VK_RETURN});
                WaitForLowCpu(pid, 0.5, 2000, 200, 30 * 1000);

                HWND confirm = WaitForWindow(pid, L".*确认另存为.*");
                if (confirm) {
                    SimulateKey({'Y'});
                    WaitNoWindow(confirm);
                }

                WaitForLowCpu(pid, 0.5, 2000, 200, 30 * 1000);
                WaitNoWindow(winSaveAs);

            }),
            Tasks([&]() {}),
            Tasks([&]() {}),
            Tasks([&]() {
                SimulateCtrlW();
                std::this_thread::sleep_for(std::chrono::seconds(1));
                auto pts = FindTemplateInWindow(nullptr, bNoN.c_str());
                ptBuffer = pts[0] + cv::Point(15, 15);
                ClickPoint(ptBuffer);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }),
            Tasks(),
            Tasks(),
            Tasks(),
            Tasks(),
            Tasks(),
            Tasks(),
            Tasks(),
        };
        tasks.run(1);
        return;
    }
}

void testMain()
{
    //// 设置控制台为 UTF-8 编码
    //SetConsoleOutputCP(CP_UTF8);
    // 设置控制台为 UTF-16 编码
    int retSetMode = _setmode(_fileno(stdout), _O_U16TEXT);
    std::wcout << L"Hello 控制台" << std::endl;

    cv::ocl::setUseOpenCL(false);
    cv::ipp::setUseIPP(false);
    cv::ipp::setUseIPP_NotExact(false);

    std::wstring targetProcess = L"Photoshop.exe";
    DWORD pid = GetProcessIdByName(targetProcess);

    if (pid == 0) {
        std::wcout << L"找不到进程: " << targetProcess << L"\n";
        return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
    //{
    //std::wregex reg1(L".*增强.*", std::regex_constants::icase);
    //std::wregex reg(L"*增强*", std::regex_constants::icase);

    HWND enhanceWindow = WaitForWindow(pid, L".*增强.*");
    auto uias = GetAllUIAInfoByPid(pid);
    //}

    std::vector<std::wstring> files{
        LR"(D:\_File\a6700\100MSDCF\ALL_RAW\DSC06684.ARW)",
        LR"(D:\_File\a6700\100MSDCF\ALL_RAW\DSC06685.ARW)",
        LR"(D:\_File\a6700\100MSDCF\ALL_RAW\DSC06686.ARW)",
        LR"(D:\_File\a6700\100MSDCF\ALL_RAW\DSC06687.ARW)",
    };
    for (size_t i = 0; i < files.size(); i++) {
        RomoveNoisePS(files[i]);
    }

    return;

    std::wcout << L"找到进程 " << targetProcess << L" ，PID=" << pid << L"\n";

    auto windows = GetVisibleWindowsByPid(pid, L"Camera Raw 16*");
    windows = GetVisibleWindowsByPid(pid);
    std::wcout << L"共找到 " << windows.size() << L" 个可见窗口。\n";

    for (HWND hWnd : windows) {
        wchar_t title[256];
        GetWindowTextW(hWnd, title, 256);
        std::wcout << L"  HWND=" << hWnd << L"  标题：" << title << L"\n";
    }

    if (windows.size() != 1) {
        return;
    }
    HWND window = windows[0];
    BringWindowToFront(window);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<cv::Point> pts = FindTemplateInWindow(nullptr, L"./_button.png");

    // 例如：找到“记事本”的主窗口
    HWND hwnd = FindWindowW(L"Adobe Photoshop 2024", NULL);
    hwnd = reinterpret_cast<HWND>(0x30EB2);
    hwnd = window;
    if (!hwnd) {
        std::wcerr << L"未找到记事本窗口，请先打开一个记事本。" << std::endl;
        return;
    }
    UIAElementInfo info;
    EnumUIAElements(hwnd, info);
    UIAElementInfo openButton;

    if (Find(
            info, [](const UIAElementInfo &item) { return item.Name == L"打开"; }, openButton)) {
        //ClickButton(openButton);
    }

    return;
}

// 将指定窗口提到最前面
bool BringWindowToFront(HWND hWnd)
{
    if (hWnd == nullptr || !IsWindow(hWnd))
        return false;

    // 如果窗口被最小化，则先恢复
    if (IsIconic(hWnd))
        ShowWindow(hWnd, SW_RESTORE);

    // 置顶到最前面，并激活
    BOOL result = SetForegroundWindow(hWnd);
    if (!result) {
        // 某些情况下（比如当前线程不在前台）可能会失败，
        // 这里可以通过AttachThreadInput提升权限
        DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        DWORD thisThread = GetCurrentThreadId();
        AttachThreadInput(thisThread, fgThread, TRUE);
        result = SetForegroundWindow(hWnd);
        AttachThreadInput(thisThread, fgThread, FALSE);
    }

    // 再次确保显示
    BringWindowToTop(hWnd);
    ShowWindow(hWnd, SW_SHOW);

    return result != FALSE;
}

// 检查指定窗口是否在最前面
bool IsWindowInForeground(HWND hWnd)
{
    if (hWnd == nullptr || !IsWindow(hWnd))
        return false;

    HWND hForeground = GetForegroundWindow();
    return hWnd == hForeground;
}

#include <iostream>
#include <string>
#include <tlhelp32.h>
#include <vector>
#include <windows.h>

// 根据进程名获取 PID（不区分大小写）
DWORD GetProcessIdByName(const std::wstring &processName)
{
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            std::wstring exeName = entry.szExeFile;
            if (_wcsicmp(exeName.c_str(), processName.c_str()) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

// 枚举所有窗口，找到属于 pid 的可见顶层窗口
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hWnd, &windowPid);

    if (windowPid == (DWORD)lParam) {
        if (IsWindowVisible(hWnd)) {
            // 打印标题
            wchar_t title[256];
            GetWindowTextW(hWnd, title, 256);
            std::wcout << L"[窗口] " << title << L" (HWND=" << hWnd << L")\n";
        }
    }

    return TRUE; // 继续枚举
}

// 获取某个进程的所有可见窗口，可选正则标题过滤
std::vector<HWND> GetVisibleWindowsByPid(DWORD pid, const std::wregex *titleRegex)
{
    std::vector<HWND> result;

    struct EnumData
    {
        DWORD pid;
        const std::wregex *titleRegex;
        std::vector<HWND> *pResult;
    } data{pid, titleRegex, &result};

    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            auto *pData = reinterpret_cast<EnumData *>(lParam);
            DWORD windowPid = 0;
            GetWindowThreadProcessId(hWnd, &windowPid);

            if (windowPid != pData->pid || !IsWindowVisible(hWnd))
                return TRUE;

            // 获取窗口标题
            wchar_t title[512];
            GetWindowTextW(hWnd, title, 512);
            std::wstring titleStr = title;

            // 如果指定了正则，就匹配
            if (pData->titleRegex) {
                if (std::regex_search(titleStr, *pData->titleRegex))
                    pData->pResult->push_back(hWnd);
            }
            else {
                // 没指定正则就全部加入
                pData->pResult->push_back(hWnd);
            }

            return TRUE;
        },
        reinterpret_cast<LPARAM>(&data));

    return result;
}

void ClickPoint(const cv::Point &pt)
{
    // 移动鼠标
    SetCursorPos(pt.x, pt.y);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // 鼠标左键按下
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(50); // 保证系统能捕捉到点击

    // 鼠标左键抬起
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

    Sleep(50); // 防止连续点击过快
}

// ===================================================
// 函数2：传入匹配点，点击每个匹配中心
// ===================================================
void ClickMatchedPoints(const std::vector<cv::Point> &points, const cv::Size &templSize)
{
    for (const auto &pt : points) {
        // 计算模板中心点
        cv::Point center(pt.x + templSize.width / 2, pt.y + templSize.height / 2);
        ClickPoint(center);
    }
}

// =======================================================
// 轮询等待窗口出现
// =======================================================
HWND WaitForWindow(DWORD pid, const std::wstring &titlePattern, int timeout_ms, int interval_ms)
{
    auto start = std::chrono::steady_clock::now();
    HWND found = nullptr;

    while (true) {
        auto windows = GetVisibleWindowsByPid(pid, titlePattern);
        if (!windows.empty()) {
            found = windows[0]; // 找到第一个符合的窗口
            break;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed >= timeout_ms) {
            break; // 超时退出
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    return found; // nullptr 表示未找到
}

bool WaitNoWindow(HWND hwnd, int timeout_ms, int interval_ms)
{
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (::IsWindow(hwnd) == false) {
            return true;
        }
        if (::IsWindowVisible(hwnd) == false) {
            return true;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed >= timeout_ms) {
            break; // 超时退出
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    return false; // nullptr 表示未找到
}

double GetCpuUsageByPid(DWORD pid, int intervalMs)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        std::cerr << "无法打开进程: " << pid << std::endl;
        return -1;
    }

    FILETIME prevSysIdle, prevSysKernel, prevSysUser;
    FILETIME prevProcCreation, prevProcExit, prevProcKernel, prevProcUser;

    // 获取初始系统时间
    if (!GetSystemTimes(&prevSysIdle, &prevSysKernel, &prevSysUser)) {
        CloseHandle(hProcess);
        return -1;
    }

    // 获取初始进程时间
    if (!GetProcessTimes(hProcess, &prevProcCreation, &prevProcExit, &prevProcKernel, &prevProcUser)) {
        CloseHandle(hProcess);
        return -1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));

    FILETIME sysIdle, sysKernel, sysUser;
    FILETIME procCreation, procExit, procKernel, procUser;

    if (!GetSystemTimes(&sysIdle, &sysKernel, &sysUser) ||
        !GetProcessTimes(hProcess, &procCreation, &procExit, &procKernel, &procUser)) {
        CloseHandle(hProcess);
        return -1;
    }

    ULONGLONG sysTotal = (reinterpret_cast<ULONGLONG &>(sysKernel) + reinterpret_cast<ULONGLONG &>(sysUser)) -
                         (reinterpret_cast<ULONGLONG &>(prevSysKernel) + reinterpret_cast<ULONGLONG &>(prevSysUser));

    ULONGLONG procTotal = (reinterpret_cast<ULONGLONG &>(procKernel) + reinterpret_cast<ULONGLONG &>(procUser)) -
                          (reinterpret_cast<ULONGLONG &>(prevProcKernel) + reinterpret_cast<ULONGLONG &>(prevProcUser));

    CloseHandle(hProcess);

    if (sysTotal == 0)
        return 0.0;

    return (double)(procTotal * 100) / sysTotal;
}

bool WaitForLowCpu(DWORD pid, double threshold, int stableDurationMs, int checkIntervalMs, int timeoutMs)
{
    auto startTime = std::chrono::steady_clock::now();
    int stableTime = 0;

    while (true) {
        double cpu = GetCpuUsageByPid(pid, checkIntervalMs); // 获取CPU占用
        if (cpu < threshold) {
            stableTime += checkIntervalMs;
            if (stableTime >= stableDurationMs) {
                return true; // 连续低于阈值
            }
        }
        else {
            stableTime = 0; // CPU升高，重置计时
        }

        // 超时检查
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() > timeoutMs) {
            return false; // 超时未达到低CPU
        }
    }
}
