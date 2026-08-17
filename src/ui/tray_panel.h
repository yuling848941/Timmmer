/*
 * 托盘快速操控面板（Quick Control Flyout）
 * 左键点击托盘图标弹出，提供开始/暂停、重置、预设快速开始与常用开关
 */

#ifndef TIMER_TRAY_PANEL_H
#define TIMER_TRAY_PANEL_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化面板系统（程序启动时调用一次）
BOOL TrayPanel_Initialize(HINSTANCE hInstance);

// 关闭面板系统（程序结束时调用一次）
void TrayPanel_Shutdown(void);

// 切换面板显示状态（已打开则关闭，否则在托盘图标上方弹出）
void TrayPanel_Toggle(HWND owner);

#ifdef __cplusplus
}
#endif

#endif // TIMER_TRAY_PANEL_H
