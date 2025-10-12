#ifndef AUTOWINS_H
#define AUTOWINS_H

#include <string>

class WinTools
{
public:
    // 将文件移动到回收站
    static bool moveToRecycleBin(const std::wstring &filePath);
};


#pragma once
#include <UIAutomation.h>
#include <Windows.h>
#include <string>
#include <set>
#include <vector>
#include <regex>

bool BringWindowToFront(HWND hWnd);
bool IsWindowInForeground(HWND hWnd);
// 根据进程名获取 PID（不区分大小写）
DWORD GetProcessIdByName(const std::wstring &processName);

// 枚举所有窗口，找到属于 pid 的可见顶层窗口
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);

// 获取某个进程的所有可见窗口
std::vector<HWND> GetVisibleWindowsByPid(DWORD pid, const std::wregex *titleRegex);
inline std::vector<HWND> GetVisibleWindowsByPid(DWORD pid,
                                         const std::wstring &titleRegex,
                                         std::regex_constants::syntax_option_type type = std::regex_constants::icase)
{
    return GetVisibleWindowsByPid(pid, &std::wregex(titleRegex, type));
}
inline std::vector<HWND> GetVisibleWindowsByPid(DWORD pid)
{
    return GetVisibleWindowsByPid(pid, nullptr);
}




struct BoundingRectangle
{
    int l, t, r, b;
};

struct UIAElementInfo
{
    std::wstring Name;
    int ControlType;
    std::wstring LocalizedControlType;
    std::set<UIAElementInfo *> subItems;
    BoundingRectangle BoundingRectangle;
    BOOL IsEnabled;


    BOOL IsKeyboardFocusable;
    BOOL HasKeyboardFocus;
    int ProcessId;
    double RuntimeId;
    std::wstring FrameworkId;
    std::wstring ClassName;
    HWND NativeWindowHandle;
    BOOL IsControlElement;
    BOOL IsContentElement;
    std::wstring ProviderDescription;
    BOOL IsDialog;
    int LegacyChildId;
    std::wstring LegacyDefaultAction;
    std::wstring LegacyName;
    int LegacyRole;
    int LegacyState;
    BOOL CanMove;
    BOOL CanResize;
    BOOL CanRotate;
    BOOL CanMaximize;
    BOOL CanMinimize;
    BOOL IsModal;
    BOOL IsTopmost;
    int WindowInteractionState;
    int WindowVisualState;

    // Pattern Available Flags
    BOOL IsAnnotationPatternAvailable;
    BOOL IsDragPatternAvailable;
    BOOL IsDockPatternAvailable;
    BOOL IsDropTargetPatternAvailable;
    BOOL IsExpandCollapsePatternAvailable;
    BOOL IsGridItemPatternAvailable;
    BOOL IsGridPatternAvailable;
    BOOL IsInvokePatternAvailable;
    BOOL IsItemContainerPatternAvailable;
    BOOL IsLegacyIAccessiblePatternAvailable;
    BOOL IsMultipleViewPatternAvailable;
    BOOL IsObjectModelPatternAvailable;
    BOOL IsRangeValuePatternAvailable;
    BOOL IsScrollItemPatternAvailable;
    BOOL IsScrollPatternAvailable;
    BOOL IsSelectionItemPatternAvailable;
    BOOL IsSelectionPatternAvailable;
    BOOL IsSpreadsheetItemPatternAvailable;
    BOOL IsSpreadsheetPatternAvailable;
    BOOL IsStylesPatternAvailable;
    BOOL IsSynchronizedInputPatternAvailable;
    BOOL IsTableItemPatternAvailable;
    BOOL IsTablePatternAvailable;
    BOOL IsTextChildPatternAvailable;
    BOOL IsTextEditPatternAvailable;
    BOOL IsTextPatternAvailable;
    BOOL IsTextPattern2Available;
    BOOL IsTogglePatternAvailable;
    BOOL IsTransformPatternAvailable;
    BOOL IsTransform2PatternAvailable;
    BOOL IsValuePatternAvailable;
    BOOL IsVirtualizedItemPatternAvailable;
    BOOL IsWindowPatternAvailable;
    BOOL IsCustomNavigationPatternAvailable;
    BOOL IsSelectionPattern2Available;
};


#endif // !AUTOWINS_H