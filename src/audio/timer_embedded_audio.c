#include "timer_embedded_audio.h"
#include <mmsystem.h>

BOOL PlayEmbeddedAudio(int resourceId) {
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceId), L"WAVE");
    if (hRes == NULL) {
        return FALSE;
    }
    
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (hData == NULL) {
        return FALSE;
    }
    
    LPVOID pData = LockResource(hData);
    if (pData == NULL) {
        return FALSE;
    }
    
    // 播放嵌入的WAV资源 - 使用异步播放避免阻塞UI线程
    BOOL result = PlaySoundW((LPCWSTR)pData, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    
    return result;
}

BOOL HasEmbeddedAudio(int resourceId) {
    // 检查是否存在指定的音频资源
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceId), L"WAVE");
    return (hRes != NULL);
}

void CreateSimpleBeep(BOOL isChineseStyle) {
    // 创建简单的提示音效果
    if (isChineseStyle) {
        // 中文风格：两声短促的提示音
        Beep(800, 200);
        Sleep(100);
        Beep(1000, 200);
    } else {
        // 英文风格：一声较长的提示音
        Beep(1000, 500);
    }
}