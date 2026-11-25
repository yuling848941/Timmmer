#ifndef TIMER_DIALOG_GLASS_H
#define TIMER_DIALOG_GLASS_H

#include <windows.h>
#include "timer_render_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

// 自绘设置面板共用的"玻璃底"支持。
//
// 做法：面板创建后、显示前，把它周围一大块屏幕拍成快照；之后按窗口当前位置
// 从快照里取对应区域做模糊，生成玻璃底图。这样面板被拖动时不需要重新抓屏——
// 抓屏必须在窗口不可见时进行，否则会把面板自己拍进去。
typedef struct {
    Backdrop backdrop;     // 窗口初始位置周围一块屏幕的快照
    GlassCache cache;      // 与窗口当前位置对应的玻璃底图
    int bufW, bufH;        // 面板窗口的客户区尺寸（含阴影）
    int shadow, radius;    // 阴影留白与圆角（像素）
} DialogGlass;

// 窗口创建后、显示前调用一次。失败时后续绘制自动退化为不透明底色。
void DialogGlass_Init(DialogGlass* g, HWND hwnd, int bufW, int bufH, int shadow, int radius);
void DialogGlass_Destroy(DialogGlass* g);

// 按窗口当前位置从快照取区域，重建玻璃底图。
// 在窗口位置变化后（拖动结束）和主题切换后调用。
void DialogGlass_Rebuild(DialogGlass* g, HWND hwnd);

// 每次渲染开头调用：把底图拷贝到面板的 DIB。
void DialogGlass_Blit(DialogGlass* g, BYTE* pixels, int bufW, int bufH);

#ifdef __cplusplus
}
#endif

#endif // TIMER_DIALOG_GLASS_H
