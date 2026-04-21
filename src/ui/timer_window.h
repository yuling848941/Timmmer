#ifndef TIMER_WINDOW_H
#define TIMER_WINDOW_H

#include "timer_types.h"

// 窗口创建和管理
HWND CreateTimerWindow(HINSTANCE hInstance, int nCmdShow);
void SetupWindowStyle(HWND hwnd);

// 窗口样式
void SetWindowRoundedCorners(HWND hwnd, int cornerRadius);
void SetWindowShadow(HWND hwnd, BOOL enable);

// 窗口调整大小
int GetResizeEdge(HWND hwnd, int x, int y);
HCURSOR GetResizeCursor(int edge);

// 菜单管理
HMENU CreateContextMenu(void);

// 系统托盘管理
BOOL InitializeTrayIcon(HWND hwnd);
void ShowTrayIcon(void);
void HideTrayIcon(void);
void UpdateTrayIcon(void);
void CleanupTrayIcon(void);

// 窗口消息处理
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 双缓冲位图管理
void InitBackBuffer(void);
void CleanupBackBuffer(void);
void EnsureBackBufferSize(int width, int height);

// 数字预渲染缓存
void BuildDigitCache(int fontSize);
void CleanupDigitCache(void);

// 分层窗口渲染
void CleanupLayeredBuffer(void);
void UpdateLayeredWindow_Render(void);

#endif // TIMER_WINDOW_H