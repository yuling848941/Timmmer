#include "timer_config.h"
#include "timer_core.h"
#include <stdio.h>
#include <shlobj.h>

static void BuildConfigPath(wchar_t* outPath, DWORD maxLen) {
    // 获取用户 profile 路径
    DWORD result = GetEnvironmentVariableW(L"USERPROFILE", outPath, maxLen);
    if (result == 0 || result >= maxLen) {
        if (FAILED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, outPath))) {
            GetCurrentDirectoryW(maxLen, outPath);
        }
    }

    // 计算拼接所需总长度
    size_t currentLen = wcslen(outPath);
    size_t requiredLen = currentLen + 1 + wcslen(CONFIG_FILE_PATH);  // 路径 + \ + 文件名

    // 检查是否会溢出
    if (requiredLen >= maxLen) {
        // 降级：使用当前目录
        GetCurrentDirectoryW(maxLen, outPath);
        currentLen = wcslen(outPath);
        requiredLen = currentLen + 1 + wcslen(CONFIG_FILE_PATH);

        if (requiredLen >= maxLen) {
            // 仍然不够，直接使用文件名（不安全的降级，但不会崩溃）
            wcscpy_s(outPath, maxLen, CONFIG_FILE_PATH);
            return;
        }
    }

    // 安全拼接
    if (currentLen > 0 && outPath[currentLen - 1] != L'\\') {
        wcscat_s(outPath, maxLen, L"\\");
    }
    wcscat_s(outPath, maxLen, CONFIG_FILE_PATH);
}

// 语言文本定义
static const MenuTexts g_menuTexts[2] = {
    // 中文
    {
        L"开始/暂停",
        L"重置",
        L"设置时间",
        L"English",
        L"时间格式",
        L"退出",
        L"开始",
        L"暂停",
        L"时间显示格式",
        L"自定义时间设置",
        L"小时 (H)",
        L"分钟 (M)",
        L"秒 (S)",
        L"毫秒 (MS)",
        L"确定",
        L"取消",
        L"时间设置",
        L"时间设置",
        L"外观设置",
        L"外观设置 - 字体与颜色",
        L"超时正计时",
        L"音频设置",
        L"音频设置",
        L"关于",
        L"关于计时器",
        // Appearance dialog labels
        L"字体颜色:",
        L"背景颜色:",
        L"透明度:",
        L"字体:",
        L"重置",
        L"随机",
        L"背景透明",
        // Integrated dialog labels
        L"时间设置",
        // Audio dialog labels
        L"启用音频提示",
        L"音频语言:",
        L"中文音频",
        L"英文音频",
        L"自定义音频",
        L"浏览...",
        L"测试音频",
        // Error messages
        L"输入错误",
        L"小时数不能超过99",
        L"分钟数不能超过60",
        L"秒数不能超过60",
        L"请至少选择一个时间显示选项",
        // Preset times
        L"预设时间",
        L"编辑预设",
        L"编辑预设时间",
        L"新预设(分钟):",
        L"添加",
        L"删除",
        L"修改",
        L"预设列表:",
        // 恢复出厂设置
        L"恢复出厂设置",
        L"提示：双击主窗口可快速切换预设时间"
    },
    // 英文
    {
        L"Start/Pause",
        L"Reset",
        L"Set Time",
        L"中文",
        L"Time Format",
        L"Exit",
        L"Start",
        L"Pause",
        L"Time Display Format",
        L"Custom Time Setting",
        L"Hours (H)",
        L"Minutes (M)",
        L"Seconds (S)",
        L"Milliseconds (MS)",
        L"OK",
        L"Cancel",
        L"Time Settings",
        L"Time Settings",
        L"Appearance",
        L"Appearance Settings - Font & Colors",
        L"Overtime Count",
        L"Audio Settings",
        L"Audio Settings",
        L"About",
        L"About Timer",
        // Appearance dialog labels
        L"Font Color:",
        L"Background Color:",
        L"Opacity:",
        L"Font:",
        L"Reset",
        L"Random",
        L"Transparent Background",
        // Integrated dialog labels
        L"Time Settings",
        // Audio dialog labels
        L"Enable Audio Alert",
        L"Audio Language:",
        L"Chinese Audio",
        L"English Audio",
        L"Custom Audio",
        L"Browse...",
        L"Test Audio",
        // Error messages
        L"Input Error",
        L"Hours cannot exceed 99",
        L"Minutes cannot exceed 60",
        L"Seconds cannot exceed 60",
        L"Please select at least one time display option",
        // Preset times
        L"Preset Times",
        L"Edit Presets",
        L"Edit Preset Times",
        L"New Preset (minutes):",
        L"Add",
        L"Delete",
        L"Modify",
        L"Preset List:",
        // Factory Reset
        L"Factory Reset",
        L"Tip: Double-click main window to cycle presets"
    }
};

