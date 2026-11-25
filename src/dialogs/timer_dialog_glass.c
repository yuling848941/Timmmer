#include "timer_dialog_glass.h"
#include "timer_theme.h"

// 快照比窗口四周各多出的范围（像素）：面板被拖出这个范围后，
// 玻璃底会沿用快照边缘的内容，直到下次重新抓屏。
#define GLASS_CAPTURE_MARGIN 400

void DialogGlass_Init(DialogGlass* g, HWND hwnd, int bufW, int bufH, int shadow, int radius) {
    memset(g, 0, sizeof(*g));
    g->bufW = bufW;
    g->bufH = bufH;
    g->shadow = shadow;
    g->radius = radius;

    RECT wr;
    GetWindowRect(hwnd, &wr);
    int x = wr.left - GLASS_CAPTURE_MARGIN, y = wr.top - GLASS_CAPTURE_MARGIN;
    int w = bufW + GLASS_CAPTURE_MARGIN * 2, h = bufH + GLASS_CAPTURE_MARGIN * 2;
    // 限制在窗口所在的显示器内，避免多屏时抓到别处、也避免无谓的大块内存
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(monitor, &mi)) {
        if (x < mi.rcMonitor.left) x = mi.rcMonitor.left;
        if (y < mi.rcMonitor.top) y = mi.rcMonitor.top;
        if (x + w > mi.rcMonitor.right) w = mi.rcMonitor.right - x;
        if (y + h > mi.rcMonitor.bottom) h = mi.rcMonitor.bottom - y;
    }
    // 窗口此刻尚未显示，快照里不会有面板自己
    Backdrop_Capture(&g->backdrop, x, y, w, h);
    DialogGlass_Rebuild(g, hwnd);
}

void DialogGlass_Destroy(DialogGlass* g) {
    Backdrop_Release(&g->backdrop);
    GlassCache_Free(&g->cache);
}

void DialogGlass_Rebuild(DialogGlass* g, HWND hwnd) {
    RECT wr;
    GetWindowRect(hwnd, &wr);
    GlassCache_Build(&g->cache, g->bufW, g->bufH,
                     g->shadow, g->shadow, g->bufW - g->shadow * 2, g->bufH - g->shadow * 2,
                     g->radius, g->shadow, 9,
                     &g->backdrop, wr.left, wr.top, Ui_GetTheme());
}

void DialogGlass_Blit(DialogGlass* g, BYTE* pixels, int bufW, int bufH) {
    GlassCache_Blit(&g->cache, pixels, bufW, bufH);
}
