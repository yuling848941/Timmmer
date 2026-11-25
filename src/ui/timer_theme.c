/*
 * 统一主题（设计令牌）
 *
 * 菜单、托盘面板、各设置面板和计时器主窗口共用这一套令牌。
 * 风格定稿见 .zcode/ui-review/styles/style_A_dark_glass_hud.png 与亮色版样张：
 *   暗色 = 深色半透明玻璃 + 霓虹强调色 + 辉光
 *   亮色 = 磨砂白玻璃 + 深色强调色 + 投影
 * 两套主题的几何（圆角、间距、组件形状）完全一致，只有颜色和效果强度不同。
 */
#include "timer_theme.h"
#include "timer_core.h"
#include <math.h>

// 暗色：玻璃 (13,19,28) @74%，描边白 11%，投影 46%，辉光 60%
static const UiTheme THEME_DARK = {
    TRUE,
    RGB(34, 211, 238), RGB(34, 211, 238), RGB(74, 226, 247), RGB(14, 185, 214), RGB(6, 20, 26),
    RGB(230, 237, 243), RGB(143, 161, 184), RGB(86, 99, 122), RGB(244, 63, 94),
    RGB(16, 23, 33), RGB(28, 37, 50), RGB(9, 14, 21), RGB(60, 72, 90), RGB(38, 50, 66), RGB(30, 40, 54),
    RGB(13, 19, 28), 189, RGB(255, 255, 255), 28, 117, 51,
    RGB(255, 255, 255), 15, RGB(255, 255, 255), 15, RGB(255, 255, 255), 36, RGB(0, 0, 0), 56,
    RGB(255, 255, 255), 23, RGB(205, 214, 224),
    153
};

// 亮色：玻璃 (247,250,253) @80%，描边墨色 10%，投影 15%，辉光 30%
static const UiTheme THEME_LIGHT = {
    FALSE,
    RGB(14, 116, 144), RGB(6, 182, 212), RGB(34, 199, 226), RGB(4, 160, 188), RGB(4, 47, 75),
    RGB(15, 23, 42), RGB(71, 85, 105), RGB(130, 143, 160), RGB(225, 29, 72),
    RGB(244, 248, 252), RGB(255, 255, 255), RGB(255, 255, 255), RGB(203, 213, 225), RGB(228, 236, 244), RGB(214, 224, 234),
    RGB(247, 250, 253), 204, RGB(15, 23, 42), 26, 38, 191,
    RGB(15, 23, 42), 11, RGB(255, 255, 255), 158, RGB(15, 23, 42), 26, RGB(255, 255, 255), 168,
    RGB(15, 23, 42), 20, RGB(255, 255, 255),
    77
};

static UiTheme g_theme;
static BOOL g_initialized = FALSE;

static float Clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void HsvFromColor(COLORREF c, float* h, float* s, float* v) {
    float r = GetRValue(c) / 255.0f, g = GetGValue(c) / 255.0f, b = GetBValue(c) / 255.0f;
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
    *v = mx;
    *s = mx <= 0.0f ? 0.0f : d / mx;
    if (d <= 0.0001f) { *h = 0.0f; return; }
    if (mx == r) *h = 60.0f * fmodf((g - b) / d, 6.0f);
    else if (mx == g) *h = 60.0f * ((b - r) / d + 2.0f);
    else *h = 60.0f * ((r - g) / d + 4.0f);
    if (*h < 0.0f) *h += 360.0f;
}

static COLORREF ColorFromHsv(float h, float s, float v) {
    float c = v * s, hp = fmodf(h, 360.0f) / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f)), m = v - c, r = 0, g = 0, b = 0;
    if (hp < 1) { r = c; g = x; }
    else if (hp < 2) { r = x; g = c; }
    else if (hp < 3) { g = c; b = x; }
    else if (hp < 4) { g = x; b = c; }
    else if (hp < 5) { r = x; b = c; }
    else { r = c; b = x; }
    return RGB((BYTE)((r + m) * 255.0f + 0.5f), (BYTE)((g + m) * 255.0f + 0.5f), (BYTE)((b + m) * 255.0f + 0.5f));
}

// 强调色跟随计时器字色：菜单与面板因此总是和计时器协调。
// 字色是黑/白/灰这类无彩色时无法直接当强调色用，沿用默认青色。
// 计时器处于超时正计时时字色会临时变红，取用户原本设置的颜色来判断。
static void ApplyAccent(UiTheme* t) {
    COLORREF src = g_timerState.isInOvertimeMode ? g_timerState.originalFontColor : g_timerState.fontColor;
    float h, s, v;
    HsvFromColor(src, &h, &s, &v);
    if (s < 0.30f || v < 0.15f) return;

    if (t->isDark) {
        t->accent = t->accentFill = ColorFromHsv(h, Clampf(s, 0.70f, 0.95f), 0.93f);
        t->accentHover = ColorFromHsv(h, 0.85f, 1.00f);
        t->accentPressed = ColorFromHsv(h, 0.90f, 0.80f);
        t->onAccent = ColorFromHsv(h, 0.55f, 0.16f);
    } else {
        t->accent = ColorFromHsv(h, Clampf(s, 0.75f, 0.95f), 0.46f);
        t->accentFill = ColorFromHsv(h, 0.85f, 0.72f);
        t->accentHover = ColorFromHsv(h, 0.85f, 0.62f);
        t->accentPressed = ColorFromHsv(h, 0.85f, 0.80f);
        t->onAccent = ColorFromHsv(h, 0.60f, 0.18f);
    }
}

static BOOL CALLBACK NotifyThemeChanged(HWND hwnd, LPARAM lp) {
    (void)lp;
    SendMessageW(hwnd, WM_UI_THEME_CHANGED, 0, 0);
    return TRUE;
}

static void NotifyAll(void) {
    EnumThreadWindows(GetCurrentThreadId(), NotifyThemeChanged, 0);
}

const UiTheme* Ui_GetTheme(void) {
    if (!g_initialized) {
        g_theme = THEME_DARK;
        ApplyAccent(&g_theme);
        g_initialized = TRUE;
    }
    return &g_theme;
}

BOOL Ui_IsDark(void) {
    return Ui_GetTheme()->isDark;
}

void Ui_SetDark(BOOL dark) {
    BOOL wasInit = g_initialized;
    BOOL changed = !g_initialized || (g_theme.isDark != !!dark);
    g_theme = dark ? THEME_DARK : THEME_LIGHT;
    ApplyAccent(&g_theme);
    g_initialized = TRUE;
    // 启动阶段（尚无窗口）不必通知
    if (wasInit && changed) NotifyAll();
}

void Ui_RefreshAccent(void) {
    UiTheme before = g_theme;
    Ui_GetTheme();
    ApplyAccent(&g_theme);
    if (before.accent != g_theme.accent || before.accentFill != g_theme.accentFill) NotifyAll();
}
