#include "timer_fonts.h"
#include "timer_font_resources.h"
#include "timer_font_manager.h"
#include "timer_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 全局字体管理器
FontManager g_fontManager = {0};
static BOOL g_fontsInitialized = FALSE;

// Catime项目字体定义 - 只包含系统中实际可用的字体
const FontInfo g_catimeFonts[] = {
    // 系统默认等宽字体
    {L"Consolas", L"Consolas", FONT_TYPE_MONOSPACE, FALSE},
    {L"Courier New", L"Courier New", FONT_TYPE_MONOSPACE, FALSE},
    
    // 系统中可用的Cascadia字体系列
    {L"Cascadia Code", L"Cascadia Code", FONT_TYPE_MONOSPACE, FALSE},
    {L"Cascadia Mono", L"Cascadia Mono", FONT_TYPE_MONOSPACE, FALSE},
    
    // 系统默认字体作为回退选项
    {L"Arial", L"Arial", FONT_TYPE_SANS_SERIF, FALSE},
    {L"Times New Roman", L"Times New Roman", FONT_TYPE_SERIF, FALSE},
    {L"Segoe UI", L"Segoe UI", FONT_TYPE_SANS_SERIF, FALSE},
    {L"Tahoma", L"Tahoma", FONT_TYPE_SANS_SERIF, FALSE}
};

const int g_catimeFontCount = sizeof(g_catimeFonts) / sizeof(g_catimeFonts[0]);

// 内部函数声明
static HFONT CreateFontInternal(const wchar_t* fontName, int size, BOOL bold, BOOL italic);
static int FindCacheIndex(const wchar_t* fontName, int size, BOOL bold, BOOL italic);
static void AddToCache(HFONT hFont, const wchar_t* fontName, int size, BOOL bold, BOOL italic);
static void ClearCacheEntry(int index);

/**
 * 初始化字体系统
 */
BOOL InitializeTimerFonts(void) {
    if (g_fontsInitialized) {
        return TRUE;
    }
    
    // 初始化字体资源系统
    if (!InitializeFontResources()) {
        return FALSE;
    }
    
    // 初始化字体缓存
    for (int i = 0; i < 16; i++) {
        g_fontManager.cache[i].hFont = NULL;
        g_fontManager.cache[i].fontName[0] = L'\0';
        g_fontManager.cache[i].size = 0;
        g_fontManager.cache[i].bold = FALSE;
        g_fontManager.cache[i].italic = FALSE;
        g_fontManager.cache[i].isValid = FALSE;
    }
    g_fontManager.cacheCount = 0;
    
    g_fontsInitialized = TRUE;
    return TRUE;
}

/**
 * 清理字体系统
 */
void CleanupTimerFonts(void) {
    if (!g_fontsInitialized) {
        return;
    }
    
    ClearFontCache();
    
    // 清理字体资源系统
    CleanupFontResources();
    
    g_fontsInitialized = FALSE;
}

/**
 * 获取字体（懒加载）
 */
HFONT GetTimerFont(const wchar_t* fontName, int size, BOOL bold, BOOL italic) {
    if (!g_fontsInitialized) {
        InitializeTimerFonts();
    }
    
    if (!fontName || size <= 0) {
        return NULL;
    }
    
    // 检查缓存
    int cacheIndex = FindCacheIndex(fontName, size, bold, italic);
    if (cacheIndex >= 0 && g_fontManager.cache[cacheIndex].isValid) {
        return g_fontManager.cache[cacheIndex].hFont;
    }
    
    // 检查字体是否可用，如果不可用则使用回退字体
    const wchar_t* actualFontName = fontName;
    if (!IsFontInstalled(fontName)) {
        // 查找字体类型并获取回退字体
        const FontInfo* fontInfo = FindFontInfo(fontName);
        if (fontInfo) {
            actualFontName = GetFallbackFont(fontInfo->type);
        } else {
            // 如果找不到字体信息，默认使用等宽字体回退
            actualFontName = GetFallbackFont(FONT_TYPE_MONOSPACE);
        }

    }
    
    // 创建新字体
    HFONT hFont = CreateFontInternal(actualFontName, size, bold, italic);
    if (hFont) {
        // 添加到缓存（使用原始字体名称作为键）
        AddToCache(hFont, fontName, size, bold, italic);
    }
    
    return hFont;
}

