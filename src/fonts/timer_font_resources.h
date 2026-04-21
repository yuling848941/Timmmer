#ifndef TIMER_FONT_RESOURCES_H
#define TIMER_FONT_RESOURCES_H

#include <windows.h>
#include "timer_types.h"
#include "timer_embedded_fonts.h"  // 包含嵌入式字体资源定义

// 字体资源类型
typedef enum {
    FONT_RESOURCE_SYSTEM,    // 系统字体
    FONT_RESOURCE_EMBEDDED,  // 内置字体资源
    FONT_RESOURCE_FILE       // 外部字体文件
} FontResourceType;

// 兼容旧版本 - 嵌入式字体数据结构
typedef struct {
    const char* name;           // 字体名称
    const unsigned char* data;  // 字体数据
    size_t size;               // 数据大小
    const wchar_t* displayName; // 显示名称
} EmbeddedFontData;

// EmbeddedFontResource 现在在 timer_embedded_fonts.h 中定义

// 扩展的字体信息结构
typedef struct {
    const wchar_t* name;
    const wchar_t* displayName;
    FontResourceType type;  // 修正字段名
    union {
        struct {
            BOOL isInstalled;
        } system;
        struct {
            const EmbeddedFontResource* fontData;
            HANDLE hFontResource;
        } embedded;
        struct {
            const wchar_t* filePath;
        } file;
    } resource;
    BOOL isAvailable;
} ExtendedFontInfo;

// 字体资源管理器
typedef struct {
    ExtendedFontInfo* mixedFonts;  // 修正字段名
    int mixedFontCount;            // 修正字段名
    int capacity;
    BOOL initialized;
} FontResourceManager;

// 内置字体列表（精简版，只包含数字和符号）
extern const EmbeddedFontData g_embeddedFonts[];
extern const int g_embeddedFontCount;

// 新的嵌入式字体资源（来自timer_embedded_fonts.h）
extern const EmbeddedFontResource g_embeddedFontResources[];
extern const int g_embeddedFontResourceCount;

// 字体资源管理函数
BOOL InitializeFontResources(void);
BOOL InitializeFontResourceSystem(void);  // 添加缺失的函数声明
FontResourceManager* GetFontResourceManager(void);  // 添加缺失的函数声明
void CleanupFontResources(void);

// 字体资源查询
const ExtendedFontInfo* GetFontResource(const wchar_t* fontName);
int GetAvailableFontResourceCount(void);
const ExtendedFontInfo* GetFontResourceByIndex(int index);

// 字体资源加载
HFONT LoadFontFromResource(const ExtendedFontInfo* fontInfo, int size, BOOL bold, BOOL italic);
HANDLE RegisterEmbeddedFont(const EmbeddedFontResource* fontResource);
void UnregisterEmbeddedFont(const EmbeddedFontResource* fontResource);

// 字体可用性检测
BOOL CheckSystemFontAvailability(const wchar_t* fontName);
BOOL CheckEmbeddedFontAvailability(const EmbeddedFontResource* fontResource);

#endif // TIMER_FONT_RESOURCES_H