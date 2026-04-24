/*
 * Timer Application - Type Definitions
 * Copyright (C) 2024 [Your Name/Company Name]
 * All rights reserved.
 * 
 * See LICENSE.txt for complete licensing information.
 */

#ifndef TIMER_TYPES_H
#define TIMER_TYPES_H

#include <windows.h>
#include <dwmapi.h>

// 菜单ID定义
#define ID_START_PAUSE      1001
#define ID_RESET           1002
#define ID_SET_TIME        1003
#define ID_LANGUAGE        1004
#define ID_FORMAT          1005
#define ID_EXIT            1006
#define ID_INTEGRATED      1007
#define ID_APPEARANCE      1008
#define ID_OVERTIME_COUNT   1009
#define ID_AUDIO_SETTINGS   1010
#define ID_ABOUT           1011
#define ID_GITHUB_REPO     1012

// 系统托盘相关定义
#define WM_TRAYICON         (WM_USER + 1)
#define ID_TRAY_SHOW        1020
#define ID_TRAY_HIDE        1021
#define TRAY_ICON_ID        1

// 格式对话框控件ID
#define ID_FORMAT_CHECK_HOURS      2001
#define ID_FORMAT_CHECK_MINUTES    2002
#define ID_FORMAT_CHECK_SECONDS    2003
#define ID_FORMAT_CHECK_MILLISEC   2004
#define ID_FORMAT_BTN_OK           2005
#define ID_FORMAT_BTN_CANCEL       2006

// 时间设置对话框控件ID
#define ID_SETTIME_EDIT_HOURS      3001
#define ID_SETTIME_EDIT_MINUTES    3002
#define ID_SETTIME_EDIT_SECONDS    3003
#define ID_SETTIME_BTN_OK          3004
#define ID_SETTIME_BTN_CANCEL      3005

// 集成设置对话框控件ID
#define ID_INTEGRATED_CHECK_HOURS     4001
#define ID_INTEGRATED_CHECK_MINUTES   4002
#define ID_INTEGRATED_CHECK_SECONDS   4003
#define ID_INTEGRATED_CHECK_MILLISEC  4004
#define ID_INTEGRATED_EDIT_HOURS      4005
#define ID_INTEGRATED_EDIT_MINUTES    4006
#define ID_INTEGRATED_EDIT_SECONDS    4007
#define ID_INTEGRATED_BTN_OK          4008
#define ID_INTEGRATED_BTN_CANCEL      4009

// 外观设置对话框控件ID
#define ID_APPEARANCE_FONT_COLOR      5001
#define ID_APPEARANCE_BG_COLOR        5002
#define ID_APPEARANCE_OPACITY_SLIDER  5003
#define ID_APPEARANCE_OPACITY_LABEL   5004
#define ID_APPEARANCE_BTN_OK          5005
#define ID_APPEARANCE_BTN_CANCEL      5006
#define ID_APPEARANCE_BTN_RESET       5007
#define ID_APPEARANCE_BTN_RANDOM      5008
#define ID_APPEARANCE_FONT_COMBO      5009
#define ID_APPEARANCE_FONT_BUTTON     5010
#define ID_APPEARANCE_TRANSPARENT_BG  5011

// 音频设置对话框控件ID
#define ID_AUDIO_ENABLE_CHECKBOX      6001
#define ID_AUDIO_CHINESE_RADIO        6002
#define ID_AUDIO_ENGLISH_RADIO        6003
#define ID_AUDIO_CUSTOM_RADIO         6004
#define ID_AUDIO_CUSTOM_PATH_EDIT     6005
#define ID_AUDIO_BROWSE_BUTTON        6006
#define ID_AUDIO_TEST_BUTTON          6007
#define ID_AUDIO_BTN_OK               6008
#define ID_AUDIO_BTN_CANCEL           6009

// 预设时间菜单ID
#define ID_PRESET_TIMES               7000
#define ID_EDIT_PRESETS               7001
#define ID_PRESET_BASE                7100  // 预设时间基础ID，实际预设时间ID从7100开始
#define ID_PRESET_MAX                 7199  // 最大支持100个预设时间

// 预设时间编辑对话框ID
#define ID_PRESET_EDIT_NEW            7201
#define ID_PRESET_EDIT_ADD            7202
#define ID_PRESET_EDIT_DELETE         7203
#define ID_PRESET_EDIT_MODIFY         7204
#define ID_PRESET_EDIT_LIST           7205
#define ID_PRESET_EDIT_OK             7206
#define ID_PRESET_EDIT_CANCEL         7207