/**
 * 获取默认字体
 */
HFONT GetDefaultTimerFont(int size) {
    // 优先使用系统中可用的等宽字体
    HFONT hFont = GetTimerFont(L"Cascadia Code", size, FALSE, FALSE);
    if (!hFont) {
        hFont = GetTimerFont(L"Consolas", size, FALSE, FALSE);
    }
    if (!hFont) {
        hFont = GetTimerFont(L"Courier New", size, FALSE, FALSE);
    }
    return hFont;
}

/**
 * 获取等宽字体
 */
HFONT GetMonospaceFont(int size) {
    // 使用系统中可用的等宽字体
    const wchar_t* monospaceFonts[] = {
        L"Cascadia Code",
        L"Cascadia Mono",
        L"Consolas",
        L"Courier New"
    };
    
    for (int i = 0; i < 4; i++) {
        if (IsFontInstalled(monospaceFonts[i])) {
            return GetTimerFont(monospaceFonts[i], size, FALSE, FALSE);
        }
    }
    
    // 如果都不可用，使用系统默认等宽字体
    return GetTimerFont(L"Courier New", size, FALSE, FALSE);
}

/**
 * 查找字体信息
 */
const FontInfo* FindFontInfo(const wchar_t* fontName) {
    if (!fontName) {
        return NULL;
    }
    
    for (int i = 0; i < g_catimeFontCount; i++) {
        if (_wcsicmp(g_catimeFonts[i].name, fontName) == 0) {
            return &g_catimeFonts[i];
        }
    }
    
    return NULL;
}

/**
 * 根据索引获取字体信息
 */
const FontInfo* GetFontInfoByIndex(int index) {
    if (index >= 0 && index < g_catimeFontCount) {
        return &g_catimeFonts[index];
    }
    return NULL;
}

/**
 * 获取可用字体数量
 */
int GetAvailableFontCount(void) {
    return g_catimeFontCount;
}

/**
 * 检查字体是否已安装
 */
BOOL IsFontInstalled(const wchar_t* fontName) {
    if (!fontName) {
        return FALSE;
    }
    
    HDC hdc = GetDC(NULL);
    if (!hdc) {
        return FALSE;
    }
    
    HFONT hFont = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontName
    );
    
    BOOL isInstalled = FALSE;
    if (hFont) {
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        
        wchar_t actualFontName[LF_FACESIZE];
        GetTextFaceW(hdc, LF_FACESIZE, actualFontName);
        
        isInstalled = (_wcsicmp(actualFontName, fontName) == 0);
        
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }
    
    ReleaseDC(NULL, hdc);
    return isInstalled;
}

/**
 * 获取回退字体
 */
const wchar_t* GetFallbackFont(FontType type) {
    switch (type) {
        case FONT_TYPE_MONOSPACE:
            return L"JetBrains Mono";
        case FONT_TYPE_DISPLAY:
            return L"Pixelify Sans";
        case FONT_TYPE_DEFAULT:
        default:
            return L"JetBrains Mono";
    }
}

/**
 * 创建字体（原有函数，保持兼容性）
 */
HFONT CreateTimerFont(const wchar_t* fontName, int size, BOOL bold, BOOL italic) {
    if (!g_fontsInitialized) {
        InitializeTimerFonts();
    }
    
    // 检查缓存
    int cacheIndex = FindCacheIndex(fontName, size, bold, italic);
    if (cacheIndex >= 0 && g_fontManager.cache[cacheIndex].isValid) {
        return g_fontManager.cache[cacheIndex].hFont;
    }
    
    // 创建新字体
    HFONT hFont = CreateFontInternal(fontName, size, bold, italic);
    
    if (hFont) {
        AddToCache(hFont, fontName, size, bold, italic);
    }
    
    return hFont;
}

