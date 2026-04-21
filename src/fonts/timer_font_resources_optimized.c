#include "timer_font_resources.h"
#include "timer_fonts.h"
#include "timer_embedded_fonts.h"
#include "timer_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 全局字体资源管理器
static FontResourceManager g_fontResourceManager = {0};

// 混合字体配置：系统字体 + 嵌入式字体
static ExtendedFontInfo g_mixedFonts[] = {
    // 仅使用内置开源字体（避免版权问题）
    // MIT 许可证字体
    {L"ProFontWindows Essence", L"ProFontWindows Essence", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[0], FALSE},  // MIT
    
    // OFL 许可证字体
    {L"Jacquard 12", L"Jacquard 12", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[1], FALSE},  // OFL
    {L"Jacquarda Bastarda 9", L"Jacquarda Bastarda 9", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[2], FALSE},  // OFL
    {L"Pixelify Sans", L"Pixelify Sans", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[3], FALSE},  // OFL
    {L"Rubik Burned", L"Rubik Burned", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[4], FALSE},  // OFL
    {L"Rubik Glitch", L"Rubik Glitch", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[5], FALSE},  // OFL
    {L"Rubik Marker Hatch", L"Rubik Marker Hatch", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[6], FALSE},  // OFL
    {L"Rubik Puddles", L"Rubik Puddles", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[7], FALSE},  // OFL
    {L"Wallpoet", L"Wallpoet", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[8], FALSE},  // OFL
    
    // SIL 许可证字体
    {L"Rec Mono Casual", L"Rec Mono Casual", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[9], FALSE},  // SIL
    {L"Terminess Nerd Font Mono", L"Terminess Nerd Font Mono", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[10], FALSE},  // SIL
    {L"DaddyTimeMono", L"DaddyTimeMono", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[11], FALSE},  // SIL
    {L"Departure Mono", L"Departure Mono", FONT_RESOURCE_EMBEDDED, .resource.embedded.fontData = &g_embeddedFontResources[12], FALSE}  // SIL
};

/**
 * 快速注册内置字体（无调试输出）
 */
static HANDLE RegisterEmbeddedFontFast(const EmbeddedFontResource* fontResource) {
    if (!fontResource || !fontResource->data || fontResource->size == 0) {
        return NULL;
    }
    
    // 直接调用系统API，无任何调试输出
    DWORD numFonts = 0;
    return AddFontMemResourceEx(
        (PVOID)fontResource->data,
        fontResource->size,
        NULL,
        &numFonts
    );
}

/**
 * 初始化字体资源系统（优化版本）
 */
BOOL InitializeFontResources(void) {
    if (g_fontResourceManager.initialized) {
        return TRUE;
    }
    
    // 设置混合字体配置
    g_fontResourceManager.mixedFonts = g_mixedFonts;
    g_fontResourceManager.mixedFontCount = sizeof(g_mixedFonts) / sizeof(g_mixedFonts[0]);
    g_fontResourceManager.capacity = g_fontResourceManager.mixedFontCount;
    g_fontResourceManager.initialized = TRUE;
    
    // 快速注册所有嵌入式字体
    for (int i = 0; i < g_fontResourceManager.mixedFontCount; i++) {
        ExtendedFontInfo* fontInfo = &g_fontResourceManager.mixedFonts[i];
        if (fontInfo->type == FONT_RESOURCE_EMBEDDED) {
            // 快速注册，无调试输出
            HANDLE hFontResource = RegisterEmbeddedFontFast(fontInfo->resource.embedded.fontData);
            fontInfo->resource.embedded.hFontResource = hFontResource;
            fontInfo->isAvailable = (hFontResource != NULL);
        } else if (fontInfo->type == FONT_RESOURCE_SYSTEM) {
            fontInfo->resource.system.isInstalled = CheckSystemFontAvailability(fontInfo->name);
            fontInfo->isAvailable = fontInfo->resource.system.isInstalled;
        } else if (fontInfo->type == FONT_RESOURCE_FILE) {
            if (fontInfo->resource.file.filePath) {
                DWORD fileAttributes = GetFileAttributesW(fontInfo->resource.file.filePath);
                fontInfo->isAvailable = (fileAttributes != INVALID_FILE_ATTRIBUTES && 
                                       !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY));
            } else {
                fontInfo->isAvailable = FALSE;
            }
        }
    }
    
    // 异步发送WM_FONTCHANGE消息，避免阻塞启动
    PostMessage(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
    
    return TRUE;
}

/**
 * 初始化字体资源系统（别名函数）
 */
BOOL InitializeFontResourceSystem(void) {
    return InitializeFontResources();
}

/**
 * 获取字体资源管理器
 */
FontResourceManager* GetFontResourceManager(void) {
    if (!g_fontResourceManager.initialized) {
        InitializeFontResources();
    }
    return &g_fontResourceManager;
}

/**
 * 获取混合字体数量
 */
int GetMixedFontCount(void) {
    FontResourceManager* manager = GetFontResourceManager();
    return manager ? manager->mixedFontCount : 0;
}

