/*
 * Timer Application - Main Program
 * Copyright (C) 2024 [Your Name/Company Name]
 * All rights reserved.
 * 
 * This software includes fonts licensed under:
 * - MIT License (ProFontWindows)
 * - SIL Open Font License (various fonts)
 * 
 * Uses Windows system libraries: user32, gdi32, winmm, etc.
 * See LICENSE.txt for complete licensing information.
 */

#include <stdio.h>
#include <windows.h>
#include "timer_types.h"
#include "timer_core.h"
#include "timer_config.h"
#include "timer_ui.h"
#include "timer_window.h"
#include "timer_audio.h"
#include "timer_font_resources.h"
#include "timer_dialogs.h"
#include "timer_ipc.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    // 单实例：保证命名管道 \\.\pipe\timmmer 始终对应唯一实例。
    // 已有实例时激活其窗口并退出。
    HANDLE hSingleInstance = CreateMutexW(NULL, TRUE, L"Timmmer_SingleInstance_Mutex");
    if (hSingleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"TimmerClass", NULL);
        if (existing) {
            ShowWindow(existing, SW_SHOW);
            SetForegroundWindow(existing);
        }
        CloseHandle(hSingleInstance);
        return 0;
    }

    // 初始化全局状态
    InitializeGlobalState();
    
    // 加载配置
    LoadFormatConfig();
    LoadAppearanceConfig();
    LoadAudioConfig();
    LoadPresetConfig();
    
    // 初始化计时器时间（使用上次时间或首个预设时间）
    InitializeTimerTime();
    
    // 预加载音频资源（优化启动速度）
    InitializeAudio();
    
    // 创建窗口
    HWND hwnd = CreateTimerWindow(hInstance, nCmdShow);
    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create timer window", "Error", MB_OK | MB_ICONERROR);
        if (hSingleInstance) CloseHandle(hSingleInstance);
        return 1;
    }

    // 启动命名管道 IPC（供 AI agent / 脚本控制与查询）
    Ipc_Start(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // 检查是否有活动的对话框，如果有则处理对话框消息
        BOOL dialogHandled = FALSE;
        
        // 获取当前活动的对话框句柄
        HWND hActiveDialog = NULL;
        if (IsWindow(GetFormatDialog())) {
            hActiveDialog = GetFormatDialog();
        } else if (IsWindow(GetSetTimeDialog())) {
            hActiveDialog = GetSetTimeDialog();
        } else if (IsWindow(GetAppearanceDialog())) {
            hActiveDialog = GetAppearanceDialog();
        } else if (IsWindow(GetIntegratedDialog())) {
            hActiveDialog = GetIntegratedDialog();
        }
        
        // 如果有活动对话框，处理对话框消息
        if (hActiveDialog && IsWindow(hActiveDialog)) {
            // 对于ENTER和ESC键，让对话框窗口过程处理
            if (msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                dialogHandled = TRUE;
            }
            // 对于其他消息（包括Tab键），使用IsDialogMessage处理
            else if (IsDialogMessage(hActiveDialog, &msg)) {
                dialogHandled = TRUE;
            }
        }
        
        if (!dialogHandled) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // 清理音频资源
    CleanupAudio();

    // 释放单实例互斥体（IPC 已在 WM_DESTROY 中关闭）
    if (hSingleInstance) CloseHandle(hSingleInstance);

    return (int) msg.wParam;
}