#ifndef TIMER_CORE_H
#define TIMER_CORE_H

#include "timer_types.h"

// 计时器核心功能
void StartTimer(void);
void StopTimer(void);
void ResetTimer(void);

// 时间格式化
void FormatTime(int seconds, char* buffer);
void FormatTimeCustom(int seconds, char* buffer);

// 计时器状态管理
BOOL IsTimerRunning(void);
BOOL IsTimeUp(void);
int GetRemainingSeconds(void);
void SetTimerSeconds(int seconds);

// 时间设置
void SetCustomTime(int hours, int minutes, int seconds);

// 全局状态管理
void InitializeGlobalState(void);
void InitializeTimerTime(void);

// 全局状态访问
extern TimerState g_timerState;

#endif // TIMER_CORE_H