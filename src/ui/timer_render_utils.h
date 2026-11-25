#ifndef TIMER_RENDER_UTILS_H
#define TIMER_RENDER_UTILS_H

#include <windows.h>
#include <math.h>
#include "timer_types.h"     // UiTheme

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

// 绘制抗锯齿的空心描边圆角矩形（thickness=1 时线条落在矩形最外一圈像素上）
void DrawRoundedRectOutlineAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, int thickness, BYTE r, BYTE g, BYTE b, BYTE a);

// 绘制水平发丝线（thickness 可随 DPI 缩放，小数部分折算成相邻一行的透明度）
void DrawHairlineH(BYTE* pixels, int bufW, int x0, int x1, int y, float alpha, int thickness);

// 绘制 Win11 风格开关（rowHeight 为所在行高，用于垂直居中）
void DrawSwitchWin11(BYTE* pixels, int bufW, int bufH, int x, int y, int rowHeight, BOOL isOn);

// 按 DPI 缩放的 Win11 风格开关（开关自身尺寸随 dpi 变化，行高由调用方缩放）
void DrawSwitchWin11Scaled(BYTE* pixels, int bufW, int bufH, int x, int y, int rowHeight, BOOL isOn, UINT dpi);

// UI 字体族名：优先 Segoe UI Variable Text（Win11），否则 Segoe UI（Win10）。
// 不能直接请求 "Segoe UI Variable"——系统里没有这个族名，GDI 会静默回退成 Arial，
// 中文再经默认字体链接落到宋体点阵字。
const wchar_t* GetUiFontFamily(void);

// 图标字体族名：优先 Segoe Fluent Icons（Win11），否则 Segoe MDL2 Assets
const wchar_t* GetIconFontFamily(void);

// 按像素字号（em 高度）创建 UI 字体，emPx 已是物理像素
HFONT CreateUiFont(int emPx, int weight, BOOL semiboldDisplay);

// 让当前线程以"按监视器 DPI 感知 V2"的上下文创建窗口（Win10 1607+）。
// 返回进入前的上下文（失败返回 NULL），用 LeaveDpiContext 恢复。
HANDLE EnterPerMonitorDpiContext(void);
void LeaveDpiContext(HANDLE prev);

// 窗口所在显示器的 DPI（旧系统回退为系统 LOGPIXELSY）
UINT GetWindowDpi(HWND hwnd);

// 指定显示器的有效 DPI（需在按监视器 DPI 感知的上下文中调用才返回真实值，否则为 96）
UINT QueryMonitorDpi(HMONITOR monitor);

// 主窗口是 DPI 无感知的，它给出的 GetCursorPos / Shell_NotifyIconGetRect 结果都是"逻辑坐标"，
// 而按监视器 DPI 感知创建的菜单/面板窗口需要"物理坐标"。换算分两步、在两种上下文里各做一步：
// 1) 在主窗口（无感知）的上下文里调用 FindLogicalMonitor：找出逻辑点所在显示器，
//    并取回该显示器在逻辑坐标系中的矩形；
// 2) 进入按监视器 DPI 感知上下文后调用 MapLogicalPointToPhysical：用同一显示器在物理坐标系中的矩形
//    做线性映射。（系统的 LogicalToPhysicalPointForPerMonitorDPI 实测在无感知上下文里直接返回失败，故不使用）
HMONITOR FindLogicalMonitor(POINT logicalPt, RECT* logicalMonitorRect);
BOOL MapLogicalPointToPhysical(HMONITOR monitor, const RECT* logicalMonitorRect, POINT* pt);

// 把 96 DPI 基准尺寸换算到指定 DPI 的物理像素
int ScalePx(int px96, UINT dpi);

// 在不透明底上用 GDI 绘制单行文字（保留 ClearType，缺字经 GDI 字体链接补齐）
void DrawTextClearType(HDC hdc, HFONT font, const wchar_t* text, RECT rc, COLORREF color, UINT flags);

// 计算单行文字宽度（物理像素）
int MeasureTextWidth(HDC hdc, HFONT font, const wchar_t* text);

// GDI 往 32bpp DIB 上写文字时会把 alpha 清成 0：
// 绘制文字前用 SnapshotAlpha 留底（outAlpha 需由调用方分配 pixelCount 字节），
// 绘制后用 RestoreAlpha 补回。
void SnapshotAlpha(const BYTE* pixels, BYTE* outAlpha, size_t pixelCount);
void RestoreAlpha(BYTE* pixels, const BYTE* alpha, size_t pixelCount);

// ===========================================
// 玻璃风格面板底图
// 面板是逐像素透明的分层窗口，系统不会替它合成"身后的模糊画面"，所以这里不去假装磨砂：
// "玻璃感"只来自底色、细描边、顶部高光和柔和投影。底色可以带一点真透明度（glassAlpha），
// 由系统实时合成，和计时器主体的整窗透明度是同一个原理——不截图、不会过期、拖动天然跟手。
// ===========================================

// 预渲染的面板底图（预乘 ARGB：投影 + 底色 + 描边 + 顶部高光）。
// 与窗口位置无关：每个弹窗生成一次，之后每次重绘只需整体拷贝。
typedef struct {
    int w, h;
    BYTE* pixels;
} GlassCache;

void GlassCache_Free(GlassCache* gc);

// 生成底图：缓冲大小 bufW x bufH，面板位于缓冲内 (px,py,pw,ph)。
BOOL GlassCache_Build(GlassCache* gc, int bufW, int bufH, int px, int py, int pw, int ph, int radius,
                      int shadowSize, const UiTheme* theme);

// 把底图整体拷贝到同尺寸的目标缓冲
void GlassCache_Blit(const GlassCache* gc, BYTE* dst, int bufW, int bufH);

// ===========================================
// 辉光
// ===========================================

// 圆角矩形外侧的辉光。strength 为 0..1，spread 为扩散距离（像素）。
void GlowRoundedRect(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h,
                     BYTE r, BYTE g, BYTE b, float strength, int spread);

// 文字辉光：只叠加光晕，不绘制文字本身（文字由调用方随后用 DrawTextClearType 绘制）。
// additive 为 TRUE 时加色发光（暗色主题），FALSE 时按光晕强度把底色拉向 color（亮色主题）。
void DrawTextGlow(BYTE* pixels, int bufW, int bufH, HDC hdc, HFONT font, const wchar_t* text, RECT rc, UINT flags,
                  COLORREF color, float strength, int radius, BOOL additive);

// ===========================================
// 其它
// ===========================================

// 主题化开关（胶囊形，开启时带辉光）；rowHeight 为所在行高，用于垂直居中
void DrawSwitchThemed(BYTE* pixels, int bufW, int bufH, int x, int y, int rowHeight, BOOL isOn, UINT dpi, const UiTheme* theme);

// 指定颜色的水平线（thickness 可随 DPI 缩放）
void DrawHLine(BYTE* pixels, int bufW, int x0, int x1, int y, BYTE r, BYTE g, BYTE b, BYTE a, int thickness);

#ifdef __cplusplus
}
#endif

#endif // TIMER_RENDER_UTILS_H
