/*
 * iOS Style Context Menu for Win32
 * 完整实现：抗锯齿圆角、阴影、MDL2 图标、Switch 控件
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
// 以下均为 96 DPI 基准值，运行时经 PX() 按窗口所在显示器 DPI 缩放
// ===========================================

#define MENU_WIDTH              250
#define MENU_ITEM_HEIGHT        32
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
#define MENU_LABEL_EM           14

#define SWITCH_WIDTH            34

#define MENU_ANIM_MS            160
#define MENU_ANIM_SLIDE         4

#define COLOR_TEXT_PRIMARY      RGB(32, 32, 32)
#define COLOR_TEXT_SECONDARY    RGB(132, 132, 132)
#define COLOR_ICON              RGB(104, 104, 104)
#define COLOR_ACCENT            RGB(0, 95, 184)

static UINT g_menuDpi = 96;
#define PX(v) ScalePx((v), g_menuDpi)

// ===========================================
// 全局变量与数据结构
// ===========================================

static const wchar_t* g_menuClassName = L"IosMenuClass";
static BOOL g_classRegistered = FALSE;
static HWND g_currentMenuHwnd = NULL;

typedef struct {
    IosMenuItem* items;      // 自有的可变副本：开关状态可原地更新，不依赖调用方数组
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
    BYTE* alphaSnapshot;     // GDI 绘制文字前的 alpha 通道快照（见 RestoreAlphaChannel）
    HFONT hFontLabel;
    HFONT hFontIcon;
    BOOL isDraggingOwner;
    POINT dragStartScreen;
    RECT ownerStartRect;
    POINT finalPos;          // 窗口的最终屏幕位置：出场动画只改合成位置，不能让位移逐帧累加
    DWORD animStart;
    BOOL keyNav;             // 方向键导航中：忽略鼠标轮询，直到鼠标真正移动
    int topIndex;            // 滚动：首个可见项的绝对索引
    int visibleCount;        // 可见行数（菜单过高时封顶）
    int maxTopIndex;         // 滚动上限 = itemCount - visibleCount
} MenuData;

// ===========================================
// 绘图引擎（抗锯齿）
// 核心函数（含 Win11 开关）在 timer_render_utils.c
// ===========================================

// ===========================================
// 菜单项内容绘制
// ===========================================

static void DrawItemContent(MenuData* data, int idx, int mx, int y) {
    const IosMenuItem* it = &data->items[idx];
    HDC hdc = data->hdcBuffer;
    int itemH = PX(MENU_ITEM_HEIGHT);
    int ix = mx + PX(MENU_PADDING_LEFT);
    int tx = mx + PX(MENU_TEXT_OFFSET);
    int rightEdge = mx + PX(MENU_WIDTH) - PX(MENU_PADDING_RIGHT);
    if (it->type == IOS_MENU_ITEM_SWITCH) rightEdge -= PX(SWITCH_WIDTH) + PX(12);

    if (it->iconChar && wcslen(it->iconChar) == 1) {
        RECT rc = { ix, y, ix + PX(MENU_ICON_BOX_W), y + itemH };
        DrawTextClearType(hdc, data->hFontIcon, it->iconChar, rc, COLOR_ICON, DT_CENTER);
    }

    RECT rcText = { tx, y, rightEdge, y + itemH };
    if (it->subLabel && it->subLabel[0]) {
        // 辅助文本靠右对齐，主标签让出其宽度
        int subW = MeasureTextWidth(hdc, data->hFontLabel, it->subLabel);
        RECT rcSub = { rightEdge - subW - PX(8), y, rightEdge, y + itemH };
        DrawTextClearType(hdc, data->hFontLabel, it->subLabel, rcSub, COLOR_TEXT_SECONDARY, DT_RIGHT);
        rcText.right = rcSub.left - PX(8);
    }
    DrawTextClearType(hdc, data->hFontLabel, it->label, rcText, COLOR_TEXT_PRIMARY, DT_LEFT | DT_END_ELLIPSIS);
}

// ===========================================
// 渲染主逻辑
// ===========================================

static void RenderMenuContent(MenuData* data) {
    BYTE* pixels = (BYTE*)data->pBits;
    int w = data->bufWidth, h = data->bufHeight, mh = data->menuHeight;
    size_t pixelCount = (size_t)w * h;

    memset(pixels, 0, pixelCount * 4);

    int mx = PX(MENU_SHADOW_SIZE);
    int my = PX(MENU_SHADOW_SIZE);
    int mw = PX(MENU_WIDTH);
    int itemH = PX(MENU_ITEM_HEIGHT);
    int hair = max(1, PX(1));

    // 绘制全局真软阴影
    DrawSoftShadowSDF(pixels, w, h, mx, my, mw, mh, PX(MENU_RADIUS), PX(MENU_SHADOW_SIZE), PX(MENU_SHADOW_OFFSET_Y), 0.18f);

    // 1px 描边（画在主体外侧一圈，随后被主体盖住内半部分）
    FillRoundedRectAA(pixels, w, h, PX(MENU_RADIUS), PX(MENU_RADIUS), mx - hair, my - hair, mw + 2 * hair, mh + 2 * hair, 0, 0, 0, 26);

    // 菜单主体：不透明。此前为 245/255，背景里的深色内容会隐约透出，在花哨的桌面上像"脏影"
    FillRoundedRectAA(pixels, w, h, PX(MENU_RADIUS), PX(MENU_RADIUS), mx, my, mw, mh, 252, 252, 252, 255);

    int y = my + PX(MENU_PADDING_TOP);
    int lastVisible = data->topIndex + data->visibleCount;
    if (lastVisible > data->itemCount) lastVisible = data->itemCount;
    for (int i = data->topIndex; i < lastVisible; i++) {
        const IosMenuItem* it = &data->items[i];
        if (i == data->hoverIndex) {
            FillRoundedRectAA(pixels, w, h, PX(6), PX(6), mx + PX(6), y + PX(2), mw - PX(12), itemH - PX(4), 0, 0, 0, 20);
        }
        if (it->type == IOS_MENU_ITEM_SWITCH) {
            DrawSwitchWin11Scaled(pixels, w, h, mx + mw - PX(SWITCH_WIDTH) - PX(MENU_PADDING_RIGHT), y, itemH, it->isSwitchOn, g_menuDpi);
        }
        if (it->hasDividerAfter && i < data->itemCount - 1) {
            DrawHairlineH(pixels, w, mx + PX(MENU_TEXT_OFFSET), mx + mw - PX(MENU_PADDING_RIGHT), y + itemH, 0.09f, hair);
        }
        y += itemH;
    }

    // 滚动条指示（仅当内容超出可见行数时）
    if (data->maxTopIndex > 0) {
        int trackX = mx + mw - PX(5);
        int trackTop = my + PX(MENU_PADDING_TOP);
        int trackH = data->visibleCount * itemH;
        FillRoundedRectAA(pixels, w, h, PX(2), PX(2), trackX, trackTop, PX(3), trackH, 0, 0, 0, 18);
        int thumbH = max(PX(20), trackH * data->visibleCount / data->itemCount);
        int thumbTravel = trackH - thumbH;
        int thumbY = trackTop + (data->maxTopIndex > 0 ? thumbTravel * data->topIndex / data->maxTopIndex : 0);
        FillRoundedRectAA(pixels, w, h, PX(2), PX(2), trackX, thumbY, PX(3), thumbH, 0, 0, 0, 90);
    }

    // 文字与图标：主体内部已不透明，直接用 GDI 绘制以保留 ClearType；
    // GDI 会把写过的像素 alpha 置 0，绘制前后用快照把 alpha 还原
    y = my + PX(MENU_PADDING_TOP);
    SnapshotAlpha(pixels, data->alphaSnapshot, pixelCount);
    for (int i = data->topIndex; i < lastVisible; i++) {
        DrawItemContent(data, i, mx, y);
        y += itemH;
    }
    RestoreAlpha(pixels, data->alphaSnapshot, pixelCount);
}

static void PresentMenu(HWND hwnd, MenuData* d, BYTE alpha, int slideDy) {
    HDC hdc = GetDC(NULL);
    POINT dst = { d->finalPos.x, d->finalPos.y + slideDy };
    POINT src = { 0, 0 };
    SIZE sz = { d->bufWidth, d->bufHeight };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, hdc, &dst, &sz, d->hdcBuffer, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, hdc);
}

static void UpdateMenuDisplay(HWND hwnd, MenuData* data) {
    RenderMenuContent(data);
    PresentMenu(hwnd, data, 255, 0);
}

// 激活一个菜单项。返回 TRUE 表示菜单应保持打开（开关项原地切换），
// FALSE 表示应关闭（普通项，或命令处理过程中窗口已被销毁）。
static BOOL ActivateItem(HWND hwnd, MenuData* d, int idx) {
    if (!d || idx < 0 || idx >= d->itemCount) return FALSE;
    IosMenuItem* it = &d->items[idx];
    BOOL keepOpen = (it->type == IOS_MENU_ITEM_SWITCH);
    if (it->commandId != 0 && d->owner && IsWindow(d->owner)) {
        SendMessage(d->owner, WM_COMMAND, MAKEWPARAM(it->commandId, 0), 0);
    }
    // 命令处理可能打开模态对话框或销毁本窗口，先确认存活再继续
    if (!IsWindow(hwnd)) return FALSE;
    if (keepOpen) {
        // 开关项：不关闭、不释放捕获，原地刷新以反映 owner 发布的真实状态
        UpdateMenuDisplay(hwnd, d);
    }
    return keepOpen;
}

// 由 owner 在业务逻辑执行后调用，把开关的真实状态发布回当前存活的菜单。
// 以真实状态为准（而非盲目取反），命令被拒绝时显示也不会错。
void IosMenu_SetSwitchState(UINT commandId, BOOL isOn) {
    HWND hwnd = g_currentMenuHwnd;
    if (!hwnd || !IsWindow(hwnd)) return;
    MenuData* d = (MenuData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!d || !d->items) return;
    for (int i = 0; i < d->itemCount; i++) {
        if (d->items[i].type == IOS_MENU_ITEM_SWITCH && d->items[i].commandId == commandId) {
            if (d->items[i].isSwitchOn != isOn) {
                d->items[i].isSwitchOn = isOn;
                UpdateMenuDisplay(hwnd, d);
            }
            return;
        }
    }
}

static void SetHoverItem(HWND hwnd, MenuData* d, int idx) {
    if (idx == d->hoverIndex) return;
    d->prevHoverIndex = d->hoverIndex;
    d->hoverIndex = idx;
    UpdateMenuDisplay(hwnd, d);
    // 向 owner 发送悬停预览命令（commandId + 10000 区分正式选择）
    if (idx >= 0 && idx < d->itemCount && d->items[idx].commandId != 0 && d->owner) {
        SendMessage(d->owner, WM_COMMAND, MAKEWPARAM(d->items[idx].commandId + 10000, 0), 0);
    }
}

static int HitTestItem(const MenuData* d, POINT pt) {
    int mx = PX(MENU_SHADOW_SIZE), my = PX(MENU_SHADOW_SIZE), mw = PX(MENU_WIDTH);
    if (pt.x < mx || pt.x >= mx + mw) return -1;
    int top = my + PX(MENU_PADDING_TOP);
    int bodyH = d->visibleCount * PX(MENU_ITEM_HEIGHT);
    if (pt.y < top || pt.y >= top + bodyH) return -1;
    int row = (pt.y - top) / PX(MENU_ITEM_HEIGHT);
    int idx = d->topIndex + row;
    return idx < d->itemCount ? idx : -1;
}

// 把滚动位置钳制到合法范围
static void ClampScroll(MenuData* d) {
    if (d->topIndex < 0) d->topIndex = 0;
    if (d->topIndex > d->maxTopIndex) d->topIndex = d->maxTopIndex;
}

// 确保某个绝对索引可见（键盘导航时用）
static void EnsureVisible(MenuData* d, int idx) {
    if (idx < d->topIndex) d->topIndex = idx;
    else if (idx >= d->topIndex + d->visibleCount) d->topIndex = idx - d->visibleCount + 1;
    ClampScroll(d);
}

static LRESULT CALLBACK MenuProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MenuData* d = (MenuData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp; d = (MenuData*)calloc(1, sizeof(MenuData)); SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)d);
            IosMenuCreateParams* p = (IosMenuCreateParams*)cs->lpCreateParams;
            d->itemCount = p->itemCount; d->owner = p->owner; d->menuHeight = p->menuHeight;
            d->items = (IosMenuItem*)calloc((size_t)p->itemCount, sizeof(IosMenuItem));
            if (d->items) memcpy(d->items, p->items, (size_t)p->itemCount * sizeof(IosMenuItem));
            // 可见行数由（可能已封顶的）菜单高度反推；超出部分靠滚动访问
            d->visibleCount = (d->menuHeight - PX(MENU_PADDING_TOP) - PX(MENU_PADDING_BOTTOM)) / PX(MENU_ITEM_HEIGHT);
            if (d->visibleCount > d->itemCount) d->visibleCount = d->itemCount;
            if (d->visibleCount < 1) d->visibleCount = 1;
            d->maxTopIndex = d->itemCount - d->visibleCount;
            if (d->maxTopIndex < 0) d->maxTopIndex = 0;
            d->topIndex = 0;
            RECT rc; GetClientRect(hwnd, &rc); d->bufWidth = rc.right; d->bufHeight = rc.bottom;
            HDC hdc = GetDC(NULL); d->hdcBuffer = CreateCompatibleDC(hdc);
            BITMAPINFOHEADER bi = {sizeof(bi), d->bufWidth, -d->bufHeight, 1, 32, BI_RGB};
            d->hbmBuffer = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &d->pBits, NULL, 0);
            SelectObject(d->hdcBuffer, d->hbmBuffer);
            d->hFontLabel = CreateUiFont(PX(MENU_LABEL_EM), FW_NORMAL, FALSE);
            d->hFontIcon = CreateFontW(-PX(MENU_ICON_SIZE), 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0, ANTIALIASED_QUALITY, 0, GetIconFontFamily());
            d->alphaSnapshot = (BYTE*)malloc((size_t)d->bufWidth * d->bufHeight);
            ReleaseDC(NULL, hdc);
            {
                RECT wr; GetWindowRect(hwnd, &wr);
                d->finalPos.x = wr.left; d->finalPos.y = wr.top;
            }
            d->animStart = GetTickCount();
            PresentMenu(hwnd, d, 0, 0);
            SetTimer(hwnd, 1, 30, NULL);
            SetTimer(hwnd, 2, 16, NULL);
            return 0;
        }
        case WM_TIMER: {
            if (!d) return 0;
            if (wp == 2) {
                // 出场：淡入 + 轻微上浮，内容缓冲不变，只改合成时的整体 alpha 与位置
                DWORD elapsed = GetTickCount() - d->animStart;
                float t = elapsed >= MENU_ANIM_MS ? 1.0f : (float)elapsed / (float)MENU_ANIM_MS;
                float k = 1.0f - t;
                float ease = 1.0f - k * k * k;
                PresentMenu(hwnd, d, (BYTE)(255 * ease + 0.5f), (int)((1.0f - ease) * PX(MENU_ANIM_SLIDE) + 0.5f));
                if (t >= 1.0f) KillTimer(hwnd, 2);
                return 0;
            }
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            if (!d->keyNav) SetHoverItem(hwnd, d, HitTestItem(d, pt));
            return 0;
        }
        case WM_KEYDOWN: {
            if (!d) break;
            int idx = d->hoverIndex;
            switch (wp) {
                case VK_ESCAPE:
                    DestroyWindow(hwnd); return 0;
                case VK_DOWN:
                case VK_UP: {
                    int dir = (wp == VK_DOWN) ? 1 : -1;
                    do {
                        idx = (idx < 0) ? (dir > 0 ? 0 : d->itemCount - 1)
                                        : (idx + dir + d->itemCount) % d->itemCount;
                    } while (idx != d->hoverIndex && d->items[idx].commandId == 0);
                    d->keyNav = TRUE;
                    EnsureVisible(d, idx);
                    SetHoverItem(hwnd, d, idx);
                    return 0;
                }
                case VK_RETURN:
                case VK_SPACE: {
                    BOOL keep = ActivateItem(hwnd, d, idx);
                    if (!keep && IsWindow(hwnd)) DestroyWindow(hwnd);
                    return 0;
                }
                default:
                    break;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (!d || d->maxTopIndex <= 0) return 0;
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            int rows = (delta > 0) ? -3 : 3;   // 向上滚减小 topIndex
            d->topIndex += rows;
            ClampScroll(d);
            UpdateMenuDisplay(hwnd, d);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            int i = HitTestItem(d, pt);
            if (i >= 0) {
                BOOL keep = ActivateItem(hwnd, d, i);
                if (!keep && IsWindow(hwnd)) DestroyWindow(hwnd);
                // 开关项保持打开，捕获不释放，可连续切换
            } else {
                GetCursorPos(&d->dragStartScreen);
                GetWindowRect(d->owner, &d->ownerStartRect);
                d->isDraggingOwner = TRUE;
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            d->keyNav = FALSE;   // 鼠标一动就交还给鼠标导航
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
            if (d) { KillTimer(hwnd, 1); KillTimer(hwnd, 2); DeleteObject(d->hbmBuffer); DeleteDC(d->hdcBuffer); DeleteObject(d->hFontLabel); DeleteObject(d->hFontIcon); free(d->alphaSnapshot); free(d->items); free(d); }
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

    // 调用方（主窗口）是 DPI 无感知的，传来的坐标是逻辑坐标：
    // 先在它的上下文里定位显示器，稍后进入 DPI 感知上下文再换算成物理坐标
    POINT anchor = { x, y };
    RECT logicalMon;
    HMONITOR hMonitor = FindLogicalMonitor(anchor, &logicalMon);

    // 菜单窗口按"按监视器 DPI 感知"创建：否则在 125%/150% 缩放的屏幕上，
    // DWM 会把 96 DPI 的位图整体拉伸，文字和 1px 线条全部发虚。
    // 主窗口保持原样不受影响（只切换当前线程在创建期间的 DPI 上下文）。
    HANDLE prevCtx = EnterPerMonitorDpiContext();

    g_menuDpi = QueryMonitorDpi(hMonitor);
    MapLogicalPointToPhysical(hMonitor, &logicalMon, &anchor);
    x = anchor.x;
    y = anchor.y;

    int h = PX(MENU_PADDING_TOP) + PX(MENU_PADDING_BOTTOM) + ic * PX(MENU_ITEM_HEIGHT);
    int menuW = PX(MENU_WIDTH);
    int shadow = PX(MENU_SHADOW_SIZE);

    // 获取当前点所在的显示器工作区，确保菜单主体不会超出屏幕
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(hMonitor, (LPMONITORINFO)&mi)) {
        // 高度封顶：内容（如 32 项字体列表）超出工作区时压缩可见行数，超出部分靠滚轮/方向键访问
        int availH = (mi.rcWork.bottom - mi.rcWork.top) - shadow * 2 - PX(8);
        int minH = PX(MENU_PADDING_TOP) + PX(MENU_PADDING_BOTTOM) + PX(MENU_ITEM_HEIGHT);
        if (availH < minH) availH = minH;
        if (h > availH) {
            int rows = (availH - PX(MENU_PADDING_TOP) - PX(MENU_PADDING_BOTTOM)) / PX(MENU_ITEM_HEIGHT);
            if (rows < 1) rows = 1;
            h = PX(MENU_PADDING_TOP) + PX(MENU_PADDING_BOTTOM) + rows * PX(MENU_ITEM_HEIGHT);
        }
        // 如果右侧超出屏幕，将菜单显示在点击位置的左侧
        if (x + menuW > mi.rcWork.right) {
            x -= menuW;
        }
        // 如果下方超出屏幕，将菜单显示在点击位置的上方
        if (y + h > mi.rcWork.bottom) {
            y -= h;
        }

        // 确保不超出工作区的左侧和上方边缘
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y < mi.rcWork.top) y = mi.rcWork.top;
    }

    IosMenuCreateParams p = {i, ic, o, h};
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, g_menuClassName, L"", WS_POPUP,
                                x - shadow, y - shadow, menuW + shadow * 2, h + shadow * 2, o, NULL, GetModuleHandle(NULL), &p);

    LeaveDpiContext(prevCtx);

    if (!hwnd) return NULL;
    g_currentMenuHwnd = hwnd;
    ShowWindow(hwnd, SW_SHOW);
    SetCapture(hwnd);
    // 接管键盘：Esc 关闭、上下键移动、回车执行
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    return hwnd;
}
