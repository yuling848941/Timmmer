/*
 * Timer Application - Audio System
 * Copyright (C) 2024 [Your Name/Company Name]
 * All rights reserved.
 * 
 * Uses Windows Multimedia API (winmm.dll) including MCI interface
 * for enhanced audio format support (WAV/MP3/AAC).
 * 
 * See LICENSE.txt for complete licensing information.
 */

#ifndef TIMER_AUDIO_H
#define TIMER_AUDIO_H

#include "timer_types.h"
#include "timer_embedded_audio.h"
#include <windows.h>

// 音频文件路径定义
#define AUDIO_PATH_CHINESE  L"sound\\您已超时.wav"
#define AUDIO_PATH_ENGLISH  L"sound\\TimeOver.wav"

// 系统音效类型定义
#define SYSTEM_SOUND_TIMEOUT    MB_ICONEXCLAMATION
#define SYSTEM_SOUND_CHINESE    MB_ICONINFORMATION  
#define SYSTEM_SOUND_ENGLISH    MB_ICONASTERISK

// 音频播放函数
void InitializeAudio(void);
void CleanupAudio(void);
BOOL PlayTimeoutAlert(void);
BOOL TestAudioPlayback(BOOL useChineseAudio);
BOOL IsAudioFileExists(BOOL useChineseAudio);

// 音频配置函数
void SetAudioEnabled(BOOL enabled);
void SetAudioLanguage(BOOL useChineseAudio);
void SetCustomAudio(BOOL useCustom, const wchar_t* audioPath);
BOOL IsAudioEnabled(void);
BOOL IsChineseAudioSelected(void);
BOOL IsCustomAudioSelected(void);
const wchar_t* GetCustomAudioPath(void);
BOOL TestCustomAudioPlayback(const wchar_t* audioPath);

#endif // TIMER_AUDIO_H