// 关于对话框控件ID
#define ID_ABOUT_BTN_OK               8001
#define ID_ABOUT_BTN_FACTORY_RESET    8002

// 语言系统
typedef enum {
    TIMER_LANG_CHINESE = 0,
    TIMER_LANG_ENGLISH = 1
} TimerLanguage;

// 调整边缘定义
#define RESIZE_LEFT   1
#define RESIZE_RIGHT  2
#define RESIZE_TOP    4
#define RESIZE_BOTTOM 8
#define RESIZE_BORDER 8  // 边框调整区域宽度

// 颜色定义
#define COLOR_BLACK RGB(0, 0, 0)
#define COLOR_WHITE RGB(255, 255, 255)
#define COLOR_GREEN RGB(0, 255, 0)
#define COLOR_RED RGB(255, 0, 0)

// ===========================================
// 现代 UI 颜色系统（用于对话框样式优化）
// ===========================================

// 浅色主题颜色
#define UI_LIGHT_BG_PRIMARY     RGB(255, 255, 255)    // 主背景：纯白
#define UI_LIGHT_BG_SECONDARY   RGB(245, 245, 247)    // 次级背景：浅灰
#define UI_LIGHT_SURFACE        RGB(255, 255, 255)    // 表面：纯白
#define UI_LIGHT_BORDER         RGB(220, 220, 225)    // 边框：淡灰
#define UI_LIGHT_BORDER_FOCUS   RGB(100, 149, 237)    // 聚焦边框：矢车菊蓝
#define UI_LIGHT_TEXT_PRIMARY   RGB(28, 28, 30)       // 主文本：近黑
#define UI_LIGHT_TEXT_SECONDARY RGB(92, 92, 97)       // 次要文本：深灰
#define UI_LIGHT_TEXT_DISABLED  RGB(174, 174, 178)    // 禁用文本：浅灰
#define UI_LIGHT_BUTTON_HOVER   RGB(235, 235, 240)    // 按钮悬停：浅灰
#define UI_LIGHT_BUTTON_PRESSED RGB(210, 210, 215)    // 按钮按下：中灰

// 品牌色（主色调）
#define UI_PRIMARY_COLOR        RGB(74, 144, 217)     // 主色蓝
#define UI_PRIMARY_HOVER        RGB(59, 122, 194)     // 主色深蓝
#define UI_PRIMARY_PRESSED      RGB(44, 99, 171)      // 主色更蓝

// 强调色
#define UI_ACCENT_RED         RGB(255, 107, 107)      // 强调红
#define UI_ACCENT_GREEN       RGB(76, 175, 80)        // 强调绿
#define UI_ACCENT_ORANGE      RGB(255, 152, 0)        // 强调橙

// 深色主题颜色（预留）
#define UI_DARK_BG_PRIMARY      RGB(30, 30, 30)       // 主背景：深灰
#define UI_DARK_BG_SECONDARY    RGB(45, 45, 45)       // 次级背景：中灰
#define UI_DARK_SURFACE         RGB(54, 54, 54)       // 表面：深灰
#define UI_DARK_BORDER          RGB(61, 61, 61)       // 边框：暗灰
#define UI_DARK_TEXT_PRIMARY    RGB(255, 255, 255)    // 主文本：纯白
#define UI_DARK_TEXT_SECONDARY  RGB(176, 176, 176)    // 次要文本：浅灰
#define UI_DARK_BUTTON_HOVER    RGB(65, 65, 65)       // 按钮悬停：深灰
#define UI_DARK_BUTTON_PRESSED  RGB(80, 80, 80)       // 按钮按下：中灰

// ===========================================
// Fluent 2 菜单样式颜色系统
// ===========================================

// 浅色主题菜单颜色
#define FLUENT_MENU_LIGHT_BG        RGB(255, 255, 255)    // 菜单背景：纯白
#define FLUENT_MENU_LIGHT_HOVER     RGB(243, 243, 243)    // 悬停背景：浅灰
#define FLUENT_MENU_LIGHT_SEPARATOR RGB(224, 224, 224)    // 分隔线：淡灰
#define FLUENT_MENU_LIGHT_TEXT      RGB(28, 28, 28)       // 文本：近黑
#define FLUENT_MENU_LIGHT_SUBTEXT   RGB(92, 92, 92)       // 次要文本：深灰
#define FLUENT_MENU_LIGHT_BORDER    RGB(220, 220, 220)    // 边框：淡灰
#define FLUENT_MENU_LIGHT_SHADOW    RGB(0, 0, 0)          // 阴影：黑色

