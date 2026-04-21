#ifndef TIMER_EMBEDDED_FONTS_H
#define TIMER_EMBEDDED_FONTS_H

#include <stddef.h>

// 嵌入式字体数据结构
typedef struct {
    const char* name;           // 内部名称
    const unsigned char* data;  // 字体数据
    size_t size;               // 数据大小
    const wchar_t* displayName; // 显示名称
    const char* license;        // 许可证类型
} EmbeddedFontResource;

// 字体数据声明
extern const unsigned char profontwindows_essence_data[];
extern const size_t profontwindows_essence_size;
extern const unsigned char jacquard_12_essence_data[];
extern const size_t jacquard_12_essence_size;
extern const unsigned char jacquarda_bastarda_9_essence_data[];
extern const size_t jacquarda_bastarda_9_essence_size;
extern const unsigned char pixelify_sans_essence_data[];
extern const size_t pixelify_sans_essence_size;
extern const unsigned char rubik_burned_essence_data[];
extern const size_t rubik_burned_essence_size;
extern const unsigned char rubik_glitch_essence_data[];
extern const size_t rubik_glitch_essence_size;
extern const unsigned char rubik_marker_hatch_essence_data[];
extern const size_t rubik_marker_hatch_essence_size;
extern const unsigned char rubik_puddles_essence_data[];
extern const size_t rubik_puddles_essence_size;
extern const unsigned char wallpoet_essence_data[];
extern const size_t wallpoet_essence_size;
extern const unsigned char rec_mono_casual_essence_data[];
extern const size_t rec_mono_casual_essence_size;
extern const unsigned char terminess_nerd_font_essence_data[];
extern const size_t terminess_nerd_font_essence_size;
extern const unsigned char daddytimemono_essence_data[];
extern const size_t daddytimemono_essence_size;
extern const unsigned char departuremono_essence_data[];
extern const size_t departuremono_essence_size;

// 字体列表
extern const EmbeddedFontResource g_embeddedFontResources[];
extern const int g_embeddedFontResourceCount;

#endif // TIMER_EMBEDDED_FONTS_H
