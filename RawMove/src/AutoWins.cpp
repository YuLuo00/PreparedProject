#include "AutoWins.h"
#include <iostream>
#include <shlobj.h>
#include <string>
#include <windows.h>

#include <iostream>
#include <string>
#include <windows.h>
#include <functional>
#include <tuple>

#include <map>
#include <set>

#include <UIAutomation.h>
#include <Windows.h>
#include <comdef.h> // For _bstr_t
#include <iostream>
#include <atlbase.h> // CComBSTR

#include <fcntl.h>
#include <io.h>

#include <Windows.h>
#include <UIAutomation.h>
#include <iostream>
#include <comdef.h>      // for _bstr_t
#include <atlbase.h>     // for CComPtr
#include <atlcomcli.h>   // for CComQIPtr, etc.

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uiautomationcore.lib")


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
        PrintElementInfo(pAutomation,  pRoot, 0, info);
        std::wcout << L"=== 枚举完成 ===" << std::endl;

    }

    CoUninitialize();
}

struct WinItem
{
    std::set<WinItem*> subItems;
    std::wstring className;
    std::wstring text;
};



BOOL CALLBACK EnumChildProcCommon(HWND hwnd, LPARAM lParam)
{
    auto *param = reinterpret_cast<std::tuple<LPARAM, std::function<BOOL(HWND, LPARAM)>>*>(lParam);
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

void testMain()
{

     //// 设置控制台为 UTF-8 编码
    //SetConsoleOutputCP(CP_UTF8);
    // 设置控制台为 UTF-16 编码
    int retSetMode = _setmode(_fileno(stdout), _O_U16TEXT);
    std::wcout << L"Hello 控制台" << std::endl;


    // 例如：找到“记事本”的主窗口
    HWND hwnd = FindWindowW(L"Adobe Photoshop 2024", NULL);
    hwnd = reinterpret_cast<HWND>(0x30EB2);
    if (!hwnd) {
        std::wcerr << L"未找到记事本窗口，请先打开一个记事本。" << std::endl;
        return ;
    }
    UIAElementInfo info;
    EnumUIAElements(hwnd, info);
    UIAElementInfo openButton;
    
    if (Find(
        info,
        [](const UIAElementInfo &item)
        {
            if (item.Name == L"打开") {
                return true;
            }
            return false;
        },
        openButton))             {

        //ClickButton(openButton);

    }

    return ;
}

