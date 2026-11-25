#include "timer_dialog_internal.h"
#include "timer_render_utils.h"
#include "timer_theme.h"
#include "timer_dialog_glass.h"

#define DLG_WIDTH  420
#define DLG_HEIGHT 410
#define DLG_SHADOW  30
#define DLG_RADIUS  12

static HWND g_hSetTimeDialog = NULL;
static HDC g_hdcBuffer = NULL;
static HBITMAP g_hbmBuffer = NULL;
static BYTE* g_pBits = NULL;
static DialogGlass g_glass;       // 玻璃底：背景快照 + 预渲染底图

static int g_tempHours, g_tempMinutes, g_tempSeconds;
static BOOL g_tempShowHours, g_tempShowMinutes, g_tempShowSeconds, g_tempShowMilliseconds;
static int g_origHours, g_origMinutes, g_origSeconds;
static BOOL g_origShowHours, g_origShowMinutes, g_origShowSeconds, g_origShowMilliseconds;

typedef enum {
    HIT_NONE, HIT_CARD_BG,
    HIT_HRS_MINUS, HIT_HRS_DISPLAY, HIT_HRS_PLUS,
    HIT_MIN_MINUS, HIT_MIN_DISPLAY, HIT_MIN_PLUS,
    HIT_SEC_MINUS, HIT_SEC_DISPLAY, HIT_SEC_PLUS,
    HIT_TOGGLE_HOUR, HIT_TOGGLE_MINUTE, HIT_TOGGLE_SECOND, HIT_TOGGLE_MILLISECOND,
    HIT_BTN_CONFIRM, HIT_BTN_CANCEL
} HitTestID;

typedef enum { EDIT_NONE, EDIT_HOURS, EDIT_MINUTES, EDIT_SECONDS } EditField;

static HitTestID g_hoverId = HIT_NONE;
static HitTestID g_pressedId = HIT_NONE;
static BOOL g_isDraggingDlg = FALSE;
static POINT g_dragStartScreen;
static RECT g_dlgStartRect;

/* Inline editing state */
static EditField g_editingField = EDIT_NONE;
static wchar_t g_editBuffer[5];   /* max "999" + null */
static int g_editCursorPos = 0;
static BOOL g_cursorVisible = FALSE;
static UINT_PTR g_cursorTimerId = 1;
#define CURSOR_BLINK_MS 530

static void RedrawDialog(void);
static void RenderDialogUI(void);

static void RedrawDialog(void) {
    HDC hdc = GetDC(NULL);
    POINT dst = {0, 0}, src = {0, 0};
    SIZE sz = {DLG_WIDTH, DLG_HEIGHT};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT rc; GetWindowRect(g_hSetTimeDialog, &rc);
    dst.x = rc.left; dst.y = rc.top;
    UpdateLayeredWindow(g_hSetTimeDialog, hdc, &dst, &sz, g_hdcBuffer, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, hdc);
}

