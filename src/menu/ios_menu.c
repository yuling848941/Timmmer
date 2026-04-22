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

#include "timer_render_utils.h"

// ===========================================
// iOS 风格设计常量（旗舰版）
// ===========================================

#define MENU_WIDTH              250     
#define MENU_ITEM_HEIGHT        36      
#define MENU_RADIUS             8      
#define MENU_SHADOW_SIZE        20      
#define MENU_SHADOW_OFFSET_Y    4      

#define MENU_PADDING_TOP        6       
#define MENU_PADDING_BOTTOM     6       
#define MENU_PADDING_LEFT       12      
#define MENU_PADDING_RIGHT      12      

#define MENU_ICON_SIZE          16      
#define MENU_ICON_BOX_W         32      
#define MENU_TEXT_OFFSET        44      

#define SWITCH_WIDTH            34      
#define SWITCH_HEIGHT           18      

#define COLOR_TEXT_PRIMARY      RGB(30, 30, 30)
#define COLOR_ACCENT            RGB(0, 103, 192)

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
    BOOL isDraggingOwner;
    POINT dragStartScreen;
    RECT ownerStartRect;
} MenuData;

// ===========================================
// 绘图引擎（抗锯齿）
// 核心函数已移至 timer_render_utils.c
// ===========================================

static void DrawSwitchWin11(BYTE* pixels, int bufW, int bufH, int x, int y, BOOL isOn) {
    int sw = SWITCH_WIDTH, sh = SWITCH_HEIGHT, ts = 10;
    int cy = y + (MENU_ITEM_HEIGHT - sh) / 2;
    if (isOn) {
        FillRoundedRectAA(pixels, bufW, bufH, sh/2, sh/2, x, cy, sw, sh, 0, 103, 192, 255);
        FillRoundedRectAA(pixels, bufW, bufH, ts/2, ts/2, x + sw - ts - 4, cy + 4, ts, ts, 255, 255, 255, 255);
    } else {
        FillRoundedRectAA(pixels, bufW, bufH, sh/2, sh/2, x, cy, sw, sh, 180, 180, 180, 255);
        FillRoundedRectAA(pixels, bufW, bufH, (sh-2)/2, (sh-2)/2, x + 1, cy + 1, sw - 2, sh - 2, 255, 255, 255, 255);
        FillRoundedRectAA(pixels, bufW, bufH, ts/2, ts/2, x + 5, cy + 4, ts, ts, 120, 120, 120, 255);
    }
}

// ===========================================
// 菜单项内容绘制
// ===========================================

static void DrawItemContent(MenuData* data, int idx, int mx, int y) {
    const IosMenuItem* it = &data->items[idx];
    HDC hdc = data->hdcBuffer;
    int ix = mx + MENU_PADDING_LEFT;
    int tx = mx + MENU_TEXT_OFFSET;

    HTHEME hTheme = OpenThemeData(NULL, L"WINDOW");
    DTTOPTS dttOpts = { sizeof(DTTOPTS) };
    dttOpts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;

    if (it->iconChar && wcslen(it->iconChar) == 1) {
        SelectObject(hdc, data->hFontIcon); 
        dttOpts.crText = RGB(90, 90, 90);
        RECT rc = {ix, y, ix + MENU_ICON_BOX_W, y + MENU_ITEM_HEIGHT};
        DrawThemeTextEx(hTheme, hdc, 0, 0, it->iconChar, 1, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, &rc, &dttOpts);
    }

    SelectObject(hdc, data->hFontLabel); 
    dttOpts.crText = COLOR_TEXT_PRIMARY;
    RECT rcText = {tx, y, mx + MENU_WIDTH - MENU_PADDING_RIGHT, y + MENU_ITEM_HEIGHT};
    DrawThemeTextEx(hTheme, hdc, 0, 0, it->label, -1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, &rcText, &dttOpts);

    CloseThemeData(hTheme);
}

// ===========================================
// 渲染主逻辑
// ===========================================

