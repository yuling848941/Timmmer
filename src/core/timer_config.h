#ifndef TIMER_CONFIG_H
#define TIMER_CONFIG_H

#include "timer_types.h"

// 配置文件操作
void SaveFormatConfig(void);
void LoadFormatConfig(void);
void SaveAppearanceConfig(void);
void LoadAppearanceConfig(void);

// 配置项访问
void SetShowHours(BOOL show);
void SetShowMinutes(BOOL show);
void SetShowSeconds(BOOL show);
void SetShowMilliseconds(BOOL show);

BOOL GetShowHours(void);
BOOL GetShowMinutes(void);
BOOL GetShowSeconds(void);
BOOL GetShowMilliseconds(void);

// 语言设置
void SetCurrentLanguage(TimerLanguage lang);
TimerLanguage GetCurrentLanguage(void);

// 菜单文本访问
const MenuTexts* GetMenuTexts(void);

// 音频配置管理
void SaveAudioConfig(void);
void LoadAudioConfig(void);
void SetAudioEnabled(BOOL enabled);
void SetAudioLanguage(BOOL useChineseAudio);
void SetCustomAudio(BOOL useCustom, const wchar_t* audioPath);
BOOL IsAudioEnabled(void);
BOOL IsChineseAudioSelected(void);
BOOL IsCustomAudioSelected(void);
const wchar_t* GetCustomAudioPath(void);

// 预设时间配置管理
void SavePresetConfig(void);
void LoadPresetConfig(void);
void AddPresetTime(int minutes);
void DeletePresetTime(int index);
void ModifyPresetTime(int index, int newMinutes);
int GetPresetCount(void);
int GetPresetTime(int index);
void CycleToNextPresetTime(void);

// 上次使用时间配置管理
void SaveLastUsedTime(int seconds);
int LoadLastUsedTime(void);

// 恢复出厂设置
void PerformFactoryReset(void);

// 窗口位置和大小配置管理
void SaveWindowConfig(int x, int y, int width, int height);
void LoadWindowConfig(int* x, int* y, int* width, int* height);

#endif // TIMER_CONFIG_H