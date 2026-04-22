#include <stdio.h>
#include "timer_window.h"
#include "timer_core.h"
#include "timer_ui.h"
#include "timer_config.h"
#include "timer_dialogs.h"
#include "timer_fonts.h"
#include "ios_menu.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

// 前置声明
void InitBackBuffer(void);
void CleanupBackBuffer(void);
void EnsureBackBufferSize(int width, int height);
void CleanupDigitCache(void);
void CleanupLayeredBuffer(void);
void UpdateLayeredWindow_Render(void);

// iOS 菜单系统声明
void InitIosMenuSystem(void);
static void ShowIosContextMenu(HWND owner, int x, int y);

// ===========================================
// 窗口创建和初始化
// ===========================================

HWND CreateTimerWindow(HINSTANCE hInstance, int nCmdShow) {
    (void)nCmdShow;  // 预留参数，当前未使用
    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TimmerClass";
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_DBLCLKS;  // 启用双击消息

    // 设置窗口图标
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    if (!wc.hIcon) {
        wc.hIcon = (HICON)LoadImageW(NULL, L"timmmer-modern.ico", IMAGE_ICON,
                                     32, 32, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    if (!wc.hIcon) {
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    if (!RegisterClassW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return NULL;
        }
    }

    // 加载保存的窗口位置和大小
    int x, y, width, height;
    LoadWindowConfig(&x, &y, &width, &height);

    // 更新全局状态中的窗口大小
    g_timerState.windowWidth = width;
    g_timerState.windowHeight = height;

    // 创建无边框窗口
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    DWORD style = WS_POPUP;

    if (g_timerState.transparentBackground) {
        exStyle &= ~WS_EX_TRANSPARENT;
    }

    HWND hwnd = CreateWindowExW(
        exStyle,
        L"TimmerClass",
        L"Timmmer",
        style,
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        return NULL;
    }

    g_timerState.hMainWnd = hwnd;

    // 设置窗口透明度
    if (!g_timerState.transparentBackground) {
        SetLayeredWindowAttributes(hwnd, 0, g_timerState.windowOpacity, LWA_ALPHA);
    }

    // 设置窗口装饰效果（圆角和阴影）- 仅在非透明背景模式下应用
    if (!g_timerState.transparentBackground) {
        SetWindowRoundedCorners(hwnd, 6);
        SetWindowShadow(hwnd, TRUE);
    }

    // 初始化系统托盘图标
    InitializeTrayIcon(hwnd);
    ShowTrayIcon();

    // 初始化 iOS 菜单系统
    InitIosMenuSystem();

    // 初始化持久化双缓冲位图
    InitBackBuffer();

    // 强制显示窗口
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    // 透明背景模式：首次调用 UpdateLayeredWindow_Render 来初始化窗口
    if (g_timerState.transparentBackground) {
        UpdateLayeredWindow_Render();
    }

    return hwnd;
}

// 窗口样式设置
void SetWindowRoundedCorners(HWND hwnd, int cornerRadius) {
    if (!hwnd) return;

    BOOL dwmEnabled = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&dwmEnabled)) && dwmEnabled) {
        if (cornerRadius <= 0) {
            DWORD cornerPref = 1; // DWMWCP_DONOTROUND
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
            BOOL ncrenderingEnabled = FALSE;
            DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncrenderingEnabled, sizeof(ncrenderingEnabled));
            MARGINS margins = {0, 0, 0, 0};
            DwmExtendFrameIntoClientArea(hwnd, &margins);
            SetWindowRgn(hwnd, NULL, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
            return;
        }

        DWORD cornerPref = 2; // DWMWCP_ROUND
        HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
        if (SUCCEEDED(hr)) {
            SetWindowRgn(hwnd, NULL, TRUE);
            return;
        }
    }

    if (cornerRadius <= 0) {
        SetWindowRgn(hwnd, NULL, TRUE);
        return;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HRGN hRgn = CreateRoundRectRgn(0, 0, width, height, cornerRadius * 2, cornerRadius * 2);
    if (hRgn) {
        SetWindowRgn(hwnd, hRgn, TRUE);
    }
}

