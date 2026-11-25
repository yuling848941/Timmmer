#ifndef TIMER_DIALOG_GLASS_H
#define TIMER_DIALOG_GLASS_H

#include <windows.h>
#include "timer_render_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

// 自绘设置面板共用的"玻璃底"支持。
//
// 面板底图与窗口位置无关：创建时生成一次，主题切换时按新配色重新生成，拖动不需要任何处理。
typedef struct {
    GlassCache cache;      // 面板底图
    int bufW, bufH;        // 面板窗口的客户区尺寸（含阴影）
    int shadow, radius;    // 阴影留白与圆角（像素）
} DialogGlass;

// 窗口创建时调用一次。
void DialogGlass_Init(DialogGlass* g, int bufW, int bufH, int shadow, int radius);
void DialogGlass_Destroy(DialogGlass* g);

// 主题切换后调用：按新配色重新生成底图。
void DialogGlass_Rebuild(DialogGlass* g);

// 每次渲染开头调用：把底图拷贝到面板的 DIB。
void DialogGlass_Blit(DialogGlass* g, BYTE* pixels, int bufW, int bufH);

#ifdef __cplusplus
}
#endif

#endif // TIMER_DIALOG_GLASS_H