void SaveFormatConfig(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    WritePrivateProfileStringW(L"Format", L"ShowHours", g_timerState.showHours ? L"1" : L"0", configPath);
    WritePrivateProfileStringW(L"Format", L"ShowMinutes", g_timerState.showMinutes ? L"1" : L"0", configPath);
    WritePrivateProfileStringW(L"Format", L"ShowSeconds", g_timerState.showSeconds ? L"1" : L"0", configPath);
    WritePrivateProfileStringW(L"Format", L"ShowMilliseconds", g_timerState.showMilliseconds ? L"1" : L"0", configPath);
    
    // 保存语言设置
    wchar_t langBuffer[16];
    swprintf(langBuffer, 16, L"%d", g_timerState.currentLanguage);
    WritePrivateProfileStringW(L"General", L"Language", langBuffer, configPath);
}

void LoadFormatConfig(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    // 检查配置文件是否存在
    DWORD fileAttr = GetFileAttributesW(configPath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        // 文件不存在，使用默认设置
        g_timerState.showHours = TRUE;
        g_timerState.showMinutes = TRUE;
        g_timerState.showSeconds = TRUE;
        g_timerState.showMilliseconds = FALSE;
        g_timerState.currentLanguage = TIMER_LANG_CHINESE; // 默认中文
        return;
    }
    
    g_timerState.showHours = GetPrivateProfileIntW(L"Format", L"ShowHours", 1, configPath) != 0;
    g_timerState.showMinutes = GetPrivateProfileIntW(L"Format", L"ShowMinutes", 1, configPath) != 0;
    g_timerState.showSeconds = GetPrivateProfileIntW(L"Format", L"ShowSeconds", 1, configPath) != 0;
    g_timerState.showMilliseconds = GetPrivateProfileIntW(L"Format", L"ShowMilliseconds", 0, configPath) != 0;
    
    // 加载语言设置
    g_timerState.currentLanguage = (TimerLanguage)GetPrivateProfileIntW(L"General", L"Language", TIMER_LANG_CHINESE, configPath);
}

void SetShowHours(BOOL show) {
    g_timerState.showHours = show;
}

void SetShowMinutes(BOOL show) {
    g_timerState.showMinutes = show;
}

void SetShowSeconds(BOOL show) {
    g_timerState.showSeconds = show;
}

void SetShowMilliseconds(BOOL show) {
    g_timerState.showMilliseconds = show;
}

BOOL GetShowHours(void) {
    return g_timerState.showHours;
}

BOOL GetShowMinutes(void) {
    return g_timerState.showMinutes;
}

BOOL GetShowSeconds(void) {
    return g_timerState.showSeconds;
}

BOOL GetShowMilliseconds(void) {
    return g_timerState.showMilliseconds;
}

void SetCurrentLanguage(TimerLanguage lang) {
    g_timerState.currentLanguage = lang;
}

TimerLanguage GetCurrentLanguage(void) {
    return g_timerState.currentLanguage;
}

const MenuTexts* GetMenuTexts(void) {
    return &g_menuTexts[g_timerState.currentLanguage];
}

