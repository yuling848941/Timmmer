#include "timer_audio.h"
#include "timer_core.h"
#include <mmsystem.h>
#include <stdio.h>

// 外部全局状态引用
extern TimerState g_timerState;

// 简化的音频预加载标志
static BOOL g_audioPreloaded = FALSE;

/**
 * @brief 验证音频文件路径的安全性
 *
 * 检查路径长度、路径遍历攻击、文件扩展名等
 *
 * @param path 要验证的路径
 * @return TRUE 如果路径安全且有效，FALSE 否则
 */
static BOOL ValidateAudioPath(const wchar_t* path) {
    if (!path || wcslen(path) == 0) {
        return FALSE;
    }

    // 检查路径长度
    if (wcslen(path) >= MAX_PATH) {
        return FALSE;
    }

    // 禁止路径遍历（检查连续的 ..）
    if (wcsstr(path, L"..") != NULL) {
        return FALSE;
    }

    // 检查文件扩展名（只允许音频格式）
    const wchar_t* ext = wcsrchr(path, L'.');
    if (!ext) {
        return FALSE;
    }

    // 只允许常见音频格式
    if (_wcsicmp(ext, L".wav") != 0 &&
        _wcsicmp(ext, L".mp3") != 0 &&
        _wcsicmp(ext, L".wma") != 0) {
        return FALSE;
    }

    // 检查文件是否存在且为文件（非目录）
    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }

    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        return FALSE;
    }

    return TRUE;
}

void InitializeAudio(void) {
    // 延迟音频初始化 - 什么都不做，让音频在需要时才加载
    g_audioPreloaded = TRUE;
}



void CleanupAudio(void) {
    // 清理音频资源（如果需要的话）
    // 停止所有正在播放的音频
    PlaySound(NULL, NULL, SND_PURGE);
}

BOOL PlayTimeoutAlert(void) {
    // 检查是否启用音频提示
    if (!g_timerState.enableAudioAlert) {
        return FALSE;
    }
    
    BOOL playedSuccessfully = FALSE;
    
    // 第一优先级：尝试播放自定义音频文件
    if (g_timerState.useCustomAudio && ValidateAudioPath(g_timerState.customAudioPath)) {
        // 安全播放自定义音频
        playedSuccessfully = PlaySoundW(g_timerState.customAudioPath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
    
    // 第二优先级：尝试播放嵌入的音频资源
    if (!playedSuccessfully) {
        int resourceId = g_timerState.useChineseAudio ? 
                        IDR_AUDIO_TIMEOUT_CN : IDR_AUDIO_TIMEOUT_EN;
        playedSuccessfully = PlayEmbeddedAudio(resourceId);
    }
    
    // 第三优先级：尝试播放外部音频文件（如果存在且嵌入资源失败）
    if (!playedSuccessfully && IsAudioFileExists(g_timerState.useChineseAudio)) {
        const wchar_t* audioPath = g_timerState.useChineseAudio ? 
                                   AUDIO_PATH_CHINESE : AUDIO_PATH_ENGLISH;
        // 使用异步播放避免阻塞UI线程
        playedSuccessfully = PlaySoundW(audioPath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
    
    // 第四优先级：使用自定义Beep音效
    if (!playedSuccessfully) {
        CreateSimpleBeep(g_timerState.useChineseAudio);
        playedSuccessfully = TRUE;
    }
    
    // 最后备用：系统音效（理论上不会到达这里）
    if (!playedSuccessfully) {
        UINT soundType = g_timerState.useChineseAudio ? 
                        SYSTEM_SOUND_CHINESE : SYSTEM_SOUND_ENGLISH;
        MessageBeep(soundType);
        return TRUE;
    }
    
    return TRUE;
}

BOOL TestAudioPlayback(BOOL useChineseAudio) {
    BOOL playedSuccessfully = FALSE;
    
    // 第一优先级：尝试播放嵌入的音频资源
    int resourceId = useChineseAudio ? 
                    IDR_AUDIO_TIMEOUT_CN : IDR_AUDIO_TIMEOUT_EN;
    // 对于测试，使用同步播放确保用户听到完整音频
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceId), L"WAVE");
    if (hRes != NULL) {
        HGLOBAL hData = LoadResource(NULL, hRes);
        if (hData != NULL) {
            LPVOID pData = LockResource(hData);
            if (pData != NULL) {
                playedSuccessfully = PlaySoundW((LPCWSTR)pData, NULL, SND_MEMORY | SND_SYNC | SND_NODEFAULT);
            }
        }
    }
    
    // 第二优先级：尝试播放外部音频文件（如果存在且嵌入资源失败）
    if (!playedSuccessfully && IsAudioFileExists(useChineseAudio)) {
        const wchar_t* audioPath = useChineseAudio ? 
                                   AUDIO_PATH_CHINESE : AUDIO_PATH_ENGLISH;
        // 对于测试，使用同步播放确保音频完整播放
        playedSuccessfully = PlaySoundW(audioPath, NULL, SND_FILENAME | SND_SYNC | SND_NODEFAULT);
    }
    
    // 第三优先级：使用自定义Beep音效
    if (!playedSuccessfully) {
        CreateSimpleBeep(useChineseAudio);
        playedSuccessfully = TRUE;
    }
    
    // 最后备用：系统音效
    if (!playedSuccessfully) {
        UINT soundType = useChineseAudio ? 
                        SYSTEM_SOUND_CHINESE : SYSTEM_SOUND_ENGLISH;
        MessageBeep(soundType);
        return TRUE;
    }
    
    return TRUE;
}

BOOL IsAudioFileExists(BOOL useChineseAudio) {
    const wchar_t* audioPath = useChineseAudio ? 
                               AUDIO_PATH_CHINESE : AUDIO_PATH_ENGLISH;
    
    // 检查文件是否存在
    DWORD fileAttributes = GetFileAttributesW(audioPath);
    return (fileAttributes != INVALID_FILE_ATTRIBUTES && 
            !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

void SetAudioEnabled(BOOL enabled) {
    g_timerState.enableAudioAlert = enabled;
}

void SetAudioLanguage(BOOL useChineseAudio) {
    g_timerState.useChineseAudio = useChineseAudio;
}

BOOL IsAudioEnabled(void) {
    return g_timerState.enableAudioAlert;
}

BOOL IsChineseAudioSelected(void) {
    return g_timerState.useChineseAudio;
}

void SetCustomAudio(BOOL useCustom, const wchar_t* audioPath) {
    g_timerState.useCustomAudio = useCustom;
    if (audioPath != NULL) {
        wcscpy_s(g_timerState.customAudioPath, MAX_PATH, audioPath);
    }
}

BOOL IsCustomAudioSelected(void) {
    return g_timerState.useCustomAudio;
}

const wchar_t* GetCustomAudioPath(void) {
    return g_timerState.customAudioPath;
}

BOOL TestCustomAudioPlayback(const wchar_t* audioPath) {
    if (!audioPath || wcslen(audioPath) == 0) {
        return FALSE;
    }
    
    // 检查文件是否存在
    DWORD fileAttributes = GetFileAttributesW(audioPath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    // 尝试播放音频文件
    BOOL result = PlaySoundW(audioPath, NULL, SND_FILENAME | SND_ASYNC);
    return result;
}