// 深色主题菜单颜色
#define FLUENT_MENU_DARK_BG         RGB(32, 32, 32)       // 菜单背景：深灰
#define FLUENT_MENU_DARK_HOVER      RGB(49, 49, 49)       // 悬停背景：中灰
#define FLUENT_MENU_DARK_SEPARATOR  RGB(62, 62, 62)       // 分隔线：暗灰
#define FLUENT_MENU_DARK_TEXT       RGB(255, 255, 255)    // 文本：纯白
#define FLUENT_MENU_DARK_SUBTEXT    RGB(200, 200, 200)    // 次要文本：浅灰
#define FLUENT_MENU_DARK_BORDER     RGB(56, 56, 56)       // 边框：暗灰
#define FLUENT_MENU_DARK_SHADOW     RGB(0, 0, 0)          // 阴影：黑色

// Fluent 2 设计规范
#define FLUENT_MENU_RADIUS_OUTER    8                     // 外圆角：8px
#define FLUENT_MENU_RADIUS_INNER    4                     // 内圆角：4px
#define FLUENT_MENU_PADDING_Y       4                     // 上下内边距：4px
#define FLUENT_MENU_PADDING_X       12                    // 左右内边距：12px
#define FLUENT_MENU_ICON_SIZE       16                    // 图标尺寸：16px
#define FLUENT_MENU_FONT_SIZE       14                    // 字体大小：14px
#define FLUENT_MENU_LINE_HEIGHT     20                    // 行高：20px
#define FLUENT_MENU_ICON_GAP        12                    // 图标与文本间距：12px
#define FLUENT_MENU_SEPARATOR_HEIGHT 8                    // 分隔线高度：8px
#define FLUENT_MENU_CHECKMARK_GAP   8                     // 复选标记间距：8px

// UI 边距定义（用于字体大小计算）
#define UI_TEXT_MARGIN      16    // 文本边距 (像素)
#define UI_SMALL_MARGIN     6     // 小边距 (像素)
#define UI_NO_MARGIN        0     // 无边距

// 缓存容差定义（用于窗口尺寸缓存命中判断）
#define CACHE_TOLERANCE     2     // 缓存容差 (像素)

// DWM相关定义
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWM_WINDOW_CORNER_PREFERENCE
#define DWM_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

// 语言文本定义
typedef struct {
    const wchar_t* startPause;
    const wchar_t* reset;
    const wchar_t* setTime;
    const wchar_t* language;
    const wchar_t* format;
    const wchar_t* exit;
    const wchar_t* start;
    const wchar_t* pause;
    const wchar_t* formatTitle;
    const wchar_t* setTimeTitle;
    const wchar_t* hours;
    const wchar_t* minutes;
    const wchar_t* seconds;
    const wchar_t* milliseconds;
    const wchar_t* ok;
    const wchar_t* cancel;
    const wchar_t* integratedSettings;
    const wchar_t* integratedTitle;
    const wchar_t* appearance;
    const wchar_t* appearanceTitle;
    const wchar_t* overtimeCount;
    const wchar_t* audioSettings;
    const wchar_t* audioTitle;
    const wchar_t* about;
    const wchar_t* aboutTitle;
    // Appearance dialog labels
    const wchar_t* fontColor;
    const wchar_t* backgroundColor;
    const wchar_t* opacity;
    const wchar_t* font;
    const wchar_t* resetButton;
    const wchar_t* randomButton;
    const wchar_t* transparentBackground;
    // Integrated dialog labels
    const wchar_t* timeSettings;
    // Audio dialog labels
    const wchar_t* enableAudio;
    const wchar_t* audioLanguage;
    const wchar_t* chineseAudio;
    const wchar_t* englishAudio;
    const wchar_t* customAudio;
    const wchar_t* browseAudio;
    const wchar_t* testAudio;
    // Error messages
    const wchar_t* errorTitle;
    const wchar_t* errorHoursMax;
    const wchar_t* errorMinutesMax;
    const wchar_t* errorSecondsMax;
    const wchar_t* errorSelectOption;
    // Preset times
    const wchar_t* presetTimes;
    const wchar_t* editPresets;
    const wchar_t* presetEditTitle;
    const wchar_t* newPreset;
    const wchar_t* addPreset;
    const wchar_t* deletePreset;
    const wchar_t* modifyPreset;
    const wchar_t* presetList;
    
    // 恢复出厂设置
    const wchar_t* factoryReset;
    // Hint
    const wchar_t* doubleClickHint;
} MenuTexts;