void SaveAppearanceConfig(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    COLORREF fontColorToSave = g_timerState.isInOvertimeMode ? g_timerState.originalFontColor : g_timerState.fontColor;
    COLORREF bgColorToSave = g_timerState.isInOvertimeMode ? g_timerState.originalBackgroundColor : g_timerState.backgroundColor;
    
    wchar_t colorBuffer[32];
    swprintf(colorBuffer, 32, L"%d", fontColorToSave);
    WritePrivateProfileStringW(L"Appearance", L"FontColor", colorBuffer, configPath);
    
    swprintf(colorBuffer, 32, L"%d", bgColorToSave);
    WritePrivateProfileStringW(L"Appearance", L"BackgroundColor", colorBuffer, configPath);
    
    // 保存透明度
    swprintf(colorBuffer, 32, L"%d", g_timerState.windowOpacity);
    WritePrivateProfileStringW(L"Appearance", L"WindowOpacity", colorBuffer, configPath);
    
    // 保存字体名称
    WritePrivateProfileStringW(L"Appearance", L"FontName", g_timerState.currentFontName, configPath);
    
    // 保存背景透明设置
    WritePrivateProfileStringW(L"Appearance", L"TransparentBackground", 
                              g_timerState.transparentBackground ? L"1" : L"0", configPath);
    
    // 保存超时正计时功能开关
    WritePrivateProfileStringW(L"Timer", L"EnableOvertimeCount", 
                              g_timerState.enableOvertimeCount ? L"1" : L"0", configPath);
    
    // 保存原始字体颜色
    swprintf(colorBuffer, 32, L"%d", g_timerState.originalFontColor);
    WritePrivateProfileStringW(L"Timer", L"OriginalFontColor", colorBuffer, configPath);
    
    // 保存原始背景颜色
    swprintf(colorBuffer, 32, L"%d", g_timerState.originalBackgroundColor);
    WritePrivateProfileStringW(L"Timer", L"OriginalBackgroundColor", colorBuffer, configPath);
    
    // 保存音频设置
    wchar_t audioBuffer[16];
    swprintf(audioBuffer, 16, L"%d", g_timerState.enableAudioAlert ? 1 : 0);
    WritePrivateProfileStringW(L"Audio", L"EnableAudioAlert", audioBuffer, configPath);
    
    swprintf(audioBuffer, 16, L"%d", g_timerState.useChineseAudio ? 1 : 0);
    WritePrivateProfileStringW(L"Audio", L"UseChineseAudio", audioBuffer, configPath);
    
    swprintf(audioBuffer, 16, L"%d", g_timerState.useCustomAudio ? 1 : 0);
    WritePrivateProfileStringW(L"Audio", L"UseCustomAudio", audioBuffer, configPath);
    
    WritePrivateProfileStringW(L"Audio", L"CustomAudioPath", g_timerState.customAudioPath, configPath);
}

void LoadAppearanceConfig(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    // 检查配置文件是否存在
    DWORD fileAttr = GetFileAttributesW(configPath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        // 文件不存在，使用默认设置
        g_timerState.fontColor = RGB(255, 255, 255);  // 白色字体
        g_timerState.backgroundColor = RGB(0, 0, 0);  // 黑色背景
        g_timerState.windowOpacity = 255;  // 完全不透明
        wcscpy_s(g_timerState.currentFontName, 64, L"");  // 默认无字体选择
        
        // 超时正计时功能默认设置
        g_timerState.enableOvertimeCount = FALSE;
        g_timerState.isInOvertimeMode = FALSE;
        g_timerState.originalFontColor = RGB(255, 255, 255);
        g_timerState.originalBackgroundColor = RGB(0, 0, 0);
        
        // 音频设置默认值
        g_timerState.enableAudioAlert = FALSE;  // 默认关闭音频提示
        g_timerState.useChineseAudio = TRUE;    // 默认使用中文音频
        g_timerState.useCustomAudio = FALSE;    // 默认不使用自定义音频
        wcscpy_s(g_timerState.customAudioPath, MAX_PATH, L"");  // 默认无自定义音频路径
        
        // 背景透明设置默认值
        g_timerState.transparentBackground = FALSE;  // 默认不透明背景
        return;
    }
    
    // 加载字体颜色
    DWORD fontColor = GetPrivateProfileIntW(L"Appearance", L"FontColor", RGB(255, 255, 255), configPath);
    g_timerState.fontColor = (COLORREF)fontColor;
    
    // 加载背景颜色
    DWORD backgroundColor = GetPrivateProfileIntW(L"Appearance", L"BackgroundColor", RGB(0, 0, 0), configPath);
    g_timerState.backgroundColor = (COLORREF)backgroundColor;
    
    // 加载透明度
    DWORD opacity = GetPrivateProfileIntW(L"Appearance", L"WindowOpacity", 255, configPath);
    g_timerState.windowOpacity = (BYTE)opacity;
    
    // 加载字体名称
    GetPrivateProfileStringW(L"Appearance", L"FontName", L"", g_timerState.currentFontName, 64, configPath);
    
    // 加载超时正计时功能开关
    DWORD enableOvertimeCount = GetPrivateProfileIntW(L"Timer", L"EnableOvertimeCount", 0, configPath);
    g_timerState.enableOvertimeCount = (enableOvertimeCount != 0);
    
    // 加载原始字体颜色
    DWORD originalFontColor = GetPrivateProfileIntW(L"Timer", L"OriginalFontColor", RGB(255, 255, 255), configPath);
    g_timerState.originalFontColor = (COLORREF)originalFontColor;
    
    // 加载原始背景颜色
    DWORD originalBackgroundColor = GetPrivateProfileIntW(L"Timer", L"OriginalBackgroundColor", RGB(0, 0, 0), configPath);
    g_timerState.originalBackgroundColor = (COLORREF)originalBackgroundColor;
    
    // 确保不在超时模式（程序启动时重置状态）
    g_timerState.isInOvertimeMode = FALSE;
    
    // 加载音频设置
    DWORD enableAudioAlert = GetPrivateProfileIntW(L"Audio", L"EnableAudioAlert", 0, configPath);
    g_timerState.enableAudioAlert = (enableAudioAlert != 0);
    
    DWORD useChineseAudio = GetPrivateProfileIntW(L"Audio", L"UseChineseAudio", 1, configPath);
    g_timerState.useChineseAudio = (useChineseAudio != 0);
    
    DWORD useCustomAudio = GetPrivateProfileIntW(L"Audio", L"UseCustomAudio", 0, configPath);
    g_timerState.useCustomAudio = (useCustomAudio != 0);
    
    GetPrivateProfileStringW(L"Audio", L"CustomAudioPath", L"", g_timerState.customAudioPath, MAX_PATH, configPath);
    
    // 加载背景透明设置
    DWORD transparentBackground = GetPrivateProfileIntW(L"Appearance", L"TransparentBackground", 0, configPath);
    g_timerState.transparentBackground = (transparentBackground != 0);
}

