#ifndef TIMER_WINDOW_H
#define TIMER_WINDOW_H

#include "timer_types.h"

// 窗口创建和管理
HWND CreateTimerWindow(HINSTANCE hInstance, int nCmdShow);

// 窗口调整大小
int GetResizeEdge(HWND hwnd, int x, int y);
HCURSOR GetResizeCursor(int edge);

// 窗口消息处理
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // TIMER_WINDOW_H