// 全局状态结构
typedef struct {
    HWND hMainWnd;
    HWND hSetTimeDialog;
    HWND hIntegratedDialog;
    int seconds;
    BOOL isRunning;
    BOOL isTimeUp;
    UINT_PTR timerId;
    // hFont字段已移除，现在使用懒加载字体系统
    int windowWidth;
    int windowHeight;
    BOOL isDragging;
    POINT dragStart;
    TimerLanguage currentLanguage;
    
    // 时间格式化设置
    BOOL showHours;
    BOOL showMinutes;
    BOOL showSeconds;
    BOOL showMilliseconds;
    DWORD startTime; // 用于计算毫秒
    
    // 颜色设置
    COLORREF fontColor;
    COLORREF backgroundColor;
    
    // 透明度设置 (0-255, 255为完全不透明)
    BYTE windowOpacity;
    BOOL transparentBackground;  // 背景透明模式
    
    // 超时正计时功能
    BOOL enableOvertimeCount;
    BOOL isInOvertimeMode;
    COLORREF originalFontColor;
    COLORREF originalBackgroundColor;
    
    // 字体相关
    wchar_t currentFontName[64];
    
    // 音频提示相关
    BOOL enableAudioAlert;
    BOOL useChineseAudio;  // TRUE: 使用中文音频, FALSE: 使用英文音频
    BOOL useCustomAudio;   // TRUE: 使用自定义音频文件
    wchar_t customAudioPath[MAX_PATH];  // 自定义音频文件路径
    
    // 系统托盘相关
    NOTIFYICONDATAW trayIconData;
    BOOL isTrayIconVisible;
    BOOL isMinimized;         // 窗口是否最小化
    BOOL isWindowInForeground; // 窗口是否在前台
    
    // 窗口调整大小相关
    BOOL isResizing;
    int resizeEdge;
    POINT lastMousePos;
    RECT windowRect;
    
    // 预设时间相关
    int presetTimes[10];  // 最多10个预设时间（分钟）
    int presetCount;      // 当前预设时间数量

    // 字体大小计算缓存
    int cachedFontSize;         // 缓存的字体大小结果
    int cachedCalcWidth;        // 缓存对应的窗口宽度
    int cachedCalcHeight;       // 缓存对应的窗口高度
    int cachedCharCount;        // 缓存对应的字符数

    // 参考测量缓存（用于公式计算替代二分查找）
    int refMeasuredCharCount;   // 参考测量时的字符数（0=未测量）
    int refTextWidth;           // 参考字号(100pt)下测得的文本宽度
    int refTextHeight;          // 参考字号(100pt)下测得的文本高度

    // 拖拽节流相关
    DWORD lastResizeRedrawTime; // 上次拖拽重绘的时间戳
    BOOL needsFinalRedraw;      // 拖拽结束后是否需要补偿一次完整重绘

    // 持久化双缓冲位图
    HDC hdcBackBuffer;          // 持久化内存 DC
    HBITMAP hbmBackBuffer;      // 持久化位图
    int backBufferWidth;        // 位图缓存宽度
    int backBufferHeight;       // 位图缓存高度
    BOOL backBufferDirty;       // 位图缓存是否需要重绘

    // 数字位图预渲染缓存
    HBITMAP hbmDigitCache[10];  // 0-9 数字的预渲染位图
    HBITMAP hbmColonCache;      // 冒号预渲染位图
    HBITMAP hbmDotCache;        // 小数点预渲染位图
    int digitCacheFontSize;     // 缓存位图的字体大小
    BOOL digitCacheValid;       // 数字缓存是否有效

    // 帧率控制
    DWORD lastFrameTime;        // 上次渲染的时间戳
    int targetFrameInterval;    // 目标帧间隔 (ms)

    // GDI 对象缓存
    HFONT cachedHFont;                  // 当前选中的字体句柄缓存
    wchar_t cachedFontName[64];         // 缓存的字体名称
    int cachedFontHeight;               // 缓存的字体高度
    COLORREF cachedTextColor;           // 缓存的文本颜色
    BOOL cachedFontValid;               // 字体缓存是否有效

    // UpdateLayeredWindow 相关
    HDC hdcLayeredBuffer;               // 分层窗口位图 DC
    HBITMAP hbmLayeredBuffer;           // 分层窗口位图
    SIZE layeredBufferSize;             // 分层窗口缓冲区尺寸
} TimerState;

// 配置文件路径
#define CONFIG_FILE_PATH L"timer_config.ini"

#endif // TIMER_TYPES_H