/*
 * iOS Style Context Menu for Win32
 * 完整实现：抗锯齿圆角、阴影、半透明背景、MDL2 图标、Switch 控件
 */

#include "ios_menu.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <stdio.h>
#include <math.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "msimg32.lib")

// ===========================================
// iOS 风格设计常量（旗舰版）
// ===========================================

#define MENU_WIDTH              230     
#define MENU_ITEM_HEIGHT        46      
#define MENU_RADIUS             14      
#define MENU_SHADOW_SIZE        40      

#define MENU_PADDING_TOP        10       
#define MENU_PADDING_BOTTOM     10       
#define MENU_PADDING_LEFT       14      
#define MENU_PADDING_RIGHT      14      

#define MENU_ICON_SIZE          18      
#define MENU_ICON_BOX_W         28      
#define MENU_TEXT_OFFSET        50      

#define SWITCH_WIDTH            38      
#define SWITCH_HEIGHT           20      

#define COLOR_TEXT_PRIMARY      RGB(32, 32, 32)
#define COLOR_ACCENT            RGB(0, 120, 212) 

// ===========================================
// 全局变量与数据结构
// ===========================================

static const wchar_t* g_menuClassName = L"IosMenuClass";
static BOOL g_classRegistered = FALSE;
static HWND g_currentMenuHwnd = NULL;

typedef struct {
    const IosMenuItem* items;
    int itemCount;
    int menuHeight;       
    int hoverIndex;
    int prevHoverIndex;  
    HWND owner;
    HDC hdcBuffer;
    HBITMAP hbmBuffer;
    void* pBits;          
    int bufWidth;
    int bufHeight;
    BYTE* cachedFull;      
    BOOL cacheReady;       
    HFONT hFontLabel;
    HFONT hFontIcon;
} MenuData;

// ===========================================
// 绘图引擎（抗锯齿）
// ===========================================

static inline void BlendPixel(BYTE* pixels, int idx, BYTE r, BYTE g, BYTE b, float alpha) {
    if (alpha <= 0.01f) return;
    if (alpha >= 0.99f) {
        pixels[idx] = b; pixels[idx+1] = g; pixels[idx+2] = r; pixels[idx+3] = 255;
        return;
    }
    BYTE a = (BYTE)(alpha * 255);
    pixels[idx] = (pixels[idx] * (255 - a) + b * a) >> 8;
    pixels[idx+1] = (pixels[idx+1] * (255 - a) + g * a) >> 8;
    pixels[idx+2] = (pixels[idx+2] * (255 - a) + r * a) >> 8;
    pixels[idx+3] = a > pixels[idx+3] ? a : pixels[idx+3];
}

static void FillRoundedRectAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, BYTE r, BYTE g, BYTE b, BYTE a) {
    int x1 = x + w, y1 = y + h;
    for (int py = y; py < y1; py++) {
        if (py < 0 || py >= bufH) continue;
        float dy = (py < y + ry) ? (float)(y + ry - py) : (py >= y1 - ry ? (float)(py - (y1 - ry) + 1) : 0);
        for (int px = x; px < x1; px++) {
            if (px < 0 || px >= bufW) continue;
            float dx = (px < x + rx) ? (float)(x + rx - px) : (px >= x1 - rx ? (float)(px - (x1 - rx) + 1) : 0);
            float alpha = 1.0f;
            if (dx > 0 && dy > 0) {
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > (float)rx) {
                    float diff = dist - (float)rx;
                    if (diff >= 1.0f) continue;
                    alpha = 1.0f - diff;
                }
            }
            int idx = (py * bufW + px) * 4;
            BlendPixel(pixels, idx, r, g, b, alpha * (a / 255.0f));
        }
    }
}

static void DrawSoftShadow(BYTE* pixels, int bufW, int bufH, int menuH) {
    int menuW = MENU_WIDTH; int shadowSize = 30;
    for (int i = 0; i < shadowSize; i++) {
        float alpha = 0.12f * (1.0f - (float)i / shadowSize);
        for (int x = 8; x < menuW + i; x++) {
            int y = menuH + i; if (x >= bufW || y >= bufH) continue;
            BlendPixel(pixels, (y * bufW + x) * 4, 0, 0, 0, alpha);
        }
        for (int y = 8; y < menuH + i; y++) {
            int x = menuW + i; if (x >= bufW || y >= bufH) continue;
            BlendPixel(pixels, (y * bufW + x) * 4, 0, 0, 0, alpha);
        }
    }
}

