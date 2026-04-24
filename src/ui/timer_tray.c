#include "timer_tray.h"
#include "timer_core.h"
#include "timer_config.h"
#include <stdio.h>
#include <shellapi.h>

// ===========================================
// 系统托盘管理
// ===========================================

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
        64, 64,  // 系统托盘图标大小（64px 适配高分屏）
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