static void RenderMenuContent(MenuData* data) {
    BYTE* pixels = (BYTE*)data->pBits;
    int w = data->bufWidth, h = data->bufHeight, mh = data->menuHeight;
    int sz = w * h * 4;

    memset(pixels, 0, sz);
    
    int mx = MENU_SHADOW_SIZE;
    int my = MENU_SHADOW_SIZE;

    // 绘制全局真软阴影
    DrawSoftShadowSDF(pixels, w, h, mx, my, MENU_WIDTH, mh, MENU_RADIUS, MENU_SHADOW_SIZE, MENU_SHADOW_OFFSET_Y, 0.18f);
    
    // 绘制1px微弱描边 (通过画一个稍大的底层实现)
    FillRoundedRectAA(pixels, w, h, MENU_RADIUS, MENU_RADIUS, mx - 1, my - 1, MENU_WIDTH + 2, mh + 2, 0, 0, 0, 15);
    
    // 绘制菜单主体背景 (半透明玻璃态)
    FillRoundedRectAA(pixels, w, h, MENU_RADIUS, MENU_RADIUS, mx, my, MENU_WIDTH, mh, 252, 252, 252, 245);

    int y = my + MENU_PADDING_TOP;
    for (int i = 0; i < data->itemCount; i++) {
        const IosMenuItem* it = &data->items[i];
        if (i == data->hoverIndex) {
            FillRoundedRectAA(pixels, w, h, 6, 6, mx + 6, y + 2, MENU_WIDTH - 12, MENU_ITEM_HEIGHT - 4, 0, 0, 0, 12);
        }
        if (it->type == IOS_MENU_ITEM_SWITCH) {
            DrawSwitchWin11(pixels, w, h, mx + MENU_WIDTH - SWITCH_WIDTH - MENU_PADDING_RIGHT, y, it->isSwitchOn);
        }
        if (it->hasDividerAfter && i < data->itemCount - 1) {
            int ly = y + MENU_ITEM_HEIGHT;
            for (int dx = MENU_TEXT_OFFSET; dx < MENU_WIDTH - MENU_PADDING_RIGHT; dx++) BlendPixel(pixels, (ly * w + mx + dx) * 4, 0, 0, 0, 0.08f);
        }
        DrawItemContent(data, i, mx, y);
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
            int mx = MENU_SHADOW_SIZE;
            int my = MENU_SHADOW_SIZE;
            if (pt.x >= mx && pt.x < mx + MENU_WIDTH && pt.y >= my + MENU_PADDING_TOP && pt.y < my + d->menuHeight - MENU_PADDING_BOTTOM) {
                h = (pt.y - my - MENU_PADDING_TOP) / MENU_ITEM_HEIGHT; if (h >= d->itemCount) h = -1;
            }
            if (h != d->hoverIndex) {
                d->hoverIndex = h;
                UpdateMenuDisplay(hwnd, d);
                // 向 owner 发送悬停预览命令（commandId + 10000 区分正式选择）
                if (h >= 0 && h < d->itemCount && d->items[h].commandId != 0 && d->owner) {
                    SendMessage(d->owner, WM_COMMAND, MAKEWPARAM(d->items[h].commandId + 10000, 0), 0);
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            int mx = MENU_SHADOW_SIZE;
            int my = MENU_SHADOW_SIZE;
            if (pt.x >= mx && pt.x < mx + MENU_WIDTH &&
                pt.y >= my + MENU_PADDING_TOP && pt.y < my + d->menuHeight - MENU_PADDING_BOTTOM)
            {
                int i = (pt.y - my - MENU_PADDING_TOP) / MENU_ITEM_HEIGHT;
                if (i >= 0 && i < d->itemCount) {
                    const IosMenuItem* it = &d->items[i];
                    if (it->commandId != 0) SendMessage(d->owner, WM_COMMAND, MAKEWPARAM(it->commandId, 0), 0);
                }
                DestroyWindow(hwnd);
            } else {
                GetCursorPos(&d->dragStartScreen);
                GetWindowRect(d->owner, &d->ownerStartRect);
                d->isDraggingOwner = TRUE;
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (d->isDraggingOwner && d->owner && IsWindow(d->owner)) {
                POINT cur; GetCursorPos(&cur);
                int dx = cur.x - d->dragStartScreen.x;
                int dy = cur.y - d->dragStartScreen.y;
                SetWindowPos(d->owner, NULL,
                    d->ownerStartRect.left + dx, d->ownerStartRect.top + dy,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (d->isDraggingOwner) {
                d->isDraggingOwner = FALSE;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CAPTURECHANGED: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            if (d && d->isDraggingOwner) d->isDraggingOwner = FALSE;
            if (g_currentMenuHwnd == hwnd) DestroyWindow(hwnd); return 0;
        case WM_DESTROY: {
            // 菜单关闭时通知 owner 恢复字体（取消悬停预览状态）
            if (d && d->owner) {
                SendMessage(d->owner, WM_COMMAND, MAKEWPARAM(9999, 0), 0);
            }
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
    
    int fullWidth = MENU_WIDTH + MENU_SHADOW_SIZE * 2;
    int fullHeight = h + MENU_SHADOW_SIZE * 2;

    int visualX = x;
    int visualY = y;

    // 获取当前点所在的显示器工作区，确保菜单主体不会超出屏幕
    POINT pt = { x, y };
    HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(hMonitor, (LPMONITORINFO)&mi)) {
        // 如果右侧超出屏幕，将菜单显示在点击位置的左侧
        if (visualX + MENU_WIDTH > mi.rcWork.right) {
            visualX = x - MENU_WIDTH;
        }
        // 如果下方超出屏幕，将菜单显示在点击位置的上方
        if (visualY + h > mi.rcWork.bottom) {
            visualY = y - h;
        }
        
        // 确保不超出工作区的左侧和上方边缘
        if (visualX < mi.rcWork.left) visualX = mi.rcWork.left;
        if (visualY < mi.rcWork.top) visualY = mi.rcWork.top;
    }

    int winX = visualX - MENU_SHADOW_SIZE;
    int winY = visualY - MENU_SHADOW_SIZE;

    IosMenuCreateParams p = {i, ic, o, h};
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, g_menuClassName, L"", WS_POPUP, winX, winY, fullWidth, fullHeight, o, NULL, GetModuleHandle(NULL), &p);
    g_currentMenuHwnd = hwnd; ShowWindow(hwnd, SW_SHOW); SetCapture(hwnd); return hwnd;
}
