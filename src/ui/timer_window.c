#include <stdio.h>
#include "timer_window.h"
#include "timer_core.h"
#include "timer_ui.h"
#include "timer_config.h"
#include "timer_dialogs.h"
#include "timer_fonts.h"
#include "timer_tray.h"
#include "timer_buffer.h"
#include "timer_window_utils.h"
#include "ios_menu.h"
#include <shellapi.h>

// iOS 菜单系统前置声明
static void InitIosMenuSystem(void);
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

// ===========================================
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

// ===========================================
// iOS 风格菜单集成
// ===========================================

static IosMenuItem g_iosMenuItems[20];
static int g_iosMenuItemCount = 0;
static BOOL g_iosMenuInitialized = FALSE;

static void InitIosMenuSystem(void) {
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
