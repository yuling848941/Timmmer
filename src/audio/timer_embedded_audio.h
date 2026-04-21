#ifndef TIMER_EMBEDDED_AUDIO_H
#define TIMER_EMBEDDED_AUDIO_H

#include <windows.h>

// 嵌入式音频资源ID定义
#define IDR_AUDIO_TIMEOUT_CN    1001
#define IDR_AUDIO_TIMEOUT_EN    1002

// 嵌入式音频播放函数
BOOL PlayEmbeddedAudio(int resourceId);
BOOL HasEmbeddedAudio(int resourceId);

// 创建简单的提示音
void CreateSimpleBeep(BOOL isChineseStyle);

#endif // TIMER_EMBEDDED_AUDIO_H