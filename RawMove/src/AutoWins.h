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