#ifndef TIMER_FONTS_H
#define TIMER_FONTS_H

#include <windows.h>
#include "timer_font_manager.h"

// 字体类型枚举
typedef enum {
    FONT_TYPE_DEFAULT,
    FONT_TYPE_MONOSPACE,
    FONT_TYPE_DISPLAY,
    FONT_TYPE_SANS_SERIF,
    FONT_TYPE_SERIF,
    FONT_TYPE_COUNT
} FontType;

// 字体信息结构（简化版）
typedef struct {
    const wchar_t* name;
    const wchar_t* displayName;
    FontType type;
    BOOL isSystemFont;
} FontInfo;

// 字体缓存结构
typedef struct {
    HFONT hFont;
    wchar_t fontName[64];  // 使用固定大小数组避免内存管理问题
    int size;
    BOOL bold;
    BOOL italic;
    BOOL isValid;
} FontCache;

// 字体管理器（简化版）
typedef struct {
    FontCache cache[16];  // 最多缓存16个字体
    int cacheCount;
} FontManager;

// Catime项目字体列表（基于MIT、SIL OFL、OFL许可证）
extern const FontInfo g_catimeFonts[];
extern const int g_catimeFontCount;

// 字体管理函数
BOOL InitializeTimerFonts(void);
void CleanupTimerFonts(void);

// Catime字体集成函数
BOOL LoadCatimeFont(const char* fontFileName);
BOOL LoadRecommendedTimerFont(void);
int GetCatimeFontList(CatimeFontInfo* fontList, int maxFonts);
BOOL IsCatimeFontLoaded(void);

// 字体创建函数（懒加载）
HFONT CreateTimerFont(const wchar_t* fontName, int size, BOOL bold, BOOL italic);
HFONT CreateSmartTimerFont(const wchar_t* preferredFontName, int size, BOOL bold, BOOL italic);
HFONT GetTimerFont(const wchar_t* fontName, int size, BOOL bold, BOOL italic);
HFONT GetDefaultTimerFont(int size);
HFONT GetMonospaceFont(int size);

// 字体查询函数
const FontInfo* FindFontInfo(const wchar_t* fontName);
const FontInfo* GetFontInfoByIndex(int index);
int GetAvailableFontCount(void);

// 字体验证函数
BOOL IsFontInstalled(const wchar_t* fontName);
const wchar_t* GetFallbackFont(FontType type);

// 字体缓存管理
void ClearFontCache(void);
void RemoveFromCache(const wchar_t* fontName);

// 字体工具函数
SIZE GetTextSizeWithTimerFont(HDC hdc, const wchar_t* text, HFONT font);
int GetFontHeightForSize(int size);

// 混合字体列表函数（系统字体 + 内置字体）
int GetMixedFontList(wchar_t fontNames[][256], int maxFonts);

#endif // TIMER_FONTS_H