// 预设时间配置管理函数
void SavePresetConfig(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    // 保存预设时间数量
    wchar_t countBuffer[16];
    swprintf(countBuffer, 16, L"%d", g_timerState.presetCount);
    WritePrivateProfileStringW(L"Presets", L"Count", countBuffer, configPath);
    
    // 保存每个预设时间
    for (int i = 0; i < g_timerState.presetCount; i++) {
        wchar_t keyName[32];
        wchar_t valueBuffer[16];
        swprintf(keyName, 32, L"Preset%d", i);
        swprintf(valueBuffer, 16, L"%d", g_timerState.presetTimes[i]);
        WritePrivateProfileStringW(L"Presets", keyName, valueBuffer, configPath);
    }
}

void LoadPresetConfig(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    // 检查配置文件是否存在
    DWORD fileAttr = GetFileAttributesW(configPath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        // 文件不存在，使用默认预设时间
        g_timerState.presetCount = 3;
        g_timerState.presetTimes[0] = 10;  // 10分钟
        g_timerState.presetTimes[1] = 15;  // 15分钟
        g_timerState.presetTimes[2] = 20;  // 20分钟
        return;
    }
    
    // 读取预设时间数量
    g_timerState.presetCount = GetPrivateProfileIntW(L"Presets", L"Count", 4, configPath);
    
    // 限制预设时间数量
    if (g_timerState.presetCount > 10) {
        g_timerState.presetCount = 10;
    }
    if (g_timerState.presetCount < 0) {
        g_timerState.presetCount = 0;
    }
    
    // 读取每个预设时间
    for (int i = 0; i < g_timerState.presetCount; i++) {
        wchar_t keyName[32];
        swprintf(keyName, 32, L"Preset%d", i);
        g_timerState.presetTimes[i] = GetPrivateProfileIntW(L"Presets", keyName, 25, configPath);
    }
    
    // 如果没有预设时间，设置默认值
        if (g_timerState.presetCount == 0) {
            g_timerState.presetCount = 3;
            g_timerState.presetTimes[0] = 10;
            g_timerState.presetTimes[1] = 15;
            g_timerState.presetTimes[2] = 20;
    }
}

void AddPresetTime(int minutes) {
    if (g_timerState.presetCount < 10 && minutes > 0) {
        g_timerState.presetTimes[g_timerState.presetCount] = minutes;
        g_timerState.presetCount++;
        SavePresetConfig();
    }
}