static HitTestID HitTest(POINT pt) {
    int S = DLG_SHADOW;

    // Spinners
    RECT rHrsM = {S + 30, S + 107, S + 56, S + 133};
    RECT rHrsD = {S + 57, S + 108, S + 97, S + 133};
    RECT rHrsP = {S + 98, S + 107, S + 124, S + 133};
    RECT rMinM = {S + 145, S + 107, S + 171, S + 133};
    RECT rMinD = {S + 172, S + 108, S + 212, S + 133};
    RECT rMinP = {S + 213, S + 107, S + 239, S + 133};
    RECT rSecM = {S + 260, S + 107, S + 286, S + 133};
    RECT rSecD = {S + 287, S + 108, S + 327, S + 133};
    RECT rSecP = {S + 328, S + 107, S + 354, S + 133};

    // Toggles（命中区与视觉行一致：左列止于 S+190，右列起于 S+200，互不重叠且不超出卡片右缘 S+360）
    RECT rTogH = {S + 30, S + 180, S + 190, S + 210};
    RECT rTogM = {S + 200, S + 180, S + 360, S + 210};
    RECT rTogS = {S + 30, S + 222, S + 190, S + 252};
    RECT rTogMS = {S + 200, S + 222, S + 360, S + 252};

    // Buttons (centered, 14px above card bottom)
    RECT rBtnC = {S + 85, S + 300, S + 175, S + 336};
    RECT rBtnX = {S + 185, S + 300, S + 275, S + 336};

    if (PtInRect(&rHrsM, pt)) return HIT_HRS_MINUS;
    if (PtInRect(&rHrsD, pt)) return HIT_HRS_DISPLAY;
    if (PtInRect(&rHrsP, pt)) return HIT_HRS_PLUS;
    if (PtInRect(&rMinM, pt)) return HIT_MIN_MINUS;
    if (PtInRect(&rMinD, pt)) return HIT_MIN_DISPLAY;
    if (PtInRect(&rMinP, pt)) return HIT_MIN_PLUS;
    if (PtInRect(&rSecM, pt)) return HIT_SEC_MINUS;
    if (PtInRect(&rSecD, pt)) return HIT_SEC_DISPLAY;
    if (PtInRect(&rSecP, pt)) return HIT_SEC_PLUS;
    if (PtInRect(&rTogH, pt)) return HIT_TOGGLE_HOUR;
    if (PtInRect(&rTogM, pt)) return HIT_TOGGLE_MINUTE;
    if (PtInRect(&rTogS, pt)) return HIT_TOGGLE_SECOND;
    if (PtInRect(&rTogMS, pt)) return HIT_TOGGLE_MILLISECOND;
    if (PtInRect(&rBtnC, pt)) return HIT_BTN_CONFIRM;
    if (PtInRect(&rBtnX, pt)) return HIT_BTN_CANCEL;

    RECT rCard = {S, S, DLG_WIDTH - S, DLG_HEIGHT - S};
    if (PtInRect(&rCard, pt)) return HIT_CARD_BG;
    return HIT_NONE;
}

static BOOL CanToggleHour(BOOL h, BOOL m, BOOL s, BOOL ms) { return m; }
static BOOL CanToggleMinute(BOOL h, BOOL m, BOOL s, BOOL ms) { return !(ms && !s); }
static BOOL CanToggleMS(BOOL h, BOOL m, BOOL s, BOOL ms) { return s; }

/* --- Time value / display format conversion helpers --- */

static int GetMaxTotalSeconds(void) {
    if (g_tempShowHours) {
        return 99 * 3600 + 59 * 60 + 59;
    }
    if (g_tempShowMinutes) {
        return 999 * 60 + 59;
    }
    return 999;
}

static int TotalSecondsFromFields(void) {
    if (g_tempShowHours) {
        return g_tempHours * 3600 + g_tempMinutes * 60 + g_tempSeconds;
    }
    if (g_tempShowMinutes) {
        return g_tempMinutes * 60 + g_tempSeconds;
    }
    return g_tempSeconds;
}

static void FieldsFromTotalSeconds(int totalSec) {
    if (totalSec < 0) totalSec = 0;
    int maxTotal = GetMaxTotalSeconds();
    if (totalSec > maxTotal) totalSec = maxTotal;

    if (g_tempShowHours) {
        g_tempHours = totalSec / 3600;
        g_tempMinutes = (totalSec % 3600) / 60;
        g_tempSeconds = totalSec % 60;
    } else if (g_tempShowMinutes) {
        g_tempHours = 0;
        g_tempMinutes = totalSec / 60;
        g_tempSeconds = totalSec % 60;
    } else {
        g_tempHours = 0;
        g_tempMinutes = 0;
        g_tempSeconds = totalSec;
    }
}

/* --- Inline editing helpers --- */

static int GetEditFieldMax(EditField f) {
    switch (f) {
        case EDIT_HOURS:   return 99;
        case EDIT_MINUTES: return g_tempShowHours ? 59 : 999;
        case EDIT_SECONDS: return g_tempShowMinutes ? 59 : 999;
        default: return 99;
    }
}

static int* GetEditFieldPtr(EditField f) {
    switch (f) {
        case EDIT_HOURS:   return &g_tempHours;
        case EDIT_MINUTES: return &g_tempMinutes;
        case EDIT_SECONDS: return &g_tempSeconds;
        default: return NULL;
    }
}

