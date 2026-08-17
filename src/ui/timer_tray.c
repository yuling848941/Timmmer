#include "timer_tray.h"
#include "timer_core.h"
#include "timer_config.h"
#include <stdio.h>
#include <shellapi.h>

// ===========================================
// 系统托盘管理
// ===========================================

// 统一构建 tooltip 文案：时间 + 状态，走语言包，不硬编码英文前缀
static void BuildTrayTooltipText(wchar_t* out, size_t cap) {
    char timeNarrow[32];
    wchar_t timeWide[40];
    FormatTimeCustom(g_timerState.seconds, timeNarrow);
    size_t i = 0;
    for (; timeNarrow[i] != '\0' && i + 1 < 40; i++) {
        timeWide[i] = (wchar_t)(unsigned char)timeNarrow[i];
    }
    timeWide[i] = L'\0';

    const MenuTexts* texts = GetMenuTexts();
    swprintf_s(out, cap, L"%s (%s)", timeWide,
               g_timerState.isRunning ? texts->pause : texts->start);
}

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

    // 设置提示文本（初始即为当前时间状态）
    BuildTrayTooltipText(g_timerState.trayIconData.szTip, 128);

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
        BuildTrayTooltipText(g_timerState.trayIconData.szTip, 128);
        Shell_NotifyIconW(NIM_MODIFY, &g_timerState.trayIconData);
    }
}

void RefreshTrayIconAfterTaskbarRestart(void) {
    // Explorer 重启后旧图标已失效，重置可见标记以强制重新添加
    g_timerState.isTrayIconVisible = FALSE;
    ShowTrayIcon();
    UpdateTrayIcon();
}

void CleanupTrayIcon(void) {
    HideTrayIcon();

    // 释放图标资源
    if (g_timerState.trayIconData.hIcon) {
        DestroyIcon(g_timerState.trayIconData.hIcon);
        g_timerState.trayIconData.hIcon = NULL;
    }
}
