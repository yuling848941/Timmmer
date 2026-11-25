/*
 * 托盘快速操控面板（Quick Control Flyout）
 *
 * 架构与 ios_menu.c 一致：分层窗口 + UpdateLayeredWindow + DIB 像素级抗锯齿渲染。
 * 面板只负责展示与命令转发，业务逻辑全部通过 WM_COMMAND 发给主窗口处理，
 * 因此不依赖任何新的核心逻辑（预设 chips = 现有 ID_PRESET_BASE 命令 + 紧跟开始）。
 *
 * 视觉：与右键菜单同一套主题令牌（timer_theme.c）——玻璃底、辉光、强调色。
 * 头部的大号时间使用计时器当前选中的字体，使面板与计时器主体天然协调。
 */

#include "tray_panel.h"
#include "timer_core.h"
#include "timer_config.h"
#include "timer_render_utils.h"
#include "timer_theme.h"
#include "timer_fonts.h"
#include <stdio.h>
#include <uxtheme.h>

// ===========================================
// 布局常量
// 以下均为 96 DPI 基准值，运行时经 PX() 按面板所在显示器 DPI 缩放
// ===========================================

static UINT g_panelDpi = 96;
#define PX(v) ScalePx((v), g_panelDpi)

#define PANEL_WIDTH          PX(360)
#define PANEL_SHADOW         PX(24)
#define PANEL_RADIUS         PX(12)
#define PANEL_PAD_X          PX(18)
#define PANEL_PAD_TOP        PX(16)
#define PANEL_PAD_BOTTOM     PX(18)

#define HEADER_H             PX(52)   // 大号时间 + 状态行
#define BTN_H                PX(40)   // 主按钮高度
#define BTN_GAP              PX(8)
#define CHIP_H               PX(36)   // 预设 chip 高度
#define CHIP_GAP             PX(8)
#define CHIP_COLS             4
#define ROW_H                PX(40)   // 开关/功能行高度
#define SECTION_H            PX(21)   // 小节标题高度

#define SWITCH_WIDTH         PX(38)
#define HAIRLINE             (max(1, PX(1)))

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
    wchar_t label[32];        // chip 显示文字（在形状段按预设序号算好，文字段直接取用）
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
    BYTE* alphaSnapshot;      // GDI 绘制文字前的 alpha 通道快照
    Backdrop backdrop;        // 面板弹出时背后的屏幕快照（用于玻璃底）
    GlassCache glass;         // 预渲染的玻璃底图：投影 + 玻璃 + 描边
    POINT finalPos;           // 窗口左上角的屏幕位置
    HFONT hFontBig;           // 大号时间（计时器当前字体）
    HFONT hFontStatus;        // 状态文字
    HFONT hFontLabel;         // 行文字
    HFONT hFontBtn;           // 按钮文字
    HFONT hFontSection;       // 小节标题
    HFONT hFontChip;          // chip 文字
    HFONT hFontIcon;          // 图标
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
// 字体与文字
// ===========================================

