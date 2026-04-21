#include "timer_core.h"
#include "timer_config.h"
#include "timer_window.h"
#include "timer_audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 全局状态实例
TimerState g_timerState = {0};

void InitializeGlobalState(void) {
    memset(&g_timerState, 0, sizeof(TimerState));

    g_timerState.windowWidth = 300;
    g_timerState.windowHeight = 150;
    g_timerState.showMinutes = TRUE;
    g_timerState.showSeconds = TRUE;
    g_timerState.currentLanguage = 0;
    g_timerState.fontColor = RGB(255, 255, 255);
    g_timerState.backgroundColor = RGB(0, 0, 0);
    g_timerState.windowOpacity = 255;
    g_timerState.useChineseAudio = TRUE;
    g_timerState.originalFontColor = g_timerState.fontColor;
    g_timerState.originalBackgroundColor = g_timerState.backgroundColor;
}

void InitializeTimerTime(void) {
    // 尝试加载上次使用的时间
    int lastUsedTime = LoadLastUsedTime();
    
    if (lastUsedTime > 0) {
        // 如果有上次使用的时间，使用它
        g_timerState.seconds = lastUsedTime;
    } else {
        // 如果没有上次使用的时间，使用首个预设时间（10分钟）
        g_timerState.seconds = 10 * 60; // 10分钟 = 600秒
    }
}

void StartTimer(void) {
    g_timerState.isRunning = TRUE;
    g_timerState.isTimeUp = FALSE;
    g_timerState.startTime = (DWORD)GetTickCount64();
    
    // 检查是否时间已到（但不要在这里减秒数）
    if (g_timerState.seconds <= 0) {
        g_timerState.isTimeUp = TRUE;
        g_timerState.isRunning = FALSE;
        
        // 播放超时提示音（语音或系统提示音）
        if (g_timerState.enableAudioAlert && !PlayTimeoutAlert()) {
            // 如果音频启用但播放失败，播放系统提示音作为备用
            MessageBeep(MB_ICONEXCLAMATION);
        }
        return; // 如果时间已到，不启动定时器
    }
    
    // 如果显示毫秒，使用更高的刷新频率
    int interval = g_timerState.showMilliseconds ? 50 : 1000; // 毫秒模式下20FPS，否则1秒
    g_timerState.timerId = SetTimer(g_timerState.hMainWnd, 1, interval, NULL);
    
    // 更新托盘图标状态
    UpdateTrayIcon();
}

void StopTimer(void) {
    g_timerState.isRunning = FALSE;
    if (g_timerState.timerId) {
        KillTimer(g_timerState.hMainWnd, g_timerState.timerId);
        g_timerState.timerId = 0;
    }
    
    // 更新托盘图标状态
    UpdateTrayIcon();
}

void ResetTimer(void) {
    StopTimer();
    
    // 重置为上一次设置的时间
    int lastUsedTime = LoadLastUsedTime();
    if (lastUsedTime > 0) {
        g_timerState.seconds = lastUsedTime;
    } else {
        // 如果没有保存的时间，默认为10分钟
        g_timerState.seconds = 600;
    }
    
    g_timerState.isTimeUp = FALSE;
    g_timerState.startTime = 0; // 重置开始时间
    
    // 如果当前在超时正计时模式，恢复原始颜色
    if (g_timerState.isInOvertimeMode) {
        g_timerState.isInOvertimeMode = FALSE;
        g_timerState.fontColor = g_timerState.originalFontColor;
        g_timerState.backgroundColor = g_timerState.originalBackgroundColor;
    }
    
    // 更新托盘图标状态
    UpdateTrayIcon();
}

void FormatTime(int seconds, char* buffer) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    if (g_timerState.showHours) {
        sprintf_s(buffer, 32, "%02d:%02d:%02d", hours, minutes, secs);
    } else {
        sprintf_s(buffer, 32, "%02d:%02d", minutes, secs);
    }
}