/**
 * 智能字体创建：优先系统字体，回退到内置字体
 */
HFONT CreateSmartTimerFont(const wchar_t* preferredFontName, int size, BOOL bold, BOOL italic) {
    if (!g_fontsInitialized) {
        InitializeTimerFonts();
    }
    
    // 检查缓存
    int cacheIndex = FindCacheIndex(preferredFontName, size, bold, italic);
    if (cacheIndex >= 0 && g_fontManager.cache[cacheIndex].isValid) {
        return g_fontManager.cache[cacheIndex].hFont;
    }
    
    // 1. 首先尝试使用指定的字体
    if (IsFontInstalled(preferredFontName)) {
        HFONT hFont = CreateFontInternal(preferredFontName, size, bold, italic);
        if (hFont) {
            AddToCache(hFont, preferredFontName, size, bold, italic);
            return hFont;
        }
    }
    
    // 2. 如果指定字体不可用，尝试找到相似的字体
    const wchar_t* fallbackFonts[] = {
        L"Consolas", L"Cascadia Code", L"Cascadia Mono",
        L"Courier New", L"Arial", L"Segoe UI"
    };
    
    for (int i = 0; i < sizeof(fallbackFonts) / sizeof(fallbackFonts[0]); i++) {
        if (_wcsicmp(fallbackFonts[i], preferredFontName) == 0) {
            continue; // 跳过已经尝试过的字体
        }
        
        if (IsFontInstalled(fallbackFonts[i])) {
            HFONT hFont = CreateFontInternal(fallbackFonts[i], size, bold, italic);
            if (hFont) {
                // 使用原始字体名称缓存，这样下次查找时能找到
                AddToCache(hFont, preferredFontName, size, bold, italic);
                return hFont;
            }
        }
    }
    
    // 3. 最后回退到系统默认字体
    HFONT hFont = CreateFontInternal(L"Arial", size, bold, italic);
    
    if (hFont) {
        AddToCache(hFont, preferredFontName, size, bold, italic);
    }
    
    return hFont;
}

/**
 * 清理字体缓存
 */
void ClearFontCache(void) {
    for (int i = 0; i < 16; i++) {
        ClearCacheEntry(i);
    }
    g_fontManager.cacheCount = 0;
}

/**
 * 从缓存中移除指定字体
 */
void RemoveFromCache(const wchar_t* fontName) {
    if (!fontName) {
        return;
    }
    
    for (int i = 0; i < 16; i++) {
        if (g_fontManager.cache[i].isValid && 
            g_fontManager.cache[i].fontName &&
            _wcsicmp(g_fontManager.cache[i].fontName, fontName) == 0) {
            ClearCacheEntry(i);
        }
    }
}

/**
 * 获取文本大小
 */
SIZE GetTextSizeWithTimerFont(HDC hdc, const wchar_t* text, HFONT font) {
    SIZE size = {0, 0};
    
    if (!hdc || !text || !font) {
        return size;
    }
    
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &size);
    SelectObject(hdc, oldFont);
    
    return size;
}

/**
 * 根据字体大小获取字体高度
 */
int GetFontHeightForSize(int size) {
    HDC hdc = GetDC(NULL);
    if (!hdc) {
        return size;
    }
    
    int height = -MulDiv(size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);
    
    return height;
}

// 内部函数实现

/**
 * 创建字体（内部函数）
 */
static HFONT CreateFontInternal(const wchar_t* fontName, int size, BOOL bold, BOOL italic) {
    HDC hdc = GetDC(NULL);
    if (!hdc) {
        return NULL;
    }
    
    int height = -MulDiv(size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);
    
    // 根据透明背景模式选择字体质量
    // 需要引用全局状态来判断是否为透明背景模式
    extern TimerState g_timerState;
    DWORD fontQuality = g_timerState.transparentBackground ? NONANTIALIASED_QUALITY : CLEARTYPE_QUALITY;
    
    HFONT hFont = CreateFontW(
        height, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        italic, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        fontQuality,
        DEFAULT_PITCH | FF_DONTCARE,
        fontName
    );
    
    return hFont;
}