/**
 * 获取混合字体信息
 */
ExtendedFontInfo* GetMixedFontInfo(int index) {
    FontResourceManager* manager = GetFontResourceManager();
    if (!manager || index < 0 || index >= manager->mixedFontCount) {
        return NULL;
    }
    return &manager->mixedFonts[index];
}

/**
 * 根据名称查找字体
 */
ExtendedFontInfo* FindFontByName(const wchar_t* fontName) {
    FontResourceManager* manager = GetFontResourceManager();
    if (!manager || !fontName) {
        return NULL;
    }
    
    for (int i = 0; i < manager->mixedFontCount; i++) {
        if (wcscmp(manager->mixedFonts[i].name, fontName) == 0) {
            return &manager->mixedFonts[i];
        }
    }
    
    return NULL;
}

/**
 * 根据显示名称查找字体
 */
ExtendedFontInfo* FindFontByDisplayName(const wchar_t* displayName) {
    FontResourceManager* manager = GetFontResourceManager();
    if (!manager || !displayName) {
        return NULL;
    }
    
    for (int i = 0; i < manager->mixedFontCount; i++) {
        if (wcscmp(manager->mixedFonts[i].displayName, displayName) == 0) {
            return &manager->mixedFonts[i];
        }
    }
    
    return NULL;
}

/**
 * 获取可用字体列表
 */
int GetAvailableFonts(ExtendedFontInfo** fontList, int maxCount) {
    FontResourceManager* manager = GetFontResourceManager();
    if (!manager || !fontList || maxCount <= 0) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < manager->mixedFontCount && count < maxCount; i++) {
        if (manager->mixedFonts[i].isAvailable) {
            fontList[count++] = &manager->mixedFonts[i];
        }
    }
    
    return count;
}

/**
 * 创建字体句柄
 */
HFONT CreateFontFromInfo(const ExtendedFontInfo* fontInfo, int size, BOOL bold, BOOL italic) {
    if (!fontInfo || !fontInfo->isAvailable) {
        return NULL;
    }
    
    // 根据透明背景模式选择字体质量
    extern TimerState g_timerState;
    DWORD fontQuality = g_timerState.transparentBackground ? NONANTIALIASED_QUALITY : CLEARTYPE_QUALITY;
    
    return CreateFontW(
        size,                        // nHeight
        0,                          // nWidth
        0,                          // nEscapement
        0,                          // nOrientation
        bold ? FW_BOLD : FW_NORMAL, // nWeight
        italic,                     // bItalic
        FALSE,                      // bUnderline
        FALSE,                      // bStrikeOut
        DEFAULT_CHARSET,            // nCharSet
        OUT_DEFAULT_PRECIS,         // nOutPrecision
        CLIP_DEFAULT_PRECIS,        // nClipPrecision
        fontQuality,                // nQuality (根据透明背景模式动态选择)
        DEFAULT_PITCH | FF_DONTCARE, // nPitchAndFamily
        fontInfo->name              // lpszFacename
    );
}

/**
 * 获取字体文件路径
 */
const wchar_t* GetFontFilePath(const ExtendedFontInfo* fontInfo) {
    if (!fontInfo || fontInfo->type != FONT_RESOURCE_FILE) {
        return NULL;
    }
    
    return fontInfo->resource.file.filePath;
}

/**
 * 检查系统字体可用性
 */
BOOL CheckSystemFontAvailability(const wchar_t* fontName) {
    return IsFontInstalled(fontName);
}

/**
 * 检查内置字体可用性
 */
BOOL CheckEmbeddedFontAvailability(const EmbeddedFontResource* fontResource) {
    return (fontResource && fontResource->data && fontResource->size > 0);
}

/**
 * 清理字体资源
 */
void CleanupFontResources(void) {
    if (!g_fontResourceManager.initialized) {
        return;
    }
    
    // 注销所有已注册的嵌入式字体
    for (int i = 0; i < g_fontResourceManager.mixedFontCount; i++) {
        ExtendedFontInfo* fontInfo = &g_fontResourceManager.mixedFonts[i];
        if (fontInfo->type == FONT_RESOURCE_EMBEDDED && 
            fontInfo->resource.embedded.hFontResource) {
            RemoveFontMemResourceEx(fontInfo->resource.embedded.hFontResource);
            fontInfo->resource.embedded.hFontResource = NULL;
        }
    }
    
    // 重置管理器状态
    memset(&g_fontResourceManager, 0, sizeof(g_fontResourceManager));
}

/**
 * 注册内置字体（保持兼容性）
 */
HANDLE RegisterEmbeddedFont(const EmbeddedFontResource* fontResource) {
    return RegisterEmbeddedFontFast(fontResource);
}

/**
 * 注销内置字体
 */
void UnregisterEmbeddedFont(const EmbeddedFontResource* fontResource) {
    // 实际实现时需要调用RemoveFontMemResourceEx
    // 这里只是示例
}