void FormatTimeCustom(int seconds, char* buffer) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    // 计算毫秒（倒计时模式：99递减到00）
    int milliseconds = 0;
    if (g_timerState.showMilliseconds && g_timerState.isRunning) {
        DWORD currentTime = (DWORD)GetTickCount64();
        DWORD elapsed = currentTime - g_timerState.startTime;
        
        // 计算当前秒内的毫秒进度
        int msInCurrentSecond = elapsed % 1000;
        
        // 倒计时模式：从99倒数到00
        milliseconds = 99 - (msInCurrentSecond / 10);
        
        if (seconds == 0) {
            milliseconds = 0; // 时间到了，毫秒也归零
        }
    }
    
    char timeStr[256] = "";
    char tempStr[64];
    BOOL needSeparator = FALSE;
    
    // 根据设置显示各个时间单位
    if (g_timerState.showHours) {
        sprintf_s(tempStr, 64, "%02d", hours);
        strcat_s(timeStr, 256, tempStr);
        needSeparator = TRUE;
    }
    
    if (g_timerState.showMinutes) {
        if (needSeparator) strcat_s(timeStr, 256, ":");
        sprintf_s(tempStr, 64, "%02d", minutes);
        strcat_s(timeStr, 256, tempStr);
        needSeparator = TRUE;
    }
    
    if (g_timerState.showSeconds) {
        if (needSeparator) strcat_s(timeStr, 256, ":");
        sprintf_s(tempStr, 64, "%02d", secs);
        strcat_s(timeStr, 256, tempStr);
        needSeparator = TRUE;
    }
    
    if (g_timerState.showMilliseconds) {
        if (needSeparator) strcat_s(timeStr, 256, ".");
        sprintf_s(tempStr, 64, "%02d", milliseconds);
        strcat_s(timeStr, 256, tempStr);
    }
    
    // 如果所有显示选项都为false，显示默认格式（分:秒）
    if (strlen(timeStr) == 0) {
        sprintf_s(timeStr, 256, "%02d:%02d", minutes, secs);
    }
    
    strcpy_s(buffer, 32, timeStr);
}

BOOL IsTimerRunning(void) {
    return g_timerState.isRunning;
}

BOOL IsTimeUp(void) {
    return g_timerState.isTimeUp;
}

int GetRemainingSeconds(void) {
    return g_timerState.seconds;
}

void SetTimerSeconds(int seconds) {
    // 如果计时器正在运行，先停止它
    if (g_timerState.isRunning) {
        StopTimer();
    }
    g_timerState.seconds = seconds;
    g_timerState.isTimeUp = FALSE; // 重置时间到达状态
    g_timerState.startTime = 0; // 重置开始时间
    
    // 如果当前在超时正计时模式，恢复原始颜色
    if (g_timerState.isInOvertimeMode) {
        g_timerState.isInOvertimeMode = FALSE;
        g_timerState.fontColor = g_timerState.originalFontColor;
        g_timerState.backgroundColor = g_timerState.originalBackgroundColor;
    }
    
    // 保存上次使用的时间
    SaveLastUsedTime(seconds);
    
    // 立即更新主窗体显示
    if (g_timerState.hMainWnd) {
        InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
    }
}

void SetCustomTime(int hours, int minutes, int seconds) {
    // 如果计时器正在运行，先停止它
    if (g_timerState.isRunning) {
        StopTimer();
    }
    g_timerState.seconds = hours * 3600 + minutes * 60 + seconds;
    g_timerState.isTimeUp = FALSE; // 重置时间到达状态
    g_timerState.startTime = 0; // 重置开始时间
    
    // 如果当前在超时正计时模式，恢复原始颜色
    if (g_timerState.isInOvertimeMode) {
        g_timerState.isInOvertimeMode = FALSE;
        g_timerState.fontColor = g_timerState.originalFontColor;
        g_timerState.backgroundColor = g_timerState.originalBackgroundColor;
    }
    
    // 保存上次使用的时间
    SaveLastUsedTime(g_timerState.seconds);
    
    // 立即更新主窗体显示
    if (g_timerState.hMainWnd) {
        InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
    }
}