/**
 * 查找缓存索引
 */
static int FindCacheIndex(const wchar_t* fontName, int size, BOOL bold, BOOL italic) {
    for (int i = 0; i < 16; i++) {
        if (g_fontManager.cache[i].isValid &&
            g_fontManager.cache[i].fontName[0] != L'\0' &&
            _wcsicmp(g_fontManager.cache[i].fontName, fontName) == 0 &&
            g_fontManager.cache[i].size == size &&
            g_fontManager.cache[i].bold == bold &&
            g_fontManager.cache[i].italic == italic) {
            return i;
        }
    }
    return -1;
}

/**
 * 添加到缓存
 */
static void AddToCache(HFONT hFont, const wchar_t* fontName, int size, BOOL bold, BOOL italic) {
    if (!hFont || !fontName) {
        return;
    }
    
    // 查找空闲位置
    int index = -1;
    for (int i = 0; i < 16; i++) {
        if (!g_fontManager.cache[i].isValid) {
            index = i;
            break;
        }
    }
    
    // 如果没有空闲位置，替换最旧的
    if (index == -1) {
        index = 0;
        ClearCacheEntry(index);
    }
    
    // 复制字体名称到固定数组
    wcsncpy_s(g_fontManager.cache[index].fontName, 64, fontName, _TRUNCATE);
    
    g_fontManager.cache[index].hFont = hFont;
    g_fontManager.cache[index].size = size;
    g_fontManager.cache[index].bold = bold;
    g_fontManager.cache[index].italic = italic;
    g_fontManager.cache[index].isValid = TRUE;
    
    if (index >= g_fontManager.cacheCount) {
        g_fontManager.cacheCount = index + 1;
    }
}

/**
 * 清理缓存条目
 */
static void ClearCacheEntry(int index) {
    if (index >= 0 && index < 16 && g_fontManager.cache[index].isValid) {
        if (g_fontManager.cache[index].hFont) {
            DeleteObject(g_fontManager.cache[index].hFont);
        }
        
        g_fontManager.cache[index].hFont = NULL;
        g_fontManager.cache[index].fontName[0] = L'\0';
        g_fontManager.cache[index].size = 0;
        g_fontManager.cache[index].bold = FALSE;
        g_fontManager.cache[index].italic = FALSE;
        g_fontManager.cache[index].isValid = FALSE;
    }
}

// ==================== Catime字体集成函数 ====================

/**
 * 加载Catime字体
 */
BOOL LoadCatimeFont(const char* fontFileName) {
    if (!fontFileName) return FALSE;
    
    // 初始化字体管理器（如果尚未初始化）
    if (!InitializeFontManager()) {
        return FALSE;
    }
    
    FontLoadResult result;
    if (!LoadFontByFileName(fontFileName, &result)) {
        return FALSE;
    }
    
    return result.success;
}

/**
 * 加载推荐的计时器字体
 */
BOOL LoadRecommendedTimerFont(void) {
    // 初始化字体管理器
    if (!InitializeFontManager()) {
        return FALSE;
    }
    
    // 获取推荐字体列表
    CatimeFontInfo recommendedFonts[10];
    int count = GetRecommendedTimerFonts(recommendedFonts, 10);
    
    if (count == 0) {
        return FALSE;
    }
    
    // 尝试加载第一个推荐字体
    FontLoadResult result;
    if (LoadFontByFileName(recommendedFonts[0].fileName, &result)) {
        return result.success;
    }
    
    return FALSE;
}

/**
 * 获取Catime字体列表
 */
int GetCatimeFontList(CatimeFontInfo* fontList, int maxFonts) {
    if (!fontList || maxFonts <= 0) return 0;
    
    // 初始化字体管理器
    if (!InitializeFontManager()) {
        return 0;
    }
    
    return ListAvailableFonts(fontList, maxFonts);
}

/**
 * 检查是否已加载Catime字体
 */
BOOL IsCatimeFontLoaded(void) {
    return fontLoaded;
}