void DeletePresetTime(int index) {
    if (index >= 0 && index < g_timerState.presetCount) {
        // 将后面的元素前移
        for (int i = index; i < g_timerState.presetCount - 1; i++) {
            g_timerState.presetTimes[i] = g_timerState.presetTimes[i + 1];
        }
        g_timerState.presetCount--;
        SavePresetConfig();
    }
}

void ModifyPresetTime(int index, int newMinutes) {
    if (index >= 0 && index < g_timerState.presetCount && newMinutes > 0) {
        g_timerState.presetTimes[index] = newMinutes;
        SavePresetConfig();
    }
}

int GetPresetCount(void) {
    return g_timerState.presetCount;
}

int GetPresetTime(int index) {
    if (index >= 0 && index < g_timerState.presetCount) {
        return g_timerState.presetTimes[index];
    }
    return 0;
}

// 循环切换到下一个预设时间
void CycleToNextPresetTime(void) {
    if (g_timerState.presetCount == 0) {
        return; // 没有预设时间
    }
    
    // 获取当前计时器的时间（秒）
    int currentSeconds = g_timerState.seconds;
    int currentMinutes = currentSeconds / 60;
    
    // 查找当前时间在预设时间中的位置
    int currentIndex = -1;
    for (int i = 0; i < g_timerState.presetCount; i++) {
        if (g_timerState.presetTimes[i] == currentMinutes) {
            currentIndex = i;
            break;
        }
    }
    
    // 切换到下一个预设时间
    int nextIndex;
    if (currentIndex == -1) {
        // 当前时间不在预设时间中，切换到第一个预设时间
        nextIndex = 0;
    } else {
        // 循环到下一个预设时间
        nextIndex = (currentIndex + 1) % g_timerState.presetCount;
    }
    
    // 应用新的预设时间
    int newMinutes = g_timerState.presetTimes[nextIndex];
    SetTimerSeconds(newMinutes * 60);
}

// 音频配置管理函数
void SaveAudioConfig(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    wchar_t audioBuffer[16];
    swprintf(audioBuffer, 16, L"%d", g_timerState.enableAudioAlert ? 1 : 0);
    WritePrivateProfileStringW(L"Audio", L"EnableAudioAlert", audioBuffer, configPath);
    
    swprintf(audioBuffer, 16, L"%d", g_timerState.useChineseAudio ? 1 : 0);
    WritePrivateProfileStringW(L"Audio", L"UseChineseAudio", audioBuffer, configPath);
    
    swprintf(audioBuffer, 16, L"%d", g_timerState.useCustomAudio ? 1 : 0);
    WritePrivateProfileStringW(L"Audio", L"UseCustomAudio", audioBuffer, configPath);
    
    WritePrivateProfileStringW(L"Audio", L"CustomAudioPath", g_timerState.customAudioPath, configPath);
}

void LoadAudioConfig(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    // 检查配置文件是否存在
    DWORD fileAttr = GetFileAttributesW(configPath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        // 文件不存在，使用默认设置
        g_timerState.enableAudioAlert = FALSE;  // 默认关闭音频提示
        g_timerState.useChineseAudio = TRUE;    // 默认使用中文音频
        g_timerState.useCustomAudio = FALSE;    // 默认不使用自定义音频
        wcscpy_s(g_timerState.customAudioPath, MAX_PATH, L"");  // 默认无自定义音频路径
        return;
    }
    
    DWORD enableAudioAlert = GetPrivateProfileIntW(L"Audio", L"EnableAudioAlert", 0, configPath);
    g_timerState.enableAudioAlert = (enableAudioAlert != 0);
    
    DWORD useChineseAudio = GetPrivateProfileIntW(L"Audio", L"UseChineseAudio", 1, configPath);
    g_timerState.useChineseAudio = (useChineseAudio != 0);
    
    DWORD useCustomAudio = GetPrivateProfileIntW(L"Audio", L"UseCustomAudio", 0, configPath);
    g_timerState.useCustomAudio = (useCustomAudio != 0);
    
    GetPrivateProfileStringW(L"Audio", L"CustomAudioPath", L"", g_timerState.customAudioPath, MAX_PATH, configPath);
}

// 保存上次使用的时间
void SaveLastUsedTime(int seconds) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    wchar_t timeBuffer[16];
    swprintf(timeBuffer, 16, L"%d", seconds);
    WritePrivateProfileStringW(L"Timer", L"LastUsedTime", timeBuffer, configPath);
}