static void StartEditing(EditField field) {
    if (field == EDIT_NONE) return;
    g_editingField = field;
    g_editBuffer[0] = L'\0';
    g_editCursorPos = 0;
    g_cursorVisible = TRUE;
    g_cursorTimerId = SetTimer(g_hSetTimeDialog, 1, CURSOR_BLINK_MS, NULL); // 保存实际计时器ID，避免置零后闪烁失效
    RenderDialogUI();
    RedrawDialog();
}

static void StopEditing(BOOL accept) {
    if (g_editingField == EDIT_NONE) return;
    if (g_cursorTimerId) {
        KillTimer(g_hSetTimeDialog, g_cursorTimerId);
        g_cursorTimerId = 0;
    }
    if (accept && wcslen(g_editBuffer) > 0) {
        int v = wcstol(g_editBuffer, NULL, 10);
        int max = GetEditFieldMax(g_editingField);
        if (v < 0) v = 0;
        if (v > max) v = max;
        int* ptr = GetEditFieldPtr(g_editingField);
        if (ptr) *ptr = v;
    }
    g_editingField = EDIT_NONE;
    g_editBuffer[0] = L'\0';
    g_editCursorPos = 0;
    g_cursorVisible = FALSE;
    RenderDialogUI();
    RedrawDialog();
}

static void HandleEditKey(WPARAM vk) {
    if (g_editingField == EDIT_NONE) return;
    int len = (int)wcslen(g_editBuffer);

    if (vk >= '0' && vk <= '9') {
        int maxDigits = (GetEditFieldMax(g_editingField) > 99) ? 3 : 2;
        if (len < maxDigits && g_editCursorPos < maxDigits) {
            /* Insert digit at cursor position */
            for (int i = len; i > g_editCursorPos; i--)
                g_editBuffer[i] = g_editBuffer[i - 1];
            g_editBuffer[g_editCursorPos] = (wchar_t)vk;
            g_editCursorPos++;
            g_editBuffer[g_editCursorPos] = L'\0';
            g_cursorVisible = TRUE;
        }
    } else if (vk == VK_BACK) {
        if (g_editCursorPos > 0 && len > 0) {
            for (int i = g_editCursorPos - 1; i < len - 1; i++)
                g_editBuffer[i] = g_editBuffer[i + 1];
            g_editCursorPos--;
            g_editBuffer[len - 1] = L'\0';
            g_cursorVisible = TRUE;
        } else if (len == 0) {
            /* Empty buffer: stop editing and discard */
            StopEditing(FALSE);
            return;
        }
    } else if (vk == VK_LEFT) {
        if (g_editCursorPos > 0) g_editCursorPos--;
        g_cursorVisible = TRUE;
    } else if (vk == VK_RIGHT) {
        if (g_editCursorPos < len) g_editCursorPos++;
        g_cursorVisible = TRUE;
    } else if (vk == VK_RETURN) {
        StopEditing(TRUE);
        return;
    } else if (vk == VK_ESCAPE) {
        StopEditing(FALSE);
        return;
    } else {
        return; /* Ignore other keys */
    }
    RenderDialogUI();
    RedrawDialog();
}

// 面板的文字在形状全部画完后再统一绘制（保留 ClearType）；GDI 会把写过的像素 alpha 置 0，
// 所以绘制前后用快照把 alpha 还原。
static BYTE* g_alphaSnap = NULL;

static void FillR(const RECT* rc, int radius, COLORREF c, BYTE a) {
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, radius, radius,
                      rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                      GetRValue(c), GetGValue(c), GetBValue(c), a);
}

static void OutlineR(const RECT* rc, int radius, int thick, COLORREF c, BYTE a) {
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, radius, radius,
                             rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                             thick, GetRValue(c), GetGValue(c), GetBValue(c), a);
}

