#ifndef TIMER_RENDER_UTILS_H
#define TIMER_RENDER_UTILS_H

#include <windows.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// 像素颜色混合
void BlendPixel(BYTE* pixels, int idx, BYTE r, BYTE g, BYTE b, float alpha);

// 计算点到圆角矩形的符号距离 (SDF)
float RoundedRectSDF(float px, float py, float cx, float cy, float hw, float hh, float r);

// 绘制真软阴影 (基于 SDF)
void DrawSoftShadowSDF(BYTE* pixels, int bufW, int bufH, int mx, int my, int mw, int mh, int r, int shadowSize, int offsetY, float maxOpacity);

// 绘制抗锯齿的纯色圆角矩形
void FillRoundedRectAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, BYTE r, BYTE g, BYTE b, BYTE a);

// 绘制带斜纹理的抗锯齿圆角矩形 (用于 Slider 轨道)
void FillStripedRoundedRectAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, BYTE r, BYTE g, BYTE b, BYTE a);

// 绘制抗锯齿的空心描边圆角矩形
void DrawRoundedRectOutlineAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, int thickness, BYTE r, BYTE g, BYTE b, BYTE a);

#ifdef __cplusplus
}
#endif

#endif // TIMER_RENDER_UTILS_H