static void DrawSwitchWin11(BYTE* pixels, int bufW, int bufH, int x, int y, BOOL isOn) {
    int sw = SWITCH_WIDTH, sh = SWITCH_HEIGHT, ts = 12;
    int cy = y + (MENU_ITEM_HEIGHT - sh) / 2;
    if (isOn) {
        FillRoundedRectAA(pixels, bufW, bufH, 10, 10, x, cy, sw, sh, 0, 120, 212, 255);
        FillRoundedRectAA(pixels, bufW, bufH, 6, 6, x + sw - ts - 4, cy + 4, ts, ts, 255, 255, 255, 255);
    } else {
        FillRoundedRectAA(pixels, bufW, bufH, 10, 10, x, cy, sw, sh, 180, 180, 180, 255);
        FillRoundedRectAA(pixels, bufW, bufH, 9, 9, x + 1, cy + 1, sw - 2, sh - 2, 255, 255, 255, 255);
        FillRoundedRectAA(pixels, bufW, bufH, 6, 6, x + 5, cy + 4, ts, ts, 120, 120, 120, 255);
    }
}

// ===========================================
// 菜单项内容绘制
// ===========================================

static void DrawItemContent(MenuData* data, int idx, int y) {
    const IosMenuItem* it = &data->items[idx];
    HDC hdc = data->hdcBuffer;
    int ix = MENU_PADDING_LEFT;
    int tx = MENU_TEXT_OFFSET;

    if (it->iconChar && wcslen(it->iconChar) == 1) {
        SelectObject(hdc, data->hFontIcon); SetTextColor(hdc, RGB(80, 80, 80)); SetBkMode(hdc, TRANSPARENT);
        RECT rc = {ix, y, ix + MENU_ICON_BOX_W, y + MENU_ITEM_HEIGHT};
        DrawTextW(hdc, it->iconChar, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    SelectObject(hdc, data->hFontLabel); SetTextColor(hdc, COLOR_TEXT_PRIMARY);
    RECT rcText = {tx, y, MENU_WIDTH - MENU_PADDING_RIGHT, y + MENU_ITEM_HEIGHT};
    DrawTextW(hdc, it->label, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Alpha 修复
    BYTE* p = (BYTE*)data->pBits;
    for (int i = y; i < y + MENU_ITEM_HEIGHT; i++) {
        if (i >= data->bufHeight) break;
        BYTE* row = p + (i * data->bufWidth * 4);
        for (int j = 0; j < MENU_WIDTH; j++) {
            BYTE* px = row + (j * 4);
            if (px[0] < 240 || px[1] < 240 || px[2] < 240) px[3] = 255;
        }
    }
}

// ===========================================
// 渲染主逻辑
// ===========================================

static void RenderMenuContent(MenuData* data) {
    BYTE* pixels = (BYTE*)data->pBits;
    int w = data->bufWidth, h = data->bufHeight, mh = data->menuHeight;
    int sz = w * h * 4;

    memset(pixels, 0, sz);
    DrawSoftShadow(pixels, w, h, mh);
    FillRoundedRectAA(pixels, w, h, MENU_RADIUS, MENU_RADIUS, 0, 0, MENU_WIDTH, mh, 255, 255, 255, 252);

    int y = MENU_PADDING_TOP;
    for (int i = 0; i < data->itemCount; i++) {
        const IosMenuItem* it = &data->items[i];
        if (i == data->hoverIndex) {
            FillRoundedRectAA(pixels, w, h, 10, 10, 6, y + 2, MENU_WIDTH - 12, MENU_ITEM_HEIGHT - 4, 240, 240, 250, 255);
        }
        if (it->type == IOS_MENU_ITEM_SWITCH) {
            DrawSwitchWin11(pixels, w, h, MENU_WIDTH - 45 - MENU_PADDING_RIGHT, y, it->isSwitchOn);
        }
        if (it->hasDividerAfter && i < data->itemCount - 1) {
            int ly = y + MENU_ITEM_HEIGHT;
            for (int x = MENU_TEXT_OFFSET; x < MENU_WIDTH - MENU_PADDING_RIGHT; x++) BlendPixel(pixels, (ly * w + x) * 4, 235, 235, 235, 0.8f);
        }
        DrawItemContent(data, i, y);
        y += MENU_ITEM_HEIGHT;
    }
}

static void UpdateMenuDisplay(HWND hwnd, MenuData* data) {
    RenderMenuContent(data);
    HDC hdc = GetDC(NULL); POINT dst = {0,0}, src = {0,0}; SIZE sz = {data->bufWidth, data->bufHeight}; BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT rc; GetWindowRect(hwnd, &rc); dst.x = rc.left; dst.y = rc.top;
    UpdateLayeredWindow(hwnd, hdc, &dst, &sz, data->hdcBuffer, &src, 0, &bf, ULW_ALPHA); ReleaseDC(NULL, hdc);
}

static LRESULT CALLBACK MenuProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MenuData* d = (MenuData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp; d = (MenuData*)calloc(1, sizeof(MenuData)); SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)d);
            IosMenuCreateParams* p = (IosMenuCreateParams*)cs->lpCreateParams; d->items = p->items; d->itemCount = p->itemCount; d->owner = p->owner; d->menuHeight = p->menuHeight;
            RECT rc; GetClientRect(hwnd, &rc); d->bufWidth = rc.right; d->bufHeight = rc.bottom;
            HDC hdc = GetDC(NULL); d->hdcBuffer = CreateCompatibleDC(hdc);
            BITMAPINFOHEADER bi = {sizeof(bi), d->bufWidth, -d->bufHeight, 1, 32, BI_RGB};
            d->hbmBuffer = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &d->pBits, NULL, 0);
            SelectObject(d->hdcBuffer, d->hbmBuffer);
            d->hFontLabel = CreateFontW(18, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
            d->hFontIcon = CreateFontW(MENU_ICON_SIZE, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");
            ReleaseDC(NULL, hdc); UpdateMenuDisplay(hwnd, d); SetTimer(hwnd, 1, 30, NULL); return 0;
        }
        case WM_TIMER: {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt); int h = -1;
            if (pt.x >= 0 && pt.x < MENU_WIDTH && pt.y >= MENU_PADDING_TOP && pt.y < d->menuHeight - MENU_PADDING_BOTTOM) {
                h = (pt.y - MENU_PADDING_TOP) / MENU_ITEM_HEIGHT; if (h >= d->itemCount) h = -1;
            }
            if (h != d->hoverIndex) { d->hoverIndex = h; UpdateMenuDisplay(hwnd, d); } return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            // 检查点击是否在有效菜单区域内
            if (pt.x >= 0 && pt.x < MENU_WIDTH && 
                pt.y >= MENU_PADDING_TOP && pt.y < d->menuHeight - MENU_PADDING_BOTTOM) 
            {
                int i = (pt.y - MENU_PADDING_TOP) / MENU_ITEM_HEIGHT;
                if (i >= 0 && i < d->itemCount) {
                    const IosMenuItem* it = &d->items[i];
                    if (it->commandId != 0) SendMessage(d->owner, WM_COMMAND, MAKEWPARAM(it->commandId, 0), 0);
                }
            }
            // 无论点击哪里，都销毁菜单（如果在外面就是关闭，在里面就是执行后关闭）
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_CAPTURECHANGED: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: if (g_currentMenuHwnd == hwnd) DestroyWindow(hwnd); return 0;
        case WM_DESTROY: {
            if (d) { KillTimer(hwnd, 1); DeleteObject(d->hbmBuffer); DeleteDC(d->hdcBuffer); if (d->cachedFull) free(d->cachedFull); DeleteObject(d->hFontLabel); DeleteObject(d->hFontIcon); free(d); }
            g_currentMenuHwnd = NULL; return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

BOOL IosMenu_Initialize(HINSTANCE h) {
    if (g_classRegistered) return TRUE;
    WNDCLASSEXW wc = {sizeof(WNDCLASSEXW), CS_DBLCLKS, MenuProc, 0, 0, h, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, g_menuClassName, NULL};
    if (!RegisterClassExW(&wc)) return FALSE;
    g_classRegistered = TRUE; return TRUE;
}

void IosMenu_Shutdown(void) { if (g_classRegistered) UnregisterClassW(g_menuClassName, GetModuleHandle(NULL)); }

HWND IosMenu_Show(HWND o, int x, int y, const IosMenuItem* i, int ic) {
    if (g_currentMenuHwnd) DestroyWindow(g_currentMenuHwnd);
    int h = MENU_PADDING_TOP + MENU_PADDING_BOTTOM + ic * MENU_ITEM_HEIGHT;
    IosMenuCreateParams p = {i, ic, o, h};
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, g_menuClassName, L"", WS_POPUP, x, y, MENU_WIDTH + MENU_SHADOW_SIZE, h + MENU_SHADOW_SIZE, o, NULL, GetModuleHandle(NULL), &p);
    g_currentMenuHwnd = hwnd; ShowWindow(hwnd, SW_SHOW); SetCapture(hwnd); return hwnd;
}