void SetWindowShadow(HWND hwnd, BOOL enable) {
    if (!hwnd) return;

    BOOL dwmEnabled = FALSE;
    HRESULT hr = DwmIsCompositionEnabled(&dwmEnabled);
    if (FAILED(hr) || !dwmEnabled) return;

    if (enable) {
        enum DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
        BOOL ncRenderingEnabled = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncRenderingEnabled, sizeof(ncRenderingEnabled));
        MARGINS margins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        BOOL enableShadow = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &enableShadow, sizeof(enableShadow));
    } else {
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        BOOL ncRenderingEnabled = FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncRenderingEnabled, sizeof(ncRenderingEnabled));
        enum DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
        BOOL disableShadow = FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &disableShadow, sizeof(disableShadow));
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_BORDER | WS_DLGFRAME | WS_THICKFRAME);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_DRAWFRAME);
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
    }
}

// ===========================================
// 系统托盘管理
BOOL InitializeTrayIcon(HWND hwnd) {
    // 初始化托盘图标数据
    ZeroMemory(&g_timerState.trayIconData, sizeof(NOTIFYICONDATAW));
    g_timerState.trayIconData.cbSize = sizeof(NOTIFYICONDATAW);
    g_timerState.trayIconData.hWnd = hwnd;
    g_timerState.trayIconData.uID = TRAY_ICON_ID;
    g_timerState.trayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_timerState.trayIconData.uCallbackMessage = WM_TRAYICON;

    // 使用自定义图标文件
    g_timerState.trayIconData.hIcon = (HICON)LoadImageW(
        NULL,
        L"timmmer-modern.ico",
        IMAGE_ICON,
        16, 16,  // 系统托盘图标大小
        LR_LOADFROMFILE | LR_DEFAULTSIZE
    );

    if (!g_timerState.trayIconData.hIcon) {
        // 如果自定义图标加载失败，尝试使用应用程序图标
        g_timerState.trayIconData.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
        if (!g_timerState.trayIconData.hIcon) {
            // 如果没有应用程序图标，使用系统默认图标
            g_timerState.trayIconData.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        }
    }

    // 设置提示文本
    const MenuTexts* texts = GetMenuTexts();
    wcscpy_s(g_timerState.trayIconData.szTip, 128, L"Timer - ");
    wcscat_s(g_timerState.trayIconData.szTip, 128, texts->startPause);

    g_timerState.isTrayIconVisible = FALSE;
    return TRUE;
}

void ShowTrayIcon(void) {
    if (!g_timerState.isTrayIconVisible) {
        Shell_NotifyIconW(NIM_ADD, &g_timerState.trayIconData);
        g_timerState.isTrayIconVisible = TRUE;
    }
}

void HideTrayIcon(void) {
    if (g_timerState.isTrayIconVisible) {
        Shell_NotifyIconW(NIM_DELETE, &g_timerState.trayIconData);
        g_timerState.isTrayIconVisible = FALSE;
    }
}

void UpdateTrayIcon(void) {
    if (g_timerState.isTrayIconVisible) {
        // 更新提示文本显示当前时间状态
        const MenuTexts* texts = GetMenuTexts();
        wchar_t timeStr[64];
        wchar_t statusStr[128];

        // 格式化时间显示
        int hours = g_timerState.seconds / 3600;
        int minutes = (g_timerState.seconds % 3600) / 60;
        int secs = g_timerState.seconds % 60;

        if (g_timerState.showHours) {
            swprintf_s(timeStr, 64, L"%02d:%02d:%02d", hours, minutes, secs);
        } else {
            swprintf_s(timeStr, 64, L"%02d:%02d", minutes, secs);
        }

        // 组合状态信息
        swprintf_s(statusStr, 128, L"Timer - %s (%s)",
                  timeStr,
                  g_timerState.isRunning ? texts->pause : texts->start);

        wcscpy_s(g_timerState.trayIconData.szTip, 128, statusStr);
        Shell_NotifyIconW(NIM_MODIFY, &g_timerState.trayIconData);
    }
}

void CleanupTrayIcon(void) {
    HideTrayIcon();

    // 释放图标资源
    if (g_timerState.trayIconData.hIcon) {
        DestroyIcon(g_timerState.trayIconData.hIcon);
        g_timerState.trayIconData.hIcon = NULL;
    }
}

// 窗口消息处理
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 透明背景模式使用 UpdateLayeredWindow
            if (g_timerState.transparentBackground) {
                UpdateLayeredWindow_Render();
                EndPaint(hwnd, &ps);
                return 0;
            }

            // 非透明模式使用普通双缓冲渲染
            // 确保双缓冲位图尺寸正确
            EnsureBackBufferSize(g_timerState.windowWidth, g_timerState.windowHeight);

            // 初始化位图（首次创建时）
            if (g_timerState.hdcBackBuffer == NULL) {
                InitBackBuffer();
            }

            // 每次都重新绘制内容（倒计时时需要更新显示）
            HDC hdcMem = g_timerState.hdcBackBuffer;

            // 绘制背景
            RECT rect = {0, 0, g_timerState.windowWidth, g_timerState.windowHeight};
            HBRUSH hBrush = CreateSolidBrush(g_timerState.backgroundColor);
            FillRect(hdcMem, &rect, hBrush);
            DeleteObject(hBrush);

            UpdateDisplay(hdcMem);

            // 从持久化位图复制到屏幕
            BitBlt(hdc, 0, 0, g_timerState.windowWidth, g_timerState.windowHeight,
                   g_timerState.hdcBackBuffer, 0, 0, SRCCOPY);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_PRINTCLIENT: {
            // 处理 WM_PRINTCLIENT 消息，确保窗口内容正确绘制到设备上下文
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);

            if (g_timerState.transparentBackground) {
                // 透明模式：绘制文本
                UpdateDisplay(hdc);
            } else {
                // 非透明模式：绘制背景和文本
                HBRUSH hBrush = CreateSolidBrush(g_timerState.backgroundColor);
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);
                UpdateDisplay(hdc);
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                // 帧率智能控制：根据窗口可见性决定是否渲染
                if (IsWindowVisible(hwnd) && !g_timerState.isMinimized) {
                    TimerTick(hwnd);

                    // 检查窗口是否在后台（无焦点且不是前台窗口）
                    HWND foregroundWnd = GetForegroundWindow();
                    if (foregroundWnd != hwnd && foregroundWnd != NULL) {
                        // 判断前台窗口是否属于本进程（如字体菜单、外观设置等子窗口）
                        // 若属于本进程，不视为"后台"，仍正常刷新
                        DWORD fgPid = 0;
                        GetWindowThreadProcessId(foregroundWnd, &fgPid);
                        DWORD myPid = GetCurrentProcessId();
                        if (fgPid != myPid) {
                            // 真正在后台（其他进程的窗口在前台），降低刷新率（每 3 次刷新一次）
                            static int skipCounter = 0;
                            skipCounter++;
                            if (skipCounter < 3) {
                                // 跳过这次渲染，但仍然更新计时逻辑
                                return 0;
                            }
                            skipCounter = 0;
                        }
                    }

                    // 根据模式选择渲染方法
                    if (g_timerState.transparentBackground) {
                        UpdateLayeredWindow_Render();
                    } else {
                        InvalidateRect(hwnd, NULL, FALSE);
                        UpdateWindow(hwnd);
                    }
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            g_timerState.resizeEdge = GetResizeEdge(hwnd, x, y);

            if (g_timerState.resizeEdge != 0) {
                g_timerState.isResizing = TRUE;
                SetCapture(hwnd);
                GetCursorPos(&g_timerState.lastMousePos);
                GetWindowRect(hwnd, &g_timerState.windowRect);
            } else {
                g_timerState.isDragging = TRUE;
                SetCapture(hwnd);
                GetCursorPos(&g_timerState.dragStart);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            if (g_timerState.isResizing) {
                POINT currentPos;
                GetCursorPos(&currentPos);

                int deltaX = currentPos.x - g_timerState.lastMousePos.x;
                int deltaY = currentPos.y - g_timerState.lastMousePos.y;

                RECT newRect = g_timerState.windowRect;

                if (g_timerState.resizeEdge & RESIZE_LEFT) {
                    newRect.left += deltaX;
                }
                if (g_timerState.resizeEdge & RESIZE_RIGHT) {
                    newRect.right += deltaX;
                }
                if (g_timerState.resizeEdge & RESIZE_TOP) {
                    newRect.top += deltaY;
                }
                if (g_timerState.resizeEdge & RESIZE_BOTTOM) {
                    newRect.bottom += deltaY;
                }

                int newWidth = newRect.right - newRect.left;
                int newHeight = newRect.bottom - newRect.top;

                if (newWidth >= 100 && newHeight >= 50) {
                    SetWindowPos(hwnd, NULL, newRect.left, newRect.top,
                               newWidth, newHeight, SWP_NOZORDER);
                    g_timerState.needsFinalRedraw = TRUE;
                }
            } else if (g_timerState.isDragging) {
                POINT currentPos;
                GetCursorPos(&currentPos);

                int deltaX = currentPos.x - g_timerState.dragStart.x;
                int deltaY = currentPos.y - g_timerState.dragStart.y;

                RECT rect;
                GetWindowRect(hwnd, &rect);
                SetWindowPos(hwnd, NULL, rect.left + deltaX, rect.top + deltaY,
                           0, 0, SWP_NOSIZE | SWP_NOZORDER);

                g_timerState.dragStart = currentPos;
            } else {
                int edge = GetResizeEdge(hwnd, x, y);
                SetCursor(GetResizeCursor(edge));
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_timerState.isDragging || g_timerState.isResizing) {
                g_timerState.isDragging = FALSE;
                g_timerState.isResizing = FALSE;
                g_timerState.resizeEdge = 0;
                ReleaseCapture();

                // 拖拽结束后补偿一次完整重绘（确保最终尺寸精确渲染）
                if (g_timerState.needsFinalRedraw) {
                    g_timerState.needsFinalRedraw = FALSE;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            // 双击切换预设时间
            CycleToNextPresetTime();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_TRAYICON: {
            switch (lParam) {
                case WM_RBUTTONUP: {
                    // 托盘图标右键菜单
                    POINT pt;
                    GetCursorPos(&pt);

                    // 设置前台窗口，确保菜单能正确显示和消失
                    SetForegroundWindow(hwnd);

                    // 使用 iOS 风格菜单
                    ShowIosContextMenu(hwnd, pt.x, pt.y);

                    break;
                }

                case WM_LBUTTONDBLCLK: {
                    // 托盘图标双击切换预设时间
                    CycleToNextPresetTime();
                    // 如果窗口隐藏，则显示窗口以便用户看到时间变化
                    if (!IsWindowVisible(hwnd)) {
                        ShowWindow(hwnd, SW_SHOW);
                        SetForegroundWindow(hwnd);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            ShowIosContextMenu(hwnd, pt.x, pt.y);
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_START_PAUSE:
                    if (g_timerState.isRunning) {
                        StopTimer();
                    } else {
                        StartTimer();
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;

                case ID_RESET:
                    ResetTimer();
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;

                case ID_SET_TIME:
                    ShowIntegratedDialog();
                    break;

                case ID_FORMAT:
                    ShowIntegratedDialog();
                    break;

                case ID_INTEGRATED:
                    ShowIntegratedDialog();
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;

                case ID_APPEARANCE:
                    ShowAppearanceDialog();
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;

                case ID_AUDIO_SETTINGS:
                    ShowAudioDialog();
                    break;

                case ID_OVERTIME_COUNT:
                    // 切换超时正计时功能状态
                    g_timerState.enableOvertimeCount = !g_timerState.enableOvertimeCount;

                    // 如果关闭功能且当前在超时模式，则恢复正常状态
                    if (!g_timerState.enableOvertimeCount && g_timerState.isInOvertimeMode) {
                        g_timerState.isInOvertimeMode = FALSE;
                        g_timerState.fontColor = g_timerState.originalFontColor;
                        g_timerState.backgroundColor = g_timerState.originalBackgroundColor;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    break;

                case ID_LANGUAGE:
                    g_timerState.currentLanguage = (g_timerState.currentLanguage == TIMER_LANG_CHINESE) ?
                                                        TIMER_LANG_ENGLISH : TIMER_LANG_CHINESE;
                    break;

                case ID_ABOUT:
                    ShowAboutDialog();
                    break;

                case ID_EDIT_PRESETS:
                    ShowPresetEditDialog();
                    break;

                default:
                    // 处理预设时间菜单项
                    if (LOWORD(wParam) >= ID_PRESET_BASE && LOWORD(wParam) < ID_PRESET_BASE + g_timerState.presetCount) {
                        int presetIndex = LOWORD(wParam) - ID_PRESET_BASE;
                        int presetMinutes = g_timerState.presetTimes[presetIndex];
                        SetTimerSeconds(presetMinutes * 60);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    break;

                case ID_TRAY_SHOW:
                    ShowWindow(hwnd, SW_SHOW);
                    SetForegroundWindow(hwnd);
                    break;

                case ID_TRAY_HIDE:
                    ShowWindow(hwnd, SW_HIDE);
                    break;

                case ID_EXIT:
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            // 获取滚轮滚动方向和距离
            int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);

            // 获取当前窗口大小和位置
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            int currentWidth = windowRect.right - windowRect.left;
            int currentHeight = windowRect.bottom - windowRect.top;

            // 计算缩放比例（每次滚动 5%）
            float scaleFactor = 1.0f;
            if (wheelDelta > 0) {
                // 向上滚动，放大
                scaleFactor = 1.05f;
            } else {
                // 向下滚动，缩小
                scaleFactor = 0.95f;
            }

            // 计算新的窗口大小
            int newWidth = (int)(currentWidth * scaleFactor);
            int newHeight = (int)(currentHeight * scaleFactor);

            // 限制最小和最大尺寸
            if (newWidth < 100) newWidth = 100;
            if (newHeight < 50) newHeight = 50;
            if (newWidth > GetSystemMetrics(SM_CXSCREEN)) newWidth = GetSystemMetrics(SM_CXSCREEN);
            if (newHeight > GetSystemMetrics(SM_CYSCREEN)) newHeight = GetSystemMetrics(SM_CYSCREEN);

            // 计算新的窗口位置（保持窗口中心不变）
            int centerX = windowRect.left + currentWidth / 2;
            int centerY = windowRect.top + currentHeight / 2;
            int newX = centerX - newWidth / 2;
            int newY = centerY - newHeight / 2;

            // 确保窗口不会移出屏幕
            if (newX < 0) newX = 0;
            if (newY < 0) newY = 0;
            if (newX + newWidth > GetSystemMetrics(SM_CXSCREEN)) {
                newX = GetSystemMetrics(SM_CXSCREEN) - newWidth;
            }
            if (newY + newHeight > GetSystemMetrics(SM_CYSCREEN)) {
                newY = GetSystemMetrics(SM_CYSCREEN) - newHeight;
            }

            // 应用新的窗口大小和位置
            SetWindowPos(hwnd, NULL, newX, newY, newWidth, newHeight, SWP_NOZORDER);

            return 0;
        }

        case WM_SIZE: {
            g_timerState.windowWidth = LOWORD(lParam);
            g_timerState.windowHeight = HIWORD(lParam);

            // 跟踪最小化状态
            if (wParam == SIZE_MINIMIZED) {
                g_timerState.isMinimized = TRUE;
            } else {
                g_timerState.isMinimized = FALSE;
            }

            // 限制最小尺寸
            if (g_timerState.windowWidth < 50) g_timerState.windowWidth = 50;
            if (g_timerState.windowHeight < 25) g_timerState.windowHeight = 25;

            // 标记双缓冲位图需要重建
            g_timerState.backBufferDirty = TRUE;

            // 根据透明背景模式决定是否应用装饰效果
            if (!g_timerState.transparentBackground) {
                // 更新圆角效果
                SetWindowRoundedCorners(hwnd, 6);

                // 更新阴影效果
                SetWindowShadow(hwnd, TRUE);
            } else {
                // 透明模式下确保移除装饰效果
                SetWindowRoundedCorners(hwnd, 0);
                SetWindowShadow(hwnd, FALSE);
            }

            // 强制同步重绘：InvalidateRect 仅标记脏区域，WM_PAINT 要等消息队列空闲才执行，
            // 拖拽期间鼠标事件密集导致 WM_PAINT 排不上队，窗口框架和内容不同步。
            // RedrawWindow + RDW_UPDATENOW 强制立即执行 WM_PAINT。
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
            return 0;
        }

        case WM_NCHITTEST: {
            // 在透明背景模式下，确保整个窗口区域都能接收鼠标消息
            if (g_timerState.transparentBackground) {
                // 直接返回 HTCLIENT，让整个窗口区域都能接收鼠标消息
                return HTCLIENT;
            }
            // 非透明背景模式使用默认处理
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_ACTIVATE: {
            // 跟踪窗口焦点状态
            if (LOWORD(wParam) == WA_INACTIVE) {
                g_timerState.isWindowInForeground = FALSE;
            } else {
                g_timerState.isWindowInForeground = TRUE;
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (g_timerState.timerId) {
                KillTimer(hwnd, g_timerState.timerId);
            }

            // 保存窗口位置和大小
            RECT windowRect;
            if (GetWindowRect(hwnd, &windowRect)) {
                SaveWindowConfig(windowRect.left, windowRect.top,
                               windowRect.right - windowRect.left,
                               windowRect.bottom - windowRect.top);
            }

            // 清理系统托盘图标
            CleanupTrayIcon();
            // 清理字体系统
            CleanupTimerFonts();
            // 清理持久化双缓冲位图
            CleanupBackBuffer();
            // 清理分层窗口缓冲区
            CleanupLayeredBuffer();
            // 清理数字预渲染缓存
            CleanupDigitCache();
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============== 持久化双缓冲位图实现 ==============

/**
 * @brief 初始化持久化双缓冲位图
 */
void InitBackBuffer(void) {
    if (g_timerState.hdcBackBuffer != NULL) {
        return; // 已初始化
    }

    HDC hdcScreen = GetDC(NULL);
    if (hdcScreen) {
        g_timerState.hdcBackBuffer = CreateCompatibleDC(hdcScreen);
        g_timerState.hbmBackBuffer = CreateCompatibleBitmap(hdcScreen,
                                                            g_timerState.windowWidth,
                                                            g_timerState.windowHeight);
        if (g_timerState.hbmBackBuffer) {
            SelectObject(g_timerState.hdcBackBuffer, g_timerState.hbmBackBuffer);
        }
        g_timerState.backBufferWidth = g_timerState.windowWidth;
        g_timerState.backBufferHeight = g_timerState.windowHeight;
        g_timerState.backBufferDirty = TRUE;
        ReleaseDC(NULL, hdcScreen);
    }
}

/**
 * @brief 清理持久化双缓冲位图
 */
void CleanupBackBuffer(void) {
    if (g_timerState.hbmBackBuffer) {
        DeleteObject(g_timerState.hbmBackBuffer);
        g_timerState.hbmBackBuffer = NULL;
    }
    if (g_timerState.hdcBackBuffer) {
        DeleteDC(g_timerState.hdcBackBuffer);
        g_timerState.hdcBackBuffer = NULL;
    }
    g_timerState.backBufferWidth = 0;
    g_timerState.backBufferHeight = 0;
    g_timerState.backBufferDirty = FALSE;
}

/**
 * @brief 确保双缓冲位图尺寸匹配窗口
 */
void EnsureBackBufferSize(int width, int height) {
    if (g_timerState.hbmBackBuffer &&
        width == g_timerState.backBufferWidth &&
        height == g_timerState.backBufferHeight) {
        return; // 尺寸匹配，无需重建
    }

    // 清理旧位图
    CleanupBackBuffer();

    // 创建新位图
    HDC hdcScreen = GetDC(NULL);
    if (hdcScreen) {
        g_timerState.hdcBackBuffer = CreateCompatibleDC(hdcScreen);
        g_timerState.hbmBackBuffer = CreateCompatibleBitmap(hdcScreen, width, height);
        if (g_timerState.hbmBackBuffer) {
            SelectObject(g_timerState.hdcBackBuffer, g_timerState.hbmBackBuffer);
        }
        g_timerState.backBufferWidth = width;
        g_timerState.backBufferHeight = height;
        g_timerState.backBufferDirty = TRUE;
        ReleaseDC(NULL, hdcScreen);
    }
}

// ============== 数字预渲染缓存实现 ==============

/**
 * @brief 清理数字预渲染缓存
 */
void CleanupDigitCache(void) {
    for (int i = 0; i < 10; i++) {
        if (g_timerState.hbmDigitCache[i]) {
            DeleteObject(g_timerState.hbmDigitCache[i]);
            g_timerState.hbmDigitCache[i] = NULL;
        }
    }
    if (g_timerState.hbmColonCache) {
        DeleteObject(g_timerState.hbmColonCache);
        g_timerState.hbmColonCache = NULL;
    }
    if (g_timerState.hbmDotCache) {
        DeleteObject(g_timerState.hbmDotCache);
        g_timerState.hbmDotCache = NULL;
    }
    g_timerState.digitCacheFontSize = 0;
    g_timerState.digitCacheValid = FALSE;
}

/**
 * @brief 构建数字预渲染缓存
 */
void BuildDigitCache(int fontSize) {
    if (g_timerState.digitCacheValid && fontSize == g_timerState.digitCacheFontSize) {
        return;
    }

    CleanupDigitCache();

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return;
    }

    HFONT hFont = GetMonospaceFont(fontSize);
    if (!hFont) hFont = GetDefaultTimerFont(fontSize);
    if (!hFont) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return;
    }

    HFONT oldFont = (HFONT)SelectObject(hdcMem, hFont);
    SetTextAlign(hdcMem, TA_TOP | TA_LEFT | TA_BASELINE);
    SetBkMode(hdcMem, TRANSPARENT);

    const char* digits = "0123456789:.";
    for (int i = 0; digits[i] != '\0'; i++) {
        char digitStr[2] = {digits[i], '\0'};
        SIZE textSize;
        GetTextExtentPointA(hdcMem, digitStr, 1, &textSize);

        int bmpWidth = textSize.cx + 4;
        int bmpHeight = textSize.cy + 4;

        HBITMAP hbmDigit = CreateCompatibleBitmap(hdcScreen, bmpWidth, bmpHeight);
        if (hbmDigit) {
            HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, hbmDigit);
            RECT rect = {0, 0, bmpWidth, bmpHeight};
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdcMem, &rect, hBrush);
            DeleteObject(hBrush);
            SetTextColor(hdcMem, RGB(255, 255, 255));
            TextOutA(hdcMem, 2, 2, digitStr, 1);
            SelectObject(hdcMem, oldBmp);

            if (digits[i] >= '0' && digits[i] <= '9') g_timerState.hbmDigitCache[digits[i] - '0'] = hbmDigit;
            else if (digits[i] == ':') g_timerState.hbmColonCache = hbmDigit;
            else if (digits[i] == '.') g_timerState.hbmDotCache = hbmDigit;
        }
    }

    SelectObject(hdcMem, oldFont);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    g_timerState.digitCacheFontSize = fontSize;
    g_timerState.digitCacheValid = TRUE;
}

// ============== UpdateLayeredWindow 透明渲染实现 ==============

// 保存 DIB 位图的像素指针
static void* g_layeredBufferBits = NULL;

/**
 * @brief 清理分层窗口缓冲区
 */
void CleanupLayeredBuffer(void) {
    if (g_timerState.hbmLayeredBuffer) {
        DeleteObject(g_timerState.hbmLayeredBuffer);
        g_timerState.hbmLayeredBuffer = NULL;
    }
    if (g_timerState.hdcLayeredBuffer) {
        DeleteDC(g_timerState.hdcLayeredBuffer);
        g_timerState.hdcLayeredBuffer = NULL;
    }
    g_timerState.layeredBufferSize.cx = 0;
    g_timerState.layeredBufferSize.cy = 0;
    g_layeredBufferBits = NULL;
}

/**
 * @brief 使用 UpdateLayeredWindow 渲染透明窗口
 */
void UpdateLayeredWindow_Render(void) {
    if (!g_timerState.hMainWnd) {
        return;
    }

    HWND hwnd = g_timerState.hMainWnd;
    int width = g_timerState.windowWidth;
    int height = g_timerState.windowHeight;

    // 确保缓冲区尺寸正确
    if (width != g_timerState.layeredBufferSize.cx ||
        height != g_timerState.layeredBufferSize.cy) {
        CleanupLayeredBuffer();
    }

    // 创建缓冲区
    if (!g_timerState.hdcLayeredBuffer) {
        HDC hdcScreen = GetDC(NULL);
        if (hdcScreen) {
            g_timerState.hdcLayeredBuffer = CreateCompatibleDC(hdcScreen);

            BITMAPINFOHEADER bi = {0};
            bi.biSize = sizeof(BITMAPINFOHEADER);
            bi.biWidth = width;
            bi.biHeight = -height;
            bi.biPlanes = 1;
            bi.biBitCount = 32;
            bi.biCompression = BI_RGB;

            g_timerState.hbmLayeredBuffer = CreateDIBSection(hdcScreen,
                (BITMAPINFO*)&bi, DIB_RGB_COLORS, &g_layeredBufferBits, NULL, 0);

            if (g_timerState.hbmLayeredBuffer) {
                SelectObject(g_timerState.hdcLayeredBuffer, g_timerState.hbmLayeredBuffer);
                if (g_layeredBufferBits) {
                    memset(g_layeredBufferBits, 0, width * height * 4);
                }
            }
            g_timerState.layeredBufferSize.cx = width;
            g_timerState.layeredBufferSize.cy = height;
            ReleaseDC(NULL, hdcScreen);
        }
    }

    if (!g_timerState.hdcLayeredBuffer || !g_timerState.hbmLayeredBuffer) {
        return;
    }

    HDC hdcMem = g_timerState.hdcLayeredBuffer;

    if (g_layeredBufferBits) {
        memset(g_layeredBufferBits, 0, width * height * 4);
    }

    SetTextColor(hdcMem, g_timerState.fontColor);
    SetBkMode(hdcMem, TRANSPARENT);

    UpdateDisplay(hdcMem);

    GdiFlush();

    if (g_layeredBufferBits) {
        DWORD* pixels = (DWORD*)g_layeredBufferBits;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = y * width + x;
                DWORD pixel = pixels[idx];
                BYTE b = (pixel >> 0) & 0xFF;
                BYTE g = (pixel >> 8) & 0xFF;
                BYTE r = (pixel >> 16) & 0xFF;
                if (r != 0 || g != 0 || b != 0) {
                    pixels[idx] = (pixel & 0x00FFFFFF) | 0xFF000000;
                }
            }
        }
    }

    POINT ptSrc = {0, 0};
    RECT rcWnd;
    GetWindowRect(hwnd, &rcWnd);
    POINT ptDst = {rcWnd.left, rcWnd.top};

    SIZE sizeNew = {width, height};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    blend.SourceConstantAlpha = g_timerState.windowOpacity;

    UpdateLayeredWindow(hwnd, NULL, &ptDst, &sizeNew, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
}

// ===========================================
// iOS 风格菜单集成
// ===========================================

static IosMenuItem g_iosMenuItems[20];
static int g_iosMenuItemCount = 0;
static BOOL g_iosMenuInitialized = FALSE;

void InitIosMenuSystem(void) {
    if (!g_iosMenuInitialized) {
        IosMenu_Initialize(GetModuleHandle(NULL));
        g_iosMenuInitialized = TRUE;
    }
}

static void ShowIosContextMenu(HWND owner, int x, int y) {
    const MenuTexts* texts = GetMenuTexts();
    g_iosMenuItemCount = 0;

    // === 计时控制组 ===
    if (g_timerState.isRunning) {
        g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
            IOS_MENU_ITEM_NORMAL, texts->startPause, NULL, L"\uE769", FALSE, FALSE, ID_START_PAUSE
        };
    } else {
        g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
            IOS_MENU_ITEM_NORMAL, texts->startPause, NULL, L"\uE768", FALSE, FALSE, ID_START_PAUSE
        };
    }
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_NORMAL, texts->reset, NULL, L"\uE777", TRUE, FALSE, ID_RESET
    };

    // === 时间设置组 ===
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_NORMAL, texts->integratedTitle, NULL, L"\uE823", TRUE, FALSE, ID_INTEGRATED
    };

    // === 设置与资源组 ===
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_NORMAL, texts->appearance, NULL, L"\uE771", FALSE, FALSE, ID_APPEARANCE
    };
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_NORMAL, texts->audioSettings, NULL, L"\uE767", FALSE, FALSE, ID_AUDIO_SETTINGS
    };
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_NORMAL, texts->editPresets, NULL, L"\uE70F", TRUE, FALSE, ID_EDIT_PRESETS
    };


    // === 功能开关组 ===
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_SWITCH, texts->overtimeCount, NULL, L"\uE916", TRUE, g_timerState.enableOvertimeCount, ID_OVERTIME_COUNT
    };

    // === 系统组 ===
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_NORMAL, texts->language, NULL, L"\uE774", FALSE, FALSE, ID_LANGUAGE
    };
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_NORMAL, texts->about, NULL, L"\uE946", TRUE, FALSE, ID_ABOUT
    };
    g_iosMenuItems[g_iosMenuItemCount++] = (IosMenuItem){
        IOS_MENU_ITEM_NORMAL, texts->exit, NULL, L"\uE711", FALSE, FALSE, ID_EXIT
    };

    IosMenu_Show(owner, x, y, g_iosMenuItems, g_iosMenuItemCount);
}