static void RenderDialogUI(void) {
    memset(g_pBits, 0, DLG_WIDTH * DLG_HEIGHT * 4);

    const UiTheme* theme = Ui_GetTheme();
    const MenuTexts* texts = GetMenuTexts();
    int S = DLG_SHADOW;
    int cardX = S, cardY = S, cardW = DLG_WIDTH - 2 * S;
    float glow = theme->glow / 255.0f;

    // 面板：投影 + 玻璃底 + 描边（预先算好的底图，这里只拷贝）
    DialogGlass_Blit(&g_glass, g_pBits, DLG_WIDTH, DLG_HEIGHT);

    HFONT hFontTitle = CreateUiFont(19, FW_SEMIBOLD, TRUE);
    HFONT hFontSection = CreateUiFont(12, FW_NORMAL, FALSE);
    HFONT hFontToggle = CreateUiFont(13, FW_NORMAL, FALSE);
    HFONT hFontBtn = CreateUiFont(14, FW_SEMIBOLD, FALSE);
    HFONT hFontNum = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");

    // ================= 形状 =================

    // 三个步进器（减 / 数值 / 加）。几何位置必须与 HitTest 保持一致。
    struct {
        RECT minus, disp, plus;
        HitTestID hMinus, hDisp, hPlus;
        int value;
        EditField field;
        BOOL enabled;
    } sp[3] = {
        { { S + 30, S + 107, S + 56, S + 133 }, { S + 57, S + 108, S + 97, S + 133 }, { S + 98, S + 107, S + 124, S + 133 },
          HIT_HRS_MINUS, HIT_HRS_DISPLAY, HIT_HRS_PLUS, g_tempHours, EDIT_HOURS, g_tempShowHours },
        { { S + 145, S + 107, S + 171, S + 133 }, { S + 172, S + 108, S + 212, S + 133 }, { S + 213, S + 107, S + 239, S + 133 },
          HIT_MIN_MINUS, HIT_MIN_DISPLAY, HIT_MIN_PLUS, g_tempMinutes, EDIT_MINUTES, g_tempShowMinutes },
        { { S + 260, S + 107, S + 286, S + 133 }, { S + 287, S + 108, S + 327, S + 133 }, { S + 328, S + 107, S + 354, S + 133 },
          HIT_SEC_MINUS, HIT_SEC_DISPLAY, HIT_SEC_PLUS, g_tempSeconds, EDIT_SECONDS, g_tempShowSeconds },
    };
    for (int i = 0; i < 3; i++) {
        BOOL hoverM = (g_hoverId == sp[i].hMinus), pressM = (g_pressedId == sp[i].hMinus);
        BOOL hoverP = (g_hoverId == sp[i].hPlus), pressP = (g_pressedId == sp[i].hPlus);
        BOOL hoverD = (g_hoverId == sp[i].hDisp);
        BOOL editing = (g_editingField == sp[i].field);

        if (sp[i].enabled) {
            BYTE aM = pressM ? (BYTE)min(255, theme->ctrlAlpha + 55) : (hoverM ? (BYTE)min(255, theme->ctrlAlpha + 30) : theme->ctrlAlpha);
            BYTE aP = pressP ? (BYTE)min(255, theme->ctrlAlpha + 55) : (hoverP ? (BYTE)min(255, theme->ctrlAlpha + 30) : theme->ctrlAlpha);
            FillR(&sp[i].minus, 4, theme->ctrl, aM);
            OutlineR(&sp[i].minus, 4, 1, theme->ctrlLine, theme->ctrlLineAlpha);
            FillR(&sp[i].plus, 4, theme->ctrl, aP);
            OutlineR(&sp[i].plus, 4, 1, theme->ctrlLine, theme->ctrlLineAlpha);
        }

        // 数值框：编辑中用强调色 2px 边框，悬停用强调色 1px 边框，禁用时不描边
        FillR(&sp[i].disp, 4, theme->field, theme->fieldAlpha);
        if (editing) {
            OutlineR(&sp[i].disp, 4, 2, theme->accent, 255);
        } else if (sp[i].enabled) {
            OutlineR(&sp[i].disp, 4, 1, hoverD ? theme->accent : theme->ctrlLine, hoverD ? 220 : theme->ctrlLineAlpha);
        }
    }

    // 分隔线
    DrawHLine(g_pBits, DLG_WIDTH, cardX + 20, cardX + cardW - 20, cardY + 142,
              GetRValue(theme->hair), GetGValue(theme->hair), GetBValue(theme->hair), theme->hairAlpha, 1);

    // 显示格式开关（左列 / 右列，两行）。命中区：左列 S+30..S+190，右列 S+200..S+360。
    BOOL enH = CanToggleHour(g_tempShowHours, g_tempShowMinutes, g_tempShowSeconds, g_tempShowMilliseconds);
    BOOL enM = CanToggleMinute(g_tempShowHours, g_tempShowMinutes, g_tempShowSeconds, g_tempShowMilliseconds);
    BOOL enS = TRUE;
    BOOL enMS = CanToggleMS(g_tempShowHours, g_tempShowMinutes, g_tempShowSeconds, g_tempShowMilliseconds);
    const int swW = 38, swH = 22, swX1 = S + 30, swX2 = S + 200, swY1 = S + 180, swY2 = S + 222;
    struct { int x, y; BOOL on, enabled; const wchar_t* label; } tog[4] = {
        { swX1, swY1, g_tempShowHours, enH, texts->hours },
        { swX2, swY1, g_tempShowMinutes, enM, texts->minutes },
        { swX1, swY2, g_tempShowSeconds, enS, texts->seconds },
        { swX2, swY2, g_tempShowMilliseconds, enMS, texts->milliseconds },
    };
    for (int i = 0; i < 4; i++) {
        int swY = tog[i].y + (30 - swH) / 2;
        DrawSwitchThemed(g_pBits, DLG_WIDTH, DLG_HEIGHT, tog[i].x, swY, swH, tog[i].on, 96, theme);
        if (!tog[i].enabled) {
            // 不可用的开关整体压淡
            RECT rc = { tog[i].x, swY, tog[i].x + swW, swY + swH };
            FillR(&rc, swH / 2, theme->glass, 170);
        }
    }

    // 底部按钮：确定（强调色 + 辉光）/ 取消
    RECT rBtnC = { S + 85, S + 300, S + 175, S + 336 };
    RECT rBtnX = { S + 185, S + 300, S + 275, S + 336 };
    BOOL bCH = (g_hoverId == HIT_BTN_CONFIRM), bCP = (g_pressedId == HIT_BTN_CONFIRM);
    BOOL bXH = (g_hoverId == HIT_BTN_CANCEL), bXP = (g_pressedId == HIT_BTN_CANCEL);
    COLORREF fillC = bCP ? theme->accentPressed : (bCH ? theme->accentHover : theme->accentFill);
    GlowRoundedRect(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rBtnC.left, rBtnC.top,
                    rBtnC.right - rBtnC.left, rBtnC.bottom - rBtnC.top,
                    GetRValue(fillC), GetGValue(fillC), GetBValue(fillC), glow * 0.5f, 16);
    FillR(&rBtnC, 6, fillC, 255);
    FillR(&rBtnX, 6, theme->ctrl, bXH ? (BYTE)min(255, theme->ctrlAlpha + 30) : (bXP ? (BYTE)min(255, theme->ctrlAlpha + 55) : theme->ctrlAlpha));
    OutlineR(&rBtnX, 6, 1, theme->ctrlLine, theme->ctrlLineAlpha);

    // ================= 文字 =================
    size_t pixelCount = (size_t)DLG_WIDTH * DLG_HEIGHT;
    HDC hdc = g_hdcBuffer;
    SnapshotAlpha(g_pBits, g_alphaSnap, pixelCount);

    RECT rcTitle = { cardX + 20, cardY + 10, cardX + cardW - 20, cardY + 40 };
    DrawTextClearType(hdc, hFontTitle, texts->timeSettings, rcTitle, theme->text, DT_LEFT | DT_END_ELLIPSIS);

    RECT rcSec1 = { cardX + 20, cardY + 48, cardX + cardW - 20, cardY + 68 };
    DrawTextClearType(hdc, hFontSection, texts->setTimeTitle, rcSec1, theme->text2, DT_LEFT | DT_END_ELLIPSIS);

    RECT rcSec2 = { cardX + 20, cardY + 150, cardX + cardW - 20, cardY + 170 };
    DrawTextClearType(hdc, hFontSection, texts->formatTitle, rcSec2, theme->text2, DT_LEFT | DT_END_ELLIPSIS);

    const wchar_t* fieldLabels[3] = { texts->hours, texts->minutes, texts->seconds };
    for (int i = 0; i < 3; i++) {
        RECT rcL = { sp[i].minus.left, cardY + 72, sp[i].minus.left + 100, cardY + 90 };
        DrawTextClearType(hdc, hFontSection, fieldLabels[i], rcL, theme->text2, DT_LEFT | DT_END_ELLIPSIS);

        COLORREF btnText = sp[i].enabled ? theme->text2 : theme->dim;
        DrawTextClearType(hdc, hFontToggle, L"-", sp[i].minus, btnText, DT_CENTER);
        DrawTextClearType(hdc, hFontToggle, L"+", sp[i].plus, btnText, DT_CENTER);

        if (g_editingField == sp[i].field) {
            DrawTextClearType(hdc, hFontNum, g_editBuffer, sp[i].disp, theme->text, DT_CENTER);
            if (g_cursorVisible) {
                // 光标位置 = 文字整体居中后的起点 + 光标前那一段的宽度
                HFONT hOld = (HFONT)SelectObject(hdc, hFontNum);
                SIZE szPrefix, szFull;
                GetTextExtentPoint32W(hdc, g_editBuffer, g_editCursorPos, &szPrefix);
                GetTextExtentPoint32W(hdc, g_editBuffer, (int)wcslen(g_editBuffer), &szFull);
                SelectObject(hdc, hOld);
                int startX = (sp[i].disp.left + sp[i].disp.right - szFull.cx) / 2;
                int cursorX = startX + szPrefix.cx;
                RECT cursorRect = { cursorX, sp[i].disp.top + 3, cursorX + 2, sp[i].disp.bottom - 3 };
                FillR(&cursorRect, 1, theme->accent, 255);
            }
        } else {
            wchar_t buf[8];
            swprintf(buf, 8, L"%02d", sp[i].value);
            DrawTextClearType(hdc, hFontNum, buf, sp[i].disp, sp[i].enabled ? theme->text : theme->dim, DT_CENTER);
        }
    }

    for (int i = 0; i < 4; i++) {
        RECT rcT = { tog[i].x + swW + 12, tog[i].y, tog[i].x + 160, tog[i].y + 30 };
        DrawTextClearType(hdc, hFontToggle, tog[i].label, rcT, tog[i].enabled ? theme->text : theme->dim,
                          DT_LEFT | DT_END_ELLIPSIS);
    }

    DrawTextClearType(hdc, hFontBtn, texts->ok, rBtnC, theme->onAccent, DT_CENTER);
    DrawTextClearType(hdc, hFontBtn, texts->cancel, rBtnX, theme->text, DT_CENTER);

    RestoreAlpha(g_pBits, g_alphaSnap, pixelCount);

    DeleteObject(hFontTitle);
    DeleteObject(hFontSection);
    DeleteObject(hFontToggle);
    DeleteObject(hFontBtn);
    DeleteObject(hFontNum);
}

