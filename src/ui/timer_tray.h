#ifndef TIMER_TRAY_H
#define TIMER_TRAY_H

#include <windows.h>

// 系统托盘图标管理
BOOL InitializeTrayIcon(HWND hwnd);
void ShowTrayIcon(void);
void HideTrayIcon(void);
void UpdateTrayIcon(void);
void CleanupTrayIcon(void);

#endif // TIMER_TRAY_H
