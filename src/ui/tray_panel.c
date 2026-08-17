/*
 * 托盘快速操控面板（Quick Control Flyout）
 *
 * 架构与 ios_menu.c 一致：分层窗口 + UpdateLayeredWindow + DIB 像素级抗锯齿渲染。
 * 面板只负责展示与命令转发，业务逻辑全部通过 WM_COMMAND 发给主窗口处理，
 * 因此不依赖任何新的核心逻辑（预设 chips = 现有 ID_PRESET_BASE 命令 + 紧跟开始）。
 */

#include "tray_panel.h"
#include "timer_core.h"
#include "timer_config.h"
#include "timer_render_utils.h"
#include <stdio.h>
#include <uxtheme.h>

// ===========================================
// 布局常量（Win11 快速设置 flyout 风格）
// ===========================================

#define PANEL_WIDTH          320
#define PANEL_SHADOW          20
#define PANEL_SHADOW_OFFSET    4
#define PANEL_RADIUS           8
#define PANEL_PAD_X           16
#define PANEL_PAD_TOP         16
#define PANEL_PAD_BOTTOM      12

#define HEADER_H             44   // 大号时间 + 状态行
#define BTN_H                36   // 主按钮高度
#define BTN_GAP               8
#define CHIP_H               32   // 预设 chip 高度
#define CHIP_GAP              8
#define CHIP_COLS             4
#define ROW_H                36   // 开关/功能行高度
#define SECTION_H            20   // 小节标题高度

#define SWITCH_WIDTH         34

// ===========================================
// 数据结构
// ===========================================

typedef enum {
    REGION_BTN_PRIMARY = 0,   // 主色按钮（开始/暂停）
    REGION_BTN_SECONDARY,     // 次级按钮（重置）
    REGION_CHIP,              // 预设/自定义 chip
    REGION_ROW_SWITCH,        // 带开关的行（点击切换，不关闭）
    REGION_ROW_PLAIN          // 普通功能行
} PanelRegionKind;

typedef struct {
    RECT rc;                  // 面板坐标系（含阴影偏移）中的命中区域
    UINT cmd;                 // 主命令
    UINT cmd2;                // 附加命令（预设 chip 设定时间后紧跟"开始"）
    PanelRegionKind kind;
    BOOL closeAfter;          // 执行后是否关闭面板
    BOOL selected;            // 是否处于选中态（当前时间对应的预设）
} PanelRegion;

#define PANEL_MAX_REGIONS 20   // 2 按钮 + 11 chips + 2 行

typedef struct {
    HWND owner;
    HDC hdcBuffer;
    HBITMAP hbmBuffer;
    void* pBits;
    int bufWidth;
    int bufHeight;
    int contentHeight;
    HFONT hFontBig;           // 大号时间
    HFONT hFontStatus;        // 状态文字
    HFONT hFontLabel;         // 按钮文字
    HFONT hFontSection;       // 小节标题
    HFONT hFontChip;          // chip 文字
    HFONT hFontIcon;          // MDL2 图标
    PanelRegion regions[PANEL_MAX_REGIONS];
    int regionCount;
    int hoverIndex;
    // 变化检测：仅状态变化时重绘
    wchar_t lastTimeStr[40];
    int lastRunning;
    int lastTimeUp;
    int lastOvertime;
    int lastSeconds;
    int lastLang;
} PanelData;

static const wchar_t* PANEL_CLASS = L"TimmerTrayPanelClass";
static BOOL g_panelClassRegistered = FALSE;
static HWND g_panelHwnd = NULL;

// ===========================================
// 文字绘制（与 ios_menu 相同的 DrawThemeTextEx 方案）
// ===========================================

static void DrawTextAA(HDC hdc, HFONT font, const wchar_t* text, RECT rc, COLORREF color, UINT flags) {
    HFONT old = (HFONT)SelectObject(hdc, font);
    HTHEME theme = OpenThemeData(NULL, L"WINDOW");
    DTTOPTS opts = { sizeof(DTTOPTS) };
    opts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;
    opts.crText = color;
    DrawThemeTextEx(theme, hdc, 0, 0, text, -1,
                    flags | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER, &rc, &opts);
    CloseThemeData(theme);
    SelectObject(hdc, old);
}

// FormatTimeCustom 输出为窄字符（ASCII），转宽字符用于 GDI 绘制
static void CurrentTimeToWide(wchar_t* out, size_t cap) {
    char narrow[32];
    FormatTimeCustom(g_timerState.seconds, narrow);
    size_t i = 0;
    for (; narrow[i] != '\0' && i + 1 < cap; i++) {
        out[i] = (wchar_t)(unsigned char)narrow[i];
    }
    out[i] = L'\0';
}

// ===========================================
// 渲染
// ===========================================

static PanelRegion* AddRegion(PanelData* d) {
    if (d->regionCount >= PANEL_MAX_REGIONS) return NULL;
    PanelRegion* r = &d->regions[d->regionCount++];
    ZeroMemory(r, sizeof(PanelRegion));
    return r;
}

static void DrawDivider(PanelData* d, int y) {
    BYTE* pixels = (BYTE*)d->pBits;
    int w = d->bufWidth;
    int mx = PANEL_SHADOW;
    for (int dx = PANEL_PAD_X - 4; dx < PANEL_WIDTH - PANEL_PAD_X + 4; dx++) {
        BlendPixel(pixels, (y * w + mx + dx) * 4, 0, 0, 0, 0.08f);
    }
}

static void RenderPanel(PanelData* d) {
    BYTE* pixels = (BYTE*)d->pBits;
    int w = d->bufWidth;
    int mx = PANEL_SHADOW;
    int my = PANEL_SHADOW;
    const MenuTexts* texts = GetMenuTexts();

    memset(pixels, 0, (size_t)w * d->bufHeight * 4);
    d->regionCount = 0;

    // 软阴影 + 描边 + 玻璃态背景（与右键菜单一致）
    DrawSoftShadowSDF(pixels, w, d->bufHeight, mx, my, PANEL_WIDTH, d->contentHeight,
                      PANEL_RADIUS, PANEL_SHADOW, PANEL_SHADOW_OFFSET, 0.18f);
    FillRoundedRectAA(pixels, w, d->bufHeight, PANEL_RADIUS, PANEL_RADIUS,
                      mx - 1, my - 1, PANEL_WIDTH + 2, d->contentHeight + 2, 0, 0, 0, 15);
    FillRoundedRectAA(pixels, w, d->bufHeight, PANEL_RADIUS, PANEL_RADIUS,
                      mx, my, PANEL_WIDTH, d->contentHeight, 252, 252, 252, 245);

    int contentW = PANEL_WIDTH - PANEL_PAD_X * 2;

    // === 头部：大号时间 + 状态 ===
    int y = my + PANEL_PAD_TOP;
    wchar_t timeStr[40];
    CurrentTimeToWide(timeStr, 40);
    RECT rcTime = { mx + PANEL_PAD_X, y, mx + PANEL_WIDTH / 2 + PANEL_PAD_X, y + HEADER_H };
    DrawTextAA(d->hdcBuffer, d->hFontBig, timeStr, rcTime, RGB(32, 32, 32), DT_LEFT);

    const wchar_t* statusText;
    BYTE dr, dg, db;
    if (g_timerState.isTimeUp) {
        statusText = texts->panelTimeUp;  dr = 196; dg = 43;  db = 28;
    } else if (g_timerState.isRunning) {
        statusText = texts->panelRunning; dr = 0;   dg = 95;  db = 184;
    } else {
        statusText = texts->panelPaused;  dr = 150; dg = 150; db = 150;
    }
    int dotCy = y + HEADER_H / 2;
    FillRoundedRectAA(pixels, w, d->bufHeight, 4, 4,
                      mx + PANEL_WIDTH - PANEL_PAD_X - 8, dotCy - 4, 8, 8, dr, dg, db, 255);
    RECT rcStatus = { mx + PANEL_WIDTH / 2, y, mx + PANEL_WIDTH - PANEL_PAD_X - 14, y + HEADER_H };
    DrawTextAA(d->hdcBuffer, d->hFontStatus, statusText, rcStatus, RGB(102, 102, 102), DT_RIGHT);
    y += HEADER_H + 12;

    // === 按钮行：开始/暂停 + 重置 ===
    int btnW = (contentW - BTN_GAP) / 2;
    {
        PanelRegion* r = AddRegion(d);
        BOOL hovered = r && (d->hoverIndex == d->regionCount - 1);
        r->kind = REGION_BTN_PRIMARY;
        r->cmd = ID_START_PAUSE;
        r->rc.left = mx + PANEL_PAD_X; r->rc.top = y;
        r->rc.right = r->rc.left + btnW; r->rc.bottom = y + BTN_H;
        FillRoundedRectAA(pixels, w, d->bufHeight, 6, 6, r->rc.left, r->rc.top, btnW, BTN_H,
                          0, hovered ? 78 : 95, hovered ? 155 : 184, 255);
        DrawTextAA(d->hdcBuffer, d->hFontLabel,
                   g_timerState.isRunning ? texts->pause : texts->start,
                   r->rc, RGB(255, 255, 255), DT_CENTER);
    }
    {
        PanelRegion* r = AddRegion(d);
        BOOL hovered = r && (d->hoverIndex == d->regionCount - 1);
        r->kind = REGION_BTN_SECONDARY;
        r->cmd = ID_RESET;
        r->rc.left = mx + PANEL_PAD_X + btnW + BTN_GAP; r->rc.top = y;
        r->rc.right = r->rc.left + btnW; r->rc.bottom = y + BTN_H;
        FillRoundedRectAA(pixels, w, d->bufHeight, 6, 6, r->rc.left, r->rc.top, btnW, BTN_H,
                          255, 255, 255, 255);
        DrawRoundedRectOutlineAA(pixels, w, d->bufHeight, 6, 6, r->rc.left, r->rc.top, btnW, BTN_H,
                                 1, 0, 0, 0, 60);
        if (hovered) {
            FillRoundedRectAA(pixels, w, d->bufHeight, 6, 6, r->rc.left, r->rc.top, btnW, BTN_H,
                              229, 229, 229, 255);
        }
        DrawTextAA(d->hdcBuffer, d->hFontLabel, texts->reset, r->rc, RGB(32, 32, 32), DT_CENTER);
    }
    y += BTN_H + 12;
    DrawDivider(d, y);
    y += 13;

    // === 快速开始：预设 chips ===
    RECT rcSection = { mx + PANEL_PAD_X, y, mx + PANEL_WIDTH - PANEL_PAD_X, y + SECTION_H };
    DrawTextAA(d->hdcBuffer, d->hFontSection, texts->panelQuickStart, rcSection,
               RGB(102, 102, 102), DT_LEFT);
    y += SECTION_H + 8;

    int chipCount = g_timerState.presetCount + 1;  // 末尾固定为"自定义"
    int chipW = (contentW - CHIP_GAP * (CHIP_COLS - 1)) / CHIP_COLS;
    for (int i = 0; i < chipCount; i++) {
        int row = i / CHIP_COLS;
        int col = i % CHIP_COLS;
        int cx = mx + PANEL_PAD_X + col * (chipW + CHIP_GAP);
        int cy = y + row * (CHIP_H + CHIP_GAP);
        int idx = d->regionCount;
        PanelRegion* r = AddRegion(d);
        if (!r) break;
        BOOL hovered = (d->hoverIndex == idx);
        r->kind = REGION_CHIP;
        r->rc.left = cx; r->rc.top = cy;
        r->rc.right = cx + chipW; r->rc.bottom = cy + CHIP_H;

        wchar_t label[32];
        if (i < g_timerState.presetCount) {
            swprintf_s(label, 32, L"%d", g_timerState.presetTimes[i]);
            r->cmd = ID_PRESET_BASE + i;
            r->cmd2 = ID_START_PAUSE;  // 快速开始语义：设定并立即启动
            r->selected = (g_timerState.seconds == g_timerState.presetTimes[i] * 60);
        } else {
            wcscpy_s(label, 32, texts->panelCustom);
            r->cmd = ID_SET_TIME;
            r->closeAfter = TRUE;     // 打开自定义时间对话框前收起面板
        }

        if (r->selected) {
            FillRoundedRectAA(pixels, w, d->bufHeight, 6, 6, cx, cy, chipW, CHIP_H, 0, 95, 184, 255);
            DrawTextAA(d->hdcBuffer, d->hFontChip, label, r->rc, RGB(255, 255, 255),
                       DT_CENTER | DT_END_ELLIPSIS);
        } else {
            FillRoundedRectAA(pixels, w, d->bufHeight, 6, 6, cx, cy, chipW, CHIP_H,
                              hovered ? 229 : 255, hovered ? 229 : 255, hovered ? 229 : 255, 255);
            DrawRoundedRectOutlineAA(pixels, w, d->bufHeight, 6, 6, cx, cy, chipW, CHIP_H,
                                     1, 0, 0, 0, hovered ? 130 : 60);
            DrawTextAA(d->hdcBuffer, d->hFontChip, label, r->rc, RGB(32, 32, 32),
                       DT_CENTER | DT_END_ELLIPSIS);
        }
    }
    int chipRows = (chipCount + CHIP_COLS - 1) / CHIP_COLS;
    y += chipRows * CHIP_H + (chipRows - 1) * CHIP_GAP + 12;
    DrawDivider(d, y);
    y += 13;

    // === 功能行：超时正计时开关 ===
    {
        int idx = d->regionCount;
        PanelRegion* r = AddRegion(d);
        BOOL hovered = r && (d->hoverIndex == idx);
        r->kind = REGION_ROW_SWITCH;
        r->cmd = ID_OVERTIME_COUNT;  // 点击后不关闭，实时反映开关状态
        r->rc.left = mx + PANEL_PAD_X - 8; r->rc.top = y;
        r->rc.right = mx + PANEL_WIDTH - PANEL_PAD_X + 8; r->rc.bottom = y + ROW_H;
        if (hovered) {
            FillRoundedRectAA(pixels, w, d->bufHeight, 6, 6, mx + 6, y + 2,
                              PANEL_WIDTH - 12, ROW_H - 4, 0, 0, 0, 12);
        }
        RECT rcIcon = { mx + 12, y, mx + 44, y + ROW_H };
        DrawTextAA(d->hdcBuffer, d->hFontIcon, L"\uE916", rcIcon, RGB(102, 102, 102), DT_CENTER);
        RECT rcLabel = { mx + 44, y, mx + PANEL_WIDTH - PANEL_PAD_X - SWITCH_WIDTH - 12, y + ROW_H };
        DrawTextAA(d->hdcBuffer, d->hFontLabel, texts->overtimeCount, rcLabel,
                   RGB(32, 32, 32), DT_LEFT);
        DrawSwitchWin11(pixels, w, d->bufHeight,
                        mx + PANEL_WIDTH - PANEL_PAD_X - SWITCH_WIDTH, y, ROW_H,
                        g_timerState.enableOvertimeCount);
    }
    y += ROW_H;

    // === 功能行：显示主窗口 ===
    {
        int idx = d->regionCount;
        PanelRegion* r = AddRegion(d);
        BOOL hovered = r && (d->hoverIndex == idx);
        r->kind = REGION_ROW_PLAIN;
        r->cmd = ID_TRAY_SHOW;
        r->closeAfter = TRUE;
        r->rc.left = mx + PANEL_PAD_X - 8; r->rc.top = y;
        r->rc.right = mx + PANEL_WIDTH - PANEL_PAD_X + 8; r->rc.bottom = y + ROW_H;
        if (hovered) {
            FillRoundedRectAA(pixels, w, d->bufHeight, 6, 6, mx + 6, y + 2,
                              PANEL_WIDTH - 12, ROW_H - 4, 0, 0, 0, 12);
        }
        RECT rcIcon = { mx + 12, y, mx + 44, y + ROW_H };
        DrawTextAA(d->hdcBuffer, d->hFontIcon, L"\uE8A7", rcIcon, RGB(102, 102, 102), DT_CENTER);
        RECT rcLabel = { mx + 44, y, mx + PANEL_WIDTH - PANEL_PAD_X, y + ROW_H };
        DrawTextAA(d->hdcBuffer, d->hFontLabel, texts->panelShowWindow, rcLabel,
                   RGB(32, 32, 32), DT_LEFT);
    }
}

static void UpdatePanelDisplay(HWND hwnd, PanelData* d) {
    RenderPanel(d);
    HDC hdc = GetDC(NULL);
    POINT dst = {0, 0}, src = {0, 0};
    SIZE sz = {d->bufWidth, d->bufHeight};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT rc;
    GetWindowRect(hwnd, &rc);
    dst.x = rc.left; dst.y = rc.top;
    UpdateLayeredWindow(hwnd, hdc, &dst, &sz, d->hdcBuffer, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, hdc);
}

// ===========================================
// 定位：锚定在托盘图标上方
// ===========================================

// Shell_NotifyIconGetRect 的本地声明：避免依赖编译期 _WIN32_WINNT 版本判断
typedef struct {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    GUID guidItem;
} PanelNotifyIconIdentifier;

typedef HRESULT (WINAPI *PanelNotifyIconGetRectFn)(PanelNotifyIconIdentifier*, RECT*);

static BOOL GetTrayIconRect(HWND owner, RECT* rcOut) {
    PanelNotifyIconGetRectFn fn = (PanelNotifyIconGetRectFn)(void(*)(void))
        GetProcAddress(GetModuleHandleW(L"shell32.dll"), "Shell_NotifyIconGetRect");
    if (!fn) return FALSE;
    PanelNotifyIconIdentifier nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = owner;
    nid.uID = TRAY_ICON_ID;
    return SUCCEEDED(fn(&nid, rcOut));
}

static void ComputePopupPosition(int panelW, int panelH, int* outX, int* outY) {
    RECT rcIcon;
    if (!GetTrayIconRect(g_timerState.hMainWnd, &rcIcon)) {
        // 拿不到图标矩形时退化为以光标为锚点
        POINT cp;
        GetCursorPos(&cp);
        rcIcon.left = cp.x - 2; rcIcon.right = cp.x + 2;
        rcIcon.top = cp.y - 2;  rcIcon.bottom = cp.y + 2;
    }

    POINT center = { (rcIcon.left + rcIcon.right) / 2, (rcIcon.top + rcIcon.bottom) / 2 };
    HMONITOR monitor = MonitorFromPoint(center, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfoW(monitor, &mi);

    int x = center.x - panelW / 2;
    if (x < mi.rcWork.left) x = mi.rcWork.left;
    if (x + panelW > mi.rcWork.right) x = mi.rcWork.right - panelW;

    int y = rcIcon.top - panelH - 8;   // 默认在图标（任务栏）上方
    if (y < mi.rcWork.top) y = rcIcon.bottom + 8;
    if (y + panelH > mi.rcWork.bottom) y = mi.rcWork.bottom - panelH;
    if (y < mi.rcWork.top) y = mi.rcWork.top;

    *outX = x;
    *outY = y;
}

// ===========================================
// 窗口过程
// ===========================================

static int HitTestRegions(PanelData* d, POINT pt) {
    for (int i = 0; i < d->regionCount; i++) {
        if (PtInRect(&d->regions[i].rc, pt)) return i;
    }
    return -1;
}

static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PanelData* d = (PanelData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            d = (PanelData*)calloc(1, sizeof(PanelData));
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)d);
            d->owner = *(HWND*)cs->lpCreateParams;
            d->hoverIndex = -1;

            RECT rc;
            GetClientRect(hwnd, &rc);
            d->bufWidth = rc.right;
            d->bufHeight = rc.bottom;
            d->contentHeight = d->bufHeight - PANEL_SHADOW * 2;

            HDC hdc = GetDC(NULL);
            d->hdcBuffer = CreateCompatibleDC(hdc);
            BITMAPINFOHEADER bi = { sizeof(bi), d->bufWidth, -d->bufHeight, 1, 32, BI_RGB };
            d->hbmBuffer = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &d->pBits, NULL, 0);
            SelectObject(d->hdcBuffer, d->hbmBuffer);
            d->hFontBig = CreateFontW(38, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                      0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
            d->hFontStatus = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                         0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
            d->hFontLabel = CreateFontW(18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                        0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
            d->hFontSection = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                          0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
            d->hFontChip = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                       0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
            d->hFontIcon = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                       0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");
            ReleaseDC(NULL, hdc);

            UpdatePanelDisplay(hwnd, d);
            SetTimer(hwnd, 1, 30, NULL);
            return 0;
        }

        case WM_TIMER: {
            if (!d || !IsWindow(d->owner)) {
                DestroyWindow(hwnd);
                return 0;
            }
            // 悬停检测（与 ios_menu 相同的轮询方案）
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            int hover = HitTestRegions(d, pt);

            // 变化检测：时间、运行状态、开关、语言任一变化才重绘
            wchar_t timeStr[40];
            CurrentTimeToWide(timeStr, 40);
            BOOL changed = (hover != d->hoverIndex)
                        || wcscmp(timeStr, d->lastTimeStr) != 0
                        || g_timerState.isRunning != d->lastRunning
                        || g_timerState.isTimeUp != d->lastTimeUp
                        || g_timerState.enableOvertimeCount != d->lastOvertime
                        || g_timerState.seconds != d->lastSeconds
                        || (int)g_timerState.currentLanguage != d->lastLang;
            if (changed) {
                d->hoverIndex = hover;
                wcscpy_s(d->lastTimeStr, 40, timeStr);
                d->lastRunning = g_timerState.isRunning;
                d->lastTimeUp = g_timerState.isTimeUp;
                d->lastOvertime = g_timerState.enableOvertimeCount;
                d->lastSeconds = g_timerState.seconds;
                d->lastLang = (int)g_timerState.currentLanguage;
                UpdatePanelDisplay(hwnd, d);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (!d) return 0;
            POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
            int idx = HitTestRegions(d, pt);
            if (idx < 0) {
                DestroyWindow(hwnd);  // 点击面板外任意处关闭
                return 0;
            }
            PanelRegion* r = &d->regions[idx];
            if (r->cmd) SendMessage(d->owner, WM_COMMAND, MAKEWPARAM(r->cmd, 0), 0);
            if (r->cmd2) SendMessage(d->owner, WM_COMMAND, MAKEWPARAM(r->cmd2, 0), 0);
            if (r->closeAfter) {
                DestroyWindow(hwnd);
            } else {
                UpdatePanelDisplay(hwnd, d);  // 立即反映命令后的状态
            }
            return 0;
        }

        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_CAPTURECHANGED:
            if (g_panelHwnd == hwnd) DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY: {
            if (d) {
                KillTimer(hwnd, 1);
                DeleteObject(d->hbmBuffer);
                DeleteDC(d->hdcBuffer);
                DeleteObject(d->hFontBig);
                DeleteObject(d->hFontStatus);
                DeleteObject(d->hFontLabel);
                DeleteObject(d->hFontSection);
                DeleteObject(d->hFontChip);
                DeleteObject(d->hFontIcon);
                free(d);
            }
            g_panelHwnd = NULL;
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ===========================================
// 对外接口
// ===========================================

BOOL TrayPanel_Initialize(HINSTANCE hInstance) {
    if (g_panelClassRegistered) return TRUE;
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = PanelProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = PANEL_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;
    g_panelClassRegistered = TRUE;
    return TRUE;
}

void TrayPanel_Shutdown(void) {
    if (g_panelHwnd && IsWindow(g_panelHwnd)) DestroyWindow(g_panelHwnd);
    if (g_panelClassRegistered) {
        UnregisterClassW(PANEL_CLASS, GetModuleHandle(NULL));
        g_panelClassRegistered = FALSE;
    }
}

static int ComputeContentHeight(void) {
    int chipCount = g_timerState.presetCount + 1;
    int chipRows = (chipCount + CHIP_COLS - 1) / CHIP_COLS;
    return PANEL_PAD_TOP + HEADER_H + 12
         + BTN_H + 12 + 1 + 12
         + SECTION_H + 8
         + chipRows * CHIP_H + (chipRows - 1) * CHIP_GAP + 12 + 1 + 12
         + 2 * ROW_H
         + PANEL_PAD_BOTTOM;
}

void TrayPanel_Toggle(HWND owner) {
    if (g_panelHwnd && IsWindow(g_panelHwnd)) {
        DestroyWindow(g_panelHwnd);
        return;
    }

    int contentH = ComputeContentHeight();
    int fullW = PANEL_WIDTH + PANEL_SHADOW * 2;
    int fullH = contentH + PANEL_SHADOW * 2;
    int x, y;
    ComputePopupPosition(PANEL_WIDTH, contentH, &x, &y);

    HWND param = owner;
    g_panelHwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        PANEL_CLASS, L"", WS_POPUP,
        x - PANEL_SHADOW, y - PANEL_SHADOW, fullW, fullH,
        owner, NULL, GetModuleHandle(NULL), &param);
    if (!g_panelHwnd) return;

    ShowWindow(g_panelHwnd, SW_SHOW);
    SetCapture(g_panelHwnd);
}