static void ApplyAndSave(void) {
    // Apply time value
    SetTimerSeconds(TotalSecondsFromFields());
    // Apply format settings
    SetShowHours(g_tempShowHours);
    SetShowMinutes(g_tempShowMinutes);
    SetShowSeconds(g_tempShowSeconds);
    SetShowMilliseconds(g_tempShowMilliseconds);
    SaveFormatConfig();
    // Refresh main window
    InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
}

LRESULT CALLBACK SetTimeDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hSetTimeDialog = hwnd;
            HDC hdc = GetDC(NULL);
            g_hdcBuffer = CreateCompatibleDC(hdc);
            BITMAPINFOHEADER bi = {sizeof(bi), DLG_WIDTH, -DLG_HEIGHT, 1, 32, BI_RGB};
            g_hbmBuffer = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&g_pBits, NULL, 0);
            SelectObject(g_hdcBuffer, g_hbmBuffer);
            ReleaseDC(NULL, hdc);

            // Initialize temp values from current state
            int totalSec = g_timerState.seconds;
            g_tempShowHours = g_timerState.showHours;
            g_tempShowMinutes = g_timerState.showMinutes;
            g_tempShowSeconds = g_timerState.showSeconds;
            g_tempShowMilliseconds = g_timerState.showMilliseconds;
            FieldsFromTotalSeconds(totalSec);
            g_origHours = g_tempHours; g_origMinutes = g_tempMinutes; g_origSeconds = g_tempSeconds;
            g_origShowHours = g_tempShowHours; g_origShowMinutes = g_tempShowMinutes;
            g_origShowSeconds = g_tempShowSeconds; g_origShowMilliseconds = g_tempShowMilliseconds;

            // 窗口此刻尚未显示：捕获背后的屏幕做玻璃底图
            g_alphaSnap = (BYTE*)malloc((size_t)DLG_WIDTH * DLG_HEIGHT);
            DialogGlass_Init(&g_glass, hwnd, DLG_WIDTH, DLG_HEIGHT, DLG_SHADOW, DLG_RADIUS);

            RenderDialogUI();
            RedrawDialog();
            return 0;
        }

        case WM_UI_THEME_CHANGED: {
            // 亮暗色切换：背景快照仍然有效，按新主题重建底图并重绘
            DialogGlass_Rebuild(&g_glass, hwnd);
            RenderDialogUI();
            RedrawDialog();
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            if (g_isDraggingDlg) {
                POINT cur; GetCursorPos(&cur);
                int dx = cur.x - g_dragStartScreen.x;
                int dy = cur.y - g_dragStartScreen.y;
                SetWindowPos(hwnd, NULL, g_dlgStartRect.left + dx, g_dlgStartRect.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            /* Disable hover tracking while editing */
            if (g_editingField != EDIT_NONE) return 0;
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            HitTestID hit = HitTest(pt);
            if (hit != g_hoverId) {
                g_hoverId = hit;
                RenderDialogUI();
                RedrawDialog();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);
            g_pressedId = hit;
            /* If currently editing, confirm value before processing new click */
            if (g_editingField != EDIT_NONE) {
                StopEditing(TRUE);
            }
            if (hit == HIT_CARD_BG) {
                g_isDraggingDlg = TRUE;
                GetCursorPos(&g_dragStartScreen);
                GetWindowRect(hwnd, &g_dlgStartRect);
                SetCapture(hwnd);
            }
            RenderDialogUI();
            RedrawDialog();
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_isDraggingDlg) {
                g_isDraggingDlg = FALSE;
                ReleaseCapture();
                DialogGlass_Rebuild(&g_glass, hwnd);   // 拖动结束：玻璃底对齐到新位置背后的画面
            }

            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);
            if (hit == g_pressedId && hit != HIT_NONE && hit != HIT_CARD_BG) {
                BOOL h = g_tempShowHours, m = g_tempShowMinutes, s = g_tempShowSeconds, ms = g_tempShowMilliseconds;

                switch (hit) {
                    case HIT_HRS_MINUS:
                        if (g_tempShowHours && g_tempHours > 0) g_tempHours--;
                        break;
                    case HIT_HRS_PLUS:
                        if (g_tempShowHours && g_tempHours < 99) g_tempHours++;
                        break;
                    case HIT_MIN_MINUS:
                        if (g_tempShowMinutes && g_tempMinutes > 0) g_tempMinutes--;
                        break;
                    case HIT_MIN_PLUS:
                        if (g_tempShowMinutes && g_tempMinutes < GetEditFieldMax(EDIT_MINUTES)) g_tempMinutes++;
                        break;
                    case HIT_SEC_MINUS:
                        if (g_tempShowSeconds && g_tempSeconds > 0) g_tempSeconds--;
                        break;
                    case HIT_SEC_PLUS:
                        if (g_tempShowSeconds && g_tempSeconds < GetEditFieldMax(EDIT_SECONDS)) g_tempSeconds++;
                        break;

                    case HIT_HRS_DISPLAY:
                        if (g_tempShowHours) StartEditing(EDIT_HOURS);
                        break;
                    case HIT_MIN_DISPLAY:
                        if (g_tempShowMinutes) StartEditing(EDIT_MINUTES);
                        break;
                    case HIT_SEC_DISPLAY:
                        if (g_tempShowSeconds) StartEditing(EDIT_SECONDS);
                        break;

                    case HIT_TOGGLE_HOUR:
                        if (CanToggleHour(!h, m, s, ms)) {
                            int total = TotalSecondsFromFields();
                            g_tempShowHours = !h;
                            FieldsFromTotalSeconds(total);
                        }
                        break;
                    case HIT_TOGGLE_MINUTE:
                        if (CanToggleMinute(h, !m, s, ms)) {
                            int total = TotalSecondsFromFields();
                            g_tempShowMinutes = !m;
                            if (!g_tempShowMinutes) g_tempShowHours = FALSE;
                            FieldsFromTotalSeconds(total);
                        }
                        break;
                    case HIT_TOGGLE_SECOND:
                        {
                            int total = TotalSecondsFromFields();
                            g_tempShowSeconds = !s;
                            FieldsFromTotalSeconds(total);
                        }
                        break;
                    case HIT_TOGGLE_MILLISECOND:
                        if (CanToggleMS(h, m, s, !ms)) {
                            g_tempShowMilliseconds = !ms;
                        }
                        break;

                    case HIT_BTN_CONFIRM:
                        ApplyAndSave();
                        DestroyWindow(hwnd);
                        return 0;
                    case HIT_BTN_CANCEL:
                        DestroyWindow(hwnd);
                        return 0;
                }
            }
            g_pressedId = HIT_NONE;
            RenderDialogUI();
            RedrawDialog();
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hoverId != HIT_NONE) {
                g_hoverId = HIT_NONE;
                RenderDialogUI();
                RedrawDialog();
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (g_editingField != EDIT_NONE) {
                HandleEditKey(wParam);
                return 0;
            }
            switch (wParam) {
                case VK_ESCAPE:
                    DestroyWindow(hwnd);
                    return 0;
                case VK_RETURN:
                    ApplyAndSave();
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == g_cursorTimerId && g_editingField != EDIT_NONE) {
                g_cursorVisible = !g_cursorVisible;
                RenderDialogUI();
                RedrawDialog();
                return 0;
            }
            break;
        }

        case WM_KILLFOCUS: {
            if (g_editingField != EDIT_NONE) {
                StopEditing(TRUE);
            }
            break;
        }

        case WM_DESTROY: {
            if (g_cursorTimerId) KillTimer(hwnd, g_cursorTimerId);
            if (g_hbmBuffer) DeleteObject(g_hbmBuffer);
            if (g_hdcBuffer) DeleteDC(g_hdcBuffer);
            free(g_alphaSnap);
            g_alphaSnap = NULL;
            DialogGlass_Destroy(&g_glass);
            g_hSetTimeDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND GetSetTimeDialog(void) {
    return g_hSetTimeDialog;
}

void ShowSetTimeDialog(void) {
    if (g_hSetTimeDialog && IsWindow(g_hSetTimeDialog)) {
        SetForegroundWindow(g_hSetTimeDialog);
        return;
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = SetTimeDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ModernSetTimeDialogClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    RECT parentRect; GetWindowRect(g_timerState.hMainWnd, &parentRect);
    int parentWidth = parentRect.right - parentRect.left;
    int parentHeight = parentRect.bottom - parentRect.top;

    int x = parentRect.right + 20;
    int y = parentRect.top + (parentHeight - DLG_HEIGHT) / 2;

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    if (x + DLG_WIDTH > workArea.right) {
        x = parentRect.left - DLG_WIDTH - 20;
    }

    if (x < workArea.left) x = workArea.left;
    if (y < workArea.top) y = workArea.top;
    if (x + DLG_WIDTH > workArea.right) x = workArea.right - DLG_WIDTH;
    if (y + DLG_HEIGHT > workArea.bottom) y = workArea.bottom - DLG_HEIGHT;

    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST, L"ModernSetTimeDialogClass", L"", WS_POPUP,
        x, y, DLG_WIDTH, DLG_HEIGHT, g_timerState.hMainWnd, NULL, GetModuleHandle(NULL), NULL);
    EnableWindow(g_timerState.hMainWnd, FALSE);
    ShowWindow(hwnd, SW_SHOW);
}

void CreateSetTimeDialog(void) {
    ShowSetTimeDialog();
}
