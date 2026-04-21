/*
 * iOS Style Context Menu for Win32
 * 使用 UpdateLayeredWindow 实现真正的 ARGB 半透明渲染
 */

#ifndef IOS_MENU_H
#define IOS_MENU_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// 菜单项类型
typedef enum {
    IOS_MENU_ITEM_NORMAL = 0,      // 普通菜单项
    IOS_MENU_ITEM_SWITCH = 1,      // 带开关的菜单项
    IOS_MENU_ITEM_SEPARATOR = 2    // 分隔线（内部使用）
} IosMenuItemType;

// 菜单项结构体
typedef struct {
    IosMenuItemType type;          // 菜单项类型
    const wchar_t* label;          // 标签文本
    const wchar_t* subLabel;       // 子标签/辅助文本（如预设时间字符串）
    const wchar_t* iconChar;       // 图标字符（Unicode）
    BOOL hasDividerAfter;          // 是否在后面显示分隔线
    BOOL isSwitchOn;               // 开关状态（仅 SWITCH 类型）
    UINT commandId;                // 命令 ID
} IosMenuItem;

// 菜单创建参数（内部使用）
typedef struct {
    const IosMenuItem* items;
    int itemCount;
    HWND owner;
    int menuHeight;
} IosMenuCreateParams;

// 初始化菜单系统（程序启动时调用一次）
BOOL IosMenu_Initialize(HINSTANCE hInstance);

// 关闭菜单系统（程序结束时调用一次）
void IosMenu_Shutdown(void);

// 在指定位置显示菜单
HWND IosMenu_Show(HWND owner, int x, int y, const IosMenuItem* items, int itemCount);

// 隐藏并销毁菜单
void IosMenu_Hide(HWND menuHwnd);

// 获取菜单窗口类名
const wchar_t* IosMenu_GetClassName(void);

#ifdef __cplusplus
}
#endif

#endif // IOS_MENU_H