// 加载上次使用的时间
int LoadLastUsedTime(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    // 检查配置文件是否存在
    DWORD fileAttr = GetFileAttributesW(configPath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        // 文件不存在，返回-1表示没有上次使用时间
        return -1;
    }
    
    // 加载上次使用时间，如果没有则返回-1
    int lastTime = GetPrivateProfileIntW(L"Timer", L"LastUsedTime", -1, configPath);
    return lastTime;
}

// 恢复出厂设置
void PerformFactoryReset(void) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    // 删除配置文件
    DeleteFileW(configPath);
    
    // 重置所有设置到默认值
    // 格式设置
    g_timerState.showHours = TRUE;
    g_timerState.showMinutes = TRUE;
    g_timerState.showSeconds = TRUE;
    g_timerState.showMilliseconds = FALSE;
    g_timerState.currentLanguage = TIMER_LANG_CHINESE;
    
    // 外观设置
    g_timerState.fontColor = RGB(255, 255, 255);  // 白色字体
    g_timerState.backgroundColor = RGB(0, 0, 0);  // 黑色背景
    g_timerState.windowOpacity = 255;  // 完全不透明
    wcscpy_s(g_timerState.currentFontName, 64, L"");  // 默认无字体选择
    
    // 超时正计时功能
    g_timerState.enableOvertimeCount = FALSE;
    g_timerState.isInOvertimeMode = FALSE;
    g_timerState.originalFontColor = RGB(255, 255, 255);
    g_timerState.originalBackgroundColor = RGB(0, 0, 0);
    
    // 音频设置
    g_timerState.enableAudioAlert = FALSE;
    g_timerState.useChineseAudio = TRUE;
    g_timerState.useCustomAudio = FALSE;
    wcscpy_s(g_timerState.customAudioPath, MAX_PATH, L"");
    
    // 预设时间
    g_timerState.presetCount = 3;
    g_timerState.presetTimes[0] = 10;  // 10分钟
    g_timerState.presetTimes[1] = 15;  // 15分钟
    g_timerState.presetTimes[2] = 20;  // 20分钟
    
    // 保存默认设置
    SaveFormatConfig();
    SaveAppearanceConfig();
    SaveAudioConfig();
    SavePresetConfig();
}

// 保存窗口位置和大小
void SaveWindowConfig(int x, int y, int width, int height) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    
    wchar_t buffer[32];
    
    // 保存窗口位置
    swprintf(buffer, 32, L"%d", x);
    WritePrivateProfileStringW(L"Window", L"X", buffer, configPath);
    
    swprintf(buffer, 32, L"%d", y);
    WritePrivateProfileStringW(L"Window", L"Y", buffer, configPath);
    
    // 保存窗口大小
    swprintf(buffer, 32, L"%d", width);
    WritePrivateProfileStringW(L"Window", L"Width", buffer, configPath);
    
    swprintf(buffer, 32, L"%d", height);
    WritePrivateProfileStringW(L"Window", L"Height", buffer, configPath);
}

// 加载窗口位置和大小
void LoadWindowConfig(int* x, int* y, int* width, int* height) {
    wchar_t configPath[MAX_PATH];
    BuildConfigPath(configPath, MAX_PATH);
    
    // 检查配置文件是否存在
    DWORD fileAttr = GetFileAttributesW(configPath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        // 文件不存在，使用默认值（居中显示）
        *x = CW_USEDEFAULT;
        *y = CW_USEDEFAULT;
        *width = 300;  // 默认宽度
        *height = 150; // 默认高度
        return;
    }
    
    // 加载窗口位置和大小
    *x = GetPrivateProfileIntW(L"Window", L"X", CW_USEDEFAULT, configPath);
    *y = GetPrivateProfileIntW(L"Window", L"Y", CW_USEDEFAULT, configPath);
    *width = GetPrivateProfileIntW(L"Window", L"Width", 300, configPath);
    *height = GetPrivateProfileIntW(L"Window", L"Height", 150, configPath);
    
    // 验证窗口大小的合理性
    if (*width < 100) *width = 300;
    if (*height < 50) *height = 150;
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 确保窗口不会超出屏幕范围
    if (*x != CW_USEDEFAULT && (*x < 0 || *x + *width > screenWidth)) {
        *x = (screenWidth - *width) / 2;
    }
    if (*y != CW_USEDEFAULT && (*y < 0 || *y + *height > screenHeight)) {
        *y = (screenHeight - *height) / 2;
    }
}