// 头部时间用的字体：计时器当前选中的字体（通常是内置的数字字体）。
// 该字体不可用时退回 UI 字体的半粗体，而不是让系统静默换成别的字体。
static HFONT CreateDisplayFont(int emPx) {
    const wchar_t* name = g_timerState.currentFontName;
    if (name && name[0] && IsFontInstalled(name)) {
        return CreateFontW(-emPx, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, 0,
                           CLEARTYPE_QUALITY, 0, name);
    }
    return CreateUiFont(emPx, FW_SEMIBOLD, TRUE);
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

// 面板内部已不透明，直接用 GDI 绘制以保留 ClearType
static void DrawPanelText(HDC hdc, HFONT font, const wchar_t* text, RECT rc, COLORREF color, UINT flags) {
    DrawTextClearType(hdc, font, text, rc, color, flags);
}

// ===========================================
// 玻璃底图
// ===========================================

// 面板显示前捕获它背后的屏幕并生成玻璃底图；之后的重绘只拷贝底图。
// 窗口此刻尚未显示，不会把自己拍进快照。
static void BuildPanelGlass(HWND hwnd, PanelData* d) {
    RECT wr;
    GetWindowRect(hwnd, &wr);
    d->finalPos.x = wr.left;
    d->finalPos.y = wr.top;
    Backdrop_Release(&d->backdrop);
    Backdrop_Capture(&d->backdrop, wr.left, wr.top, d->bufWidth, d->bufHeight);
    GlassCache_Build(&d->glass, d->bufWidth, d->bufHeight,
                     PANEL_SHADOW, PANEL_SHADOW, PANEL_WIDTH, d->contentHeight,
                     PANEL_RADIUS, PANEL_SHADOW, PX(9),
                     &d->backdrop, wr.left, wr.top, Ui_GetTheme());
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

static void RenderPanel(PanelData* d) {
    BYTE* pixels = (BYTE*)d->pBits;
    int w = d->bufWidth;
    int mx = PANEL_SHADOW;
    int my = PANEL_SHADOW;
    const UiTheme* theme = Ui_GetTheme();
    const MenuTexts* texts = GetMenuTexts();
    size_t pixelCount = (size_t)w * d->bufHeight;
    float glow = theme->glow / 255.0f;

// 取颜色的 R/G/B 分量
#define CR(c) GetRValue(c)
#define CG(c) GetGValue(c)
#define CB(c) GetBValue(c)

    // 底图：投影 + 玻璃底 + 描边（预先算好，这里只拷贝）
    memset(pixels, 0, pixelCount * 4);
    GlassCache_Blit(&d->glass, pixels, w, d->bufHeight);

    int contentW = PANEL_WIDTH - PANEL_PAD_X * 2;
    int hair = HAIRLINE;

    // === 头部：大号时间 + 状态 ===
    int y = my + PANEL_PAD_TOP;
    wchar_t timeStr[40];
    CurrentTimeToWide(timeStr, 40);
    // 状态文字（含圆点）固定占右侧 PX(110)，其余宽度留给时间：开到毫秒（00:00:00.000）也不会被裁掉
    int statusRight = mx + PANEL_WIDTH - PANEL_PAD_X - PX(22);
    RECT rcTime = { mx + PANEL_PAD_X, y, statusRight - PX(110), y + HEADER_H };

    const wchar_t* statusText;
    COLORREF statusColor;
    if (g_timerState.isTimeUp) {
        statusText = texts->panelTimeUp;  statusColor = theme->danger;
    } else if (g_timerState.isRunning) {
        statusText = texts->panelRunning; statusColor = theme->accent;
    } else {
        statusText = texts->panelPaused;  statusColor = theme->dim;
    }
    int dot = PX(6);
    int dotCy = y + HEADER_H / 2;
    GlowRoundedRect(pixels, w, d->bufHeight, dot, dot, statusRight - dot, dotCy - dot / 2, dot * 2, dot * 2,
                    CR(statusColor), CG(statusColor), CB(statusColor), g_timerState.isRunning ? 0.7f * glow : 0.0f, PX(14));
    FillRoundedRectAA(pixels, w, d->bufHeight, dot / 2, dot / 2, statusRight - dot, dotCy - dot / 2, dot, dot,
                      CR(statusColor), CG(statusColor), CB(statusColor), 255);

    // 时间的辉光贴合字形：把文字画到临时缓冲做两级模糊再叠加
    DrawTextGlow(pixels, w, d->bufHeight, d->hdcBuffer, d->hFontBig, timeStr, rcTime, DT_LEFT | DT_END_ELLIPSIS,
                 theme->accentFill, glow * 0.8f, PX(10), !theme->isDark);
    y += HEADER_H + PX(12);

    // === 按钮行：开始/暂停 + 重置 ===
    int btnW = (contentW - BTN_GAP) / 2;
    {
        PanelRegion* r = AddRegion(d);
        BOOL hovered = r && (d->hoverIndex == d->regionCount - 1);
        r->kind = REGION_BTN_PRIMARY;
        r->cmd = ID_START_PAUSE;
        r->rc.left = mx + PANEL_PAD_X; r->rc.top = y;
        r->rc.right = r->rc.left + btnW; r->rc.bottom = y + BTN_H;
        COLORREF fill = hovered ? theme->accentHover : theme->accentFill;
        GlowRoundedRect(pixels, w, d->bufHeight, PX(6), PX(6), r->rc.left, r->rc.top, btnW, BTN_H,
                        CR(fill), CG(fill), CB(fill), glow * 0.55f, PX(18));
        FillRoundedRectAA(pixels, w, d->bufHeight, PX(6), PX(6), r->rc.left, r->rc.top, btnW, BTN_H,
                          CR(fill), CG(fill), CB(fill), 255);
    }
    {
        PanelRegion* r = AddRegion(d);
        BOOL hovered = r && (d->hoverIndex == d->regionCount - 1);
        r->kind = REGION_BTN_SECONDARY;
        r->cmd = ID_RESET;
        r->rc.left = mx + PANEL_PAD_X + btnW + BTN_GAP; r->rc.top = y;
        r->rc.right = r->rc.left + btnW; r->rc.bottom = y + BTN_H;
        FillRoundedRectAA(pixels, w, d->bufHeight, PX(6), PX(6), r->rc.left, r->rc.top, btnW, BTN_H,
                          CR(theme->ctrl), CG(theme->ctrl), CB(theme->ctrl),
                          (BYTE)(hovered ? min(255, theme->ctrlAlpha + 40) : theme->ctrlAlpha));
        DrawRoundedRectOutlineAA(pixels, w, d->bufHeight, PX(6), PX(6), r->rc.left, r->rc.top, btnW, BTN_H,
                                 hair, CR(theme->ctrlLine), CG(theme->ctrlLine), CB(theme->ctrlLine), theme->ctrlLineAlpha);
    }
    y += BTN_H + PX(12);
    DrawHLine(pixels, w, mx + PANEL_PAD_X, mx + PANEL_WIDTH - PANEL_PAD_X, y,
              CR(theme->hair), CG(theme->hair), CB(theme->hair), theme->hairAlpha, hair);
    y += PX(13);

    // === 快速开始：预设 chips ===
    RECT rcSection = { mx + PANEL_PAD_X, y, mx + PANEL_WIDTH - PANEL_PAD_X, y + SECTION_H };
    y += SECTION_H + PX(8);

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
        wcscpy_s(r->label, 32, label);   // 标签在此按预设序号算好，文字段直接取用

        if (r->selected) {
            GlowRoundedRect(pixels, w, d->bufHeight, PX(6), PX(6), cx, cy, chipW, CHIP_H,
                            CR(theme->accentFill), CG(theme->accentFill), CB(theme->accentFill), glow * 0.45f, PX(16));
            FillRoundedRectAA(pixels, w, d->bufHeight, PX(6), PX(6), cx, cy, chipW, CHIP_H,
                              CR(theme->accentFill), CG(theme->accentFill), CB(theme->accentFill), 255);
        } else {
            FillRoundedRectAA(pixels, w, d->bufHeight, PX(6), PX(6), cx, cy, chipW, CHIP_H,
                              CR(theme->ctrl), CG(theme->ctrl), CB(theme->ctrl),
                              (BYTE)(hovered ? min(255, theme->ctrlAlpha + 40) : theme->ctrlAlpha));
            DrawRoundedRectOutlineAA(pixels, w, d->bufHeight, PX(6), PX(6), cx, cy, chipW, CHIP_H,
                                     hair, CR(theme->ctrlLine), CG(theme->ctrlLine), CB(theme->ctrlLine),
                                     (BYTE)(hovered ? min(255, theme->ctrlLineAlpha + 50) : theme->ctrlLineAlpha));
        }
    }
    int chipRows = (chipCount + CHIP_COLS - 1) / CHIP_COLS;
    y += chipRows * CHIP_H + (chipRows - 1) * CHIP_GAP + PX(12);
    DrawHLine(pixels, w, mx + PANEL_PAD_X, mx + PANEL_WIDTH - PANEL_PAD_X, y,
              CR(theme->hair), CG(theme->hair), CB(theme->hair), theme->hairAlpha, hair);
    y += PX(13);

    // === 功能行：超时正计时开关 ===
    {
        int idx = d->regionCount;
        PanelRegion* r = AddRegion(d);
        BOOL hovered = r && (d->hoverIndex == idx);
        r->kind = REGION_ROW_SWITCH;
        r->cmd = ID_OVERTIME_COUNT;  // 点击后不关闭，实时反映开关状态
        r->rc.left = mx + PANEL_PAD_X - PX(10); r->rc.top = y;
        r->rc.right = mx + PANEL_WIDTH - PANEL_PAD_X + PX(10); r->rc.bottom = y + ROW_H;
        if (hovered) {
            FillRoundedRectAA(pixels, w, d->bufHeight, PX(6), PX(6), mx + PX(6), y,
                              PANEL_WIDTH - PX(12), ROW_H,
                              CR(theme->hover), CG(theme->hover), CB(theme->hover), theme->hoverAlpha);
        }
        DrawSwitchThemed(pixels, w, d->bufHeight,
                         mx + PANEL_WIDTH - PANEL_PAD_X - SWITCH_WIDTH, y, ROW_H,
                         g_timerState.enableOvertimeCount, g_panelDpi, theme);
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
        r->rc.left = mx + PANEL_PAD_X - PX(10); r->rc.top = y;
        r->rc.right = mx + PANEL_WIDTH - PANEL_PAD_X + PX(10); r->rc.bottom = y + ROW_H;
        if (hovered) {
            FillRoundedRectAA(pixels, w, d->bufHeight, PX(6), PX(6), mx + PX(6), y,
                              PANEL_WIDTH - PX(12), ROW_H,
                              CR(theme->hover), CG(theme->hover), CB(theme->hover), theme->hoverAlpha);
            FillRoundedRectAA(pixels, w, d->bufHeight, 1, 1, mx + PX(6), y + PX(11), PX(2), ROW_H - PX(22),
                              CR(theme->accent), CG(theme->accent), CB(theme->accent), 255);
        }
    }

    // === 文字（形状全部画完后再统一绘制）===
    // GDI 会把写过的像素 alpha 置 0，绘制前后用快照把 alpha 还原
    SnapshotAlpha(pixels, d->alphaSnapshot, pixelCount);
    HDC hdc = d->hdcBuffer;

    DrawPanelText(hdc, d->hFontBig, timeStr, rcTime, theme->accent, DT_LEFT | DT_END_ELLIPSIS);
    RECT rcStatus = { mx + PANEL_PAD_X, my + PANEL_PAD_TOP, statusRight - dot * 2 - PX(8), my + PANEL_PAD_TOP + HEADER_H };
    DrawPanelText(hdc, d->hFontStatus, statusText, rcStatus, theme->text2, DT_RIGHT);

    for (int i = 0; i < d->regionCount; i++) {
        PanelRegion* r = &d->regions[i];
        BOOL hovered = (d->hoverIndex == i);
        switch (r->kind) {
            case REGION_BTN_PRIMARY:
                DrawPanelText(hdc, d->hFontBtn, g_timerState.isRunning ? texts->pause : texts->start, r->rc, theme->onAccent, DT_CENTER);
                break;
            case REGION_BTN_SECONDARY:
                DrawPanelText(hdc, d->hFontBtn, texts->reset, r->rc, theme->text, DT_CENTER);
                break;
            case REGION_CHIP: {
                DrawPanelText(hdc, d->hFontChip, r->label, r->rc, r->selected ? theme->onAccent : theme->text,
                               DT_CENTER | DT_END_ELLIPSIS);
                break;
            }
            case REGION_ROW_SWITCH: {
                RECT rcIcon = { mx + PX(12), r->rc.top, mx + PX(44), r->rc.bottom };
                RECT rcLabel = { mx + PX(46), r->rc.top, mx + PANEL_WIDTH - PANEL_PAD_X - SWITCH_WIDTH - PX(12), r->rc.bottom };
                DrawPanelText(hdc, d->hFontIcon, L"\uE916", rcIcon, hovered ? theme->accent : theme->text2, DT_CENTER);
                DrawPanelText(hdc, d->hFontLabel, texts->overtimeCount, rcLabel, theme->text, DT_LEFT);
                break;
            }
            case REGION_ROW_PLAIN: {
                RECT rcIcon = { mx + PX(12), r->rc.top, mx + PX(44), r->rc.bottom };
                RECT rcLabel = { mx + PX(46), r->rc.top, mx + PANEL_WIDTH - PANEL_PAD_X, r->rc.bottom };
                DrawPanelText(hdc, d->hFontIcon, L"\uE8A7", rcIcon, hovered ? theme->accent : theme->text2, DT_CENTER);
                DrawPanelText(hdc, d->hFontLabel, texts->panelShowWindow, rcLabel, theme->text, DT_LEFT);
                break;
            }
        }
    }
    RECT rcSec = rcSection;
    DrawPanelText(hdc, d->hFontSection, texts->panelQuickStart, rcSec, theme->text2, DT_LEFT);

    RestoreAlpha(pixels, d->alphaSnapshot, pixelCount);
#undef CR
#undef CG
#undef CB
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

// rcIcon / panelW / panelH 均为物理像素，且须在按监视器 DPI 感知的上下文中调用
// （这样 GetMonitorInfoW 返回的工作区也是物理像素）
static void ComputePopupPosition(const RECT* rcIcon, int panelW, int panelH, int* outX, int* outY) {
    POINT center = { (rcIcon->left + rcIcon->right) / 2, (rcIcon->top + rcIcon->bottom) / 2 };
    HMONITOR monitor = MonitorFromPoint(center, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfoW(monitor, &mi);

    int x = center.x - panelW / 2;
    if (x < mi.rcWork.left) x = mi.rcWork.left;
    if (x + panelW > mi.rcWork.right) x = mi.rcWork.right - panelW;

    int y = rcIcon->top - panelH - PX(8);   // 默认在图标（任务栏）上方
    if (y < mi.rcWork.top) y = rcIcon->bottom + PX(8);
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
            g_panelDpi = GetWindowDpi(hwnd);

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
            // 字号按"字符高度(em)"指定：原来的正数高度是单元格高度，实际字号会随字体内边距漂移
            d->hFontBig = CreateDisplayFont(PX(34));
            d->hFontStatus = CreateUiFont(PX(12), FW_NORMAL, FALSE);
            d->hFontLabel = CreateUiFont(PX(14), FW_NORMAL, FALSE);
            d->hFontBtn = CreateUiFont(PX(14), FW_SEMIBOLD, FALSE);
            d->hFontSection = CreateUiFont(PX(11), FW_NORMAL, FALSE);
            d->hFontChip = CreateUiFont(PX(13), FW_SEMIBOLD, FALSE);
            d->hFontIcon = CreateFontW(-PX(16), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                       0, 0, ANTIALIASED_QUALITY, 0, GetIconFontFamily());
            d->alphaSnapshot = (BYTE*)malloc((size_t)d->bufWidth * d->bufHeight);
            ReleaseDC(NULL, hdc);

            // 窗口此刻尚未显示：先捕获背后的屏幕做玻璃底图
            BuildPanelGlass(hwnd, d);
            UpdatePanelDisplay(hwnd, d);
            SetTimer(hwnd, 1, 30, NULL);
            return 0;
        }

        case WM_UI_THEME_CHANGED: {
            // 亮暗色切换：背景快照仍然有效，只需按新主题重建底图并重绘
            if (!d) return 0;
            BuildPanelGlass(hwnd, d);
            UpdatePanelDisplay(hwnd, d);
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
                DeleteObject(d->hFontBtn);
                DeleteObject(d->hFontSection);
                DeleteObject(d->hFontChip);
                DeleteObject(d->hFontIcon);
                free(d->alphaSnapshot);
                Backdrop_Release(&d->backdrop);
                GlassCache_Free(&d->glass);
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
    // 与 RenderPanel 的纵向推进保持一致（分隔线 1px + 上下留白），所有量都随 DPI 缩放
    return PANEL_PAD_TOP + HEADER_H + PX(12)
         + BTN_H + PX(12) + PX(13)
         + SECTION_H + PX(8)
         + chipRows * CHIP_H + (chipRows - 1) * CHIP_GAP + PX(12) + PX(13)
         + 2 * ROW_H
         + PANEL_PAD_BOTTOM;
}

void TrayPanel_Toggle(HWND owner) {
    if (g_panelHwnd && IsWindow(g_panelHwnd)) {
        DestroyWindow(g_panelHwnd);
        return;
    }

    // 主窗口是 DPI 无感知的：它拿到的托盘图标矩形是逻辑坐标。
    // 先在它的上下文里定位显示器，再进入 DPI 感知上下文把两个角点换算成物理坐标。
    RECT rcIcon;
    if (!GetTrayIconRect(g_timerState.hMainWnd, &rcIcon)) {
        // 拿不到图标矩形时退化为以光标为锚点
        POINT cp;
        GetCursorPos(&cp);
        rcIcon.left = cp.x - 2; rcIcon.right = cp.x + 2;
        rcIcon.top = cp.y - 2;  rcIcon.bottom = cp.y + 2;
    }
    POINT center = { (rcIcon.left + rcIcon.right) / 2, (rcIcon.top + rcIcon.bottom) / 2 };
    RECT logicalMon;
    HMONITOR hMonitor = FindLogicalMonitor(center, &logicalMon);

    // 面板窗口按"按监视器 DPI 感知"创建，避免在缩放的显示器上被 DWM 拉伸发虚；
    // 主窗口不受影响（只切换当前线程在创建期间的 DPI 上下文）
    HANDLE prevCtx = EnterPerMonitorDpiContext();
    g_panelDpi = QueryMonitorDpi(hMonitor);
    POINT tl = { rcIcon.left, rcIcon.top };
    POINT br = { rcIcon.right, rcIcon.bottom };
    if (MapLogicalPointToPhysical(hMonitor, &logicalMon, &tl) && MapLogicalPointToPhysical(hMonitor, &logicalMon, &br)) {
        rcIcon.left = tl.x; rcIcon.top = tl.y;
        rcIcon.right = br.x; rcIcon.bottom = br.y;
    }

    int contentH = ComputeContentHeight();
    int fullW = PANEL_WIDTH + PANEL_SHADOW * 2;
    int fullH = contentH + PANEL_SHADOW * 2;
    int x, y;
    ComputePopupPosition(&rcIcon, PANEL_WIDTH, contentH, &x, &y);

    HWND param = owner;
    g_panelHwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        PANEL_CLASS, L"", WS_POPUP,
        x - PANEL_SHADOW, y - PANEL_SHADOW, fullW, fullH,
        owner, NULL, GetModuleHandle(NULL), &param);

    LeaveDpiContext(prevCtx);

    if (!g_panelHwnd) return;

    ShowWindow(g_panelHwnd, SW_SHOW);
    SetCapture(g_panelHwnd);
}
