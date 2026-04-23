#include "timer_dialog_internal.h"
#include "timer_render_utils.h"

#define DLG_WIDTH  420
#define DLG_HEIGHT 410
#define DLG_SHADOW  30

static HWND g_hSetTimeDialog = NULL;
static HDC g_hdcBuffer = NULL;
static HBITMAP g_hbmBuffer = NULL;
static BYTE* g_pBits = NULL;

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
static wchar_t g_editBuffer[5];   /* max "99" + null */
static int g_editCursorPos = 0;
static BOOL g_cursorVisible = FALSE;
static UINT_PTR g_cursorTimerId = 1;
#define CURSOR_BLINK_MS 530

static void DrawTextSDF(HDC hdc, const wchar_t* text, RECT* rc, int format, HFONT hFont, COLORREF color) {
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    HTHEME hTheme = OpenThemeData(NULL, L"WINDOW");
    DTTOPTS dttOpts = {sizeof(DTTOPTS)};
    dttOpts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;
    dttOpts.crText = color;
    DrawThemeTextEx(hTheme, hdc, 0, 0, text, -1, format, rc, &dttOpts);
    CloseThemeData(hTheme);
    SelectObject(hdc, hOld);
}

static void DrawToggleSwitch(BYTE* pixels, int bufW, int bufH, int x, int y, BOOL isOn, BOOL isEnabled) {
    int swW = 34, swH = 18;
    if (isOn) {
        FillRoundedRectAA(pixels, bufW, bufH, swH / 2, swH / 2, x, y, swW, swH, 74, 144, 217, 255);
    } else {
        BYTE cr = isEnabled ? 180 : 220;
        FillRoundedRectAA(pixels, bufW, bufH, swH / 2, swH / 2, x, y, swW, swH, cr, cr, isEnabled ? 185 : 225, 255);
    }
    int ts = swH - 4;
    int tx = isOn ? (x + swW - ts - 2) : (x + 2);
    FillRoundedRectAA(pixels, bufW, bufH, ts / 2, ts / 2, tx, y + 2, ts, ts, 255, 255, 255, 255);
}

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

    // Toggles
    RECT rTogH = {S + 30, S + 180, S + 205, S + 210};
    RECT rTogM = {S + 200, S + 180, S + 375, S + 210};
    RECT rTogS = {S + 30, S + 222, S + 205, S + 252};
    RECT rTogMS = {S + 200, S + 222, S + 375, S + 252};

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

/* --- Inline editing helpers --- */

static int GetEditFieldMax(EditField f) {
    switch (f) {
        case EDIT_HOURS:   return 99;
        case EDIT_MINUTES: return 59;
        case EDIT_SECONDS: return 59;
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
    SetTimer(g_hSetTimeDialog, g_cursorTimerId, CURSOR_BLINK_MS, NULL);
    RenderDialogUI();
    RedrawDialog();
}

static void StopEditing(BOOL accept) {
    if (g_editingField == EDIT_NONE) return;
    KillTimer(g_hSetTimeDialog, g_cursorTimerId);
    g_cursorTimerId = 0;
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
        if (len < 2 && g_editCursorPos < 2) {
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

static void RenderDialogUI(void) {
    memset(g_pBits, 0, DLG_WIDTH * DLG_HEIGHT * 4);

    int S = DLG_SHADOW;
    int cardX = S, cardY = S, cardW = DLG_WIDTH - 2 * S, cardH = DLG_HEIGHT - 2 * S;

    // Shadow + Card
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT, cardX, cardY, cardW, cardH, 12, DLG_SHADOW, 4, 0.15f);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 12, 12, cardX, cardY, cardW, cardH, 250, 250, 252, 255);

    const MenuTexts* texts = GetMenuTexts();

    // Fonts
    HFONT hFontTitle = CreateFontW(-20, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontSection = CreateFontW(-14, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontLabel = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontBtn = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontNum = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");

    // Title
    RECT rcTitle = {cardX + 20, cardY + 10, cardX + cardW - 20, cardY + 40};
    DrawTextSDF(g_hdcBuffer, texts->timeSettings, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontTitle, RGB(28, 28, 30));

    // Section: Custom Time Setting
    RECT rcSec1 = {cardX + 20, cardY + 48, cardX + cardW - 20, cardY + 68};
    DrawTextSDF(g_hdcBuffer, texts->setTimeTitle, &rcSec1, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontSection, RGB(28, 28, 30));

    // Spinner labels
    RECT rcHL = {S + 30, cardY + 72, S + 130, cardY + 90};
    RECT rcML = {S + 145, cardY + 72, S + 245, cardY + 90};
    RECT rcSL = {S + 260, cardY + 72, S + 360, cardY + 90};
    DrawTextSDF(g_hdcBuffer, texts->hours, &rcHL, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(92, 92, 97));
    DrawTextSDF(g_hdcBuffer, texts->minutes, &rcML, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(92, 92, 97));
    DrawTextSDF(g_hdcBuffer, texts->seconds, &rcSL, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(92, 92, 97));

    // Spinner buttons and display fields
    wchar_t buf[8];
    int spinBtnR = 4;

    // --- Hours spinner ---
    BOOL hrsEnabled = g_tempShowHours;
    BOOL hM = (g_hoverId == HIT_HRS_MINUS), hMP = (g_pressedId == HIT_HRS_MINUS);
    COLORREF hMC = hrsEnabled ? (hMP ? RGB(200, 200, 205) : (hM ? RGB(225, 225, 230) : RGB(245, 245, 247))) : RGB(248, 248, 250);
    RECT rHM = {S + 30, S + 107, S + 56, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rHM.left, rHM.top, rHM.right - rHM.left, rHM.bottom - rHM.top,
        GetRValue(hMC), GetGValue(hMC), GetBValue(hMC), 255);
    if (hrsEnabled) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rHM.left, rHM.top, rHM.right - rHM.left, rHM.bottom - rHM.top, 1, 210, 210, 215, 255);
    }

    BOOL hD = (g_hoverId == HIT_HRS_DISPLAY), hDP = (g_pressedId == HIT_HRS_DISPLAY);
    RECT rHD = {S + 57, S + 108, S + 97, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rHD.left, rHD.top, rHD.right - rHD.left, rHD.bottom - rHD.top, 255, 255, 255, 255);
    if (!hrsEnabled) {
        /* Gray overlay for disabled display */
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rHD.left, rHD.top, rHD.right - rHD.left, rHD.bottom - rHD.top, 245, 245, 247, 200);
    } else {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rHD.left, rHD.top, rHD.right - rHD.left, rHD.bottom - rHD.top,
            1, hD ? 0 : 80, hD ? 120 : 100, hD ? 190 : 120, 255);
    }

    BOOL hP = (g_hoverId == HIT_HRS_PLUS), hPP = (g_pressedId == HIT_HRS_PLUS);
    COLORREF hPC = hrsEnabled ? (hPP ? RGB(200, 200, 205) : (hP ? RGB(225, 225, 230) : RGB(245, 245, 247))) : RGB(248, 248, 250);
    RECT rHP = {S + 98, S + 107, S + 124, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rHP.left, rHP.top, rHP.right - rHP.left, rHP.bottom - rHP.top,
        GetRValue(hPC), GetGValue(hPC), GetBValue(hPC), 255);
    if (hrsEnabled) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rHP.left, rHP.top, rHP.right - rHP.left, rHP.bottom - rHP.top, 1, 210, 210, 215, 255);
    }

    swprintf(buf, 8, L"%02d", g_tempHours);
    RECT rHDt = rHD;
    COLORREF hrsTextColor = hrsEnabled ? RGB(28, 28, 30) : RGB(180, 180, 185);
    COLORREF hrsBtnTextColor = hrsEnabled ? RGB(60, 60, 67) : RGB(200, 200, 205);
    BOOL hrsEditing = (g_editingField == EDIT_HOURS);
    if (hrsEditing) {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rHD.left, rHD.top, rHD.right - rHD.left, rHD.bottom - rHD.top, 230, 240, 255, 255);
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rHD.left, rHD.top, rHD.right - rHD.left, rHD.bottom - rHD.top,
            2, 74, 144, 217, 255);
        DrawTextSDF(g_hdcBuffer, g_editBuffer, &rHDt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontNum, RGB(28, 28, 30));
        if (g_cursorVisible) {
            SIZE sz;
            HFONT hOld = (HFONT)SelectObject(g_hdcBuffer, hFontNum);
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, g_editCursorPos, &sz);
            SelectObject(g_hdcBuffer, hOld);
            int cursorX = rHDt.left + (rHDt.right - rHDt.left) / 2 + (sz.cx > 0 ? sz.cx - (rHDt.right - rHDt.left) / 2 : 0);
            int textFullWidth;
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, (int)wcslen(g_editBuffer), &sz);
            textFullWidth = sz.cx;
            int startX = (rHDt.left + rHDt.right - textFullWidth) / 2;
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, g_editCursorPos, &sz);
            cursorX = startX + sz.cx;
            RECT cursorRect = {cursorX, rHDt.top + 3, cursorX + 2, rHDt.bottom - 3};
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 1, 1, cursorRect.left, cursorRect.top, cursorRect.right - cursorRect.left, cursorRect.bottom - cursorRect.top, 74, 144, 217, 255);
        }
    } else {
        DrawTextSDF(g_hdcBuffer, buf, &rHDt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontNum, hrsTextColor);
    }

    RECT rHMt = rHM;
    DrawTextSDF(g_hdcBuffer, L"-", &rHMt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontLabel, hrsBtnTextColor);
    RECT rHPt = rHP;
    DrawTextSDF(g_hdcBuffer, L"+", &rHPt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontLabel, hrsBtnTextColor);

    // --- Minutes spinner ---
    BOOL minEnabled = g_tempShowMinutes;
    BOOL mM = (g_hoverId == HIT_MIN_MINUS), mMP = (g_pressedId == HIT_MIN_MINUS);
    COLORREF mMC = minEnabled ? (mMP ? RGB(200, 200, 205) : (mM ? RGB(225, 225, 230) : RGB(245, 245, 247))) : RGB(248, 248, 250);
    RECT rMM = {S + 145, S + 107, S + 171, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rMM.left, rMM.top, rMM.right - rMM.left, rMM.bottom - rMM.top,
        GetRValue(mMC), GetGValue(mMC), GetBValue(mMC), 255);
    if (minEnabled) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rMM.left, rMM.top, rMM.right - rMM.left, rMM.bottom - rMM.top, 1, 210, 210, 215, 255);
    }

    BOOL mD = (g_hoverId == HIT_MIN_DISPLAY), mDP = (g_pressedId == HIT_MIN_DISPLAY);
    RECT rMD = {S + 172, S + 108, S + 212, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rMD.left, rMD.top, rMD.right - rMD.left, rMD.bottom - rMD.top, 255, 255, 255, 255);
    if (!minEnabled) {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rMD.left, rMD.top, rMD.right - rMD.left, rMD.bottom - rMD.top, 245, 245, 247, 200);
    } else {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rMD.left, rMD.top, rMD.right - rMD.left, rMD.bottom - rMD.top,
            1, mD ? 0 : 80, mD ? 120 : 100, mD ? 190 : 120, 255);
    }

    BOOL mP = (g_hoverId == HIT_MIN_PLUS), mPP = (g_pressedId == HIT_MIN_PLUS);
    COLORREF mPC = minEnabled ? (mPP ? RGB(200, 200, 205) : (mP ? RGB(225, 225, 230) : RGB(245, 245, 247))) : RGB(248, 248, 250);
    RECT rMP = {S + 213, S + 107, S + 239, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rMP.left, rMP.top, rMP.right - rMP.left, rMP.bottom - rMP.top,
        GetRValue(mPC), GetGValue(mPC), GetBValue(mPC), 255);
    if (minEnabled) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rMP.left, rMP.top, rMP.right - rMP.left, rMP.bottom - rMP.top, 1, 210, 210, 215, 255);
    }

    swprintf(buf, 8, L"%02d", g_tempMinutes);
    RECT rMDt = rMD;
    COLORREF minTextColor = minEnabled ? RGB(28, 28, 30) : RGB(180, 180, 185);
    COLORREF minBtnTextColor = minEnabled ? RGB(60, 60, 67) : RGB(200, 200, 205);
    BOOL minEditing = (g_editingField == EDIT_MINUTES);
    if (minEditing) {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rMD.left, rMD.top, rMD.right - rMD.left, rMD.bottom - rMD.top, 230, 240, 255, 255);
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rMD.left, rMD.top, rMD.right - rMD.left, rMD.bottom - rMD.top,
            2, 74, 144, 217, 255);
        DrawTextSDF(g_hdcBuffer, g_editBuffer, &rMDt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontNum, RGB(28, 28, 30));
        if (g_cursorVisible) {
            SIZE sz;
            HFONT hOld = (HFONT)SelectObject(g_hdcBuffer, hFontNum);
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, g_editCursorPos, &sz);
            int textFullWidth;
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, (int)wcslen(g_editBuffer), &sz);
            textFullWidth = sz.cx;
            int startX = (rMDt.left + rMDt.right - textFullWidth) / 2;
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, g_editCursorPos, &sz);
            int cursorX = startX + sz.cx;
            RECT cursorRect = {cursorX, rMDt.top + 3, cursorX + 2, rMDt.bottom - 3};
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 1, 1, cursorRect.left, cursorRect.top, cursorRect.right - cursorRect.left, cursorRect.bottom - cursorRect.top, 74, 144, 217, 255);
            SelectObject(g_hdcBuffer, hOld);
        }
    } else {
        DrawTextSDF(g_hdcBuffer, buf, &rMDt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontNum, minTextColor);
    }

    RECT rMMt = rMM;
    DrawTextSDF(g_hdcBuffer, L"-", &rMMt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontLabel, minBtnTextColor);
    RECT rMPt = rMP;
    DrawTextSDF(g_hdcBuffer, L"+", &rMPt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontLabel, minBtnTextColor);

    // --- Seconds spinner ---
    BOOL secEnabled = g_tempShowSeconds;
    BOOL sM = (g_hoverId == HIT_SEC_MINUS), sMP = (g_pressedId == HIT_SEC_MINUS);
    COLORREF sMC = secEnabled ? (sMP ? RGB(200, 200, 205) : (sM ? RGB(225, 225, 230) : RGB(245, 245, 247))) : RGB(248, 248, 250);
    RECT rSM = {S + 260, S + 107, S + 286, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rSM.left, rSM.top, rSM.right - rSM.left, rSM.bottom - rSM.top,
        GetRValue(sMC), GetGValue(sMC), GetBValue(sMC), 255);
    if (secEnabled) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rSM.left, rSM.top, rSM.right - rSM.left, rSM.bottom - rSM.top, 1, 210, 210, 215, 255);
    }

    BOOL sD = (g_hoverId == HIT_SEC_DISPLAY), sDP = (g_pressedId == HIT_SEC_DISPLAY);
    RECT rSD = {S + 287, S + 108, S + 327, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rSD.left, rSD.top, rSD.right - rSD.left, rSD.bottom - rSD.top, 255, 255, 255, 255);
    if (!secEnabled) {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rSD.left, rSD.top, rSD.right - rSD.left, rSD.bottom - rSD.top, 245, 245, 247, 200);
    } else {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rSD.left, rSD.top, rSD.right - rSD.left, rSD.bottom - rSD.top,
            1, sD ? 0 : 80, sD ? 120 : 100, sD ? 190 : 120, 255);
    }

    BOOL sP = (g_hoverId == HIT_SEC_PLUS), sPP = (g_pressedId == HIT_SEC_PLUS);
    COLORREF sPC = secEnabled ? (sPP ? RGB(200, 200, 205) : (sP ? RGB(225, 225, 230) : RGB(245, 245, 247))) : RGB(248, 248, 250);
    RECT rSP = {S + 328, S + 107, S + 354, S + 133};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rSP.left, rSP.top, rSP.right - rSP.left, rSP.bottom - rSP.top,
        GetRValue(sPC), GetGValue(sPC), GetBValue(sPC), 255);
    if (secEnabled) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, spinBtnR, spinBtnR, rSP.left, rSP.top, rSP.right - rSP.left, rSP.bottom - rSP.top, 1, 210, 210, 215, 255);
    }

    swprintf(buf, 8, L"%02d", g_tempSeconds);
    RECT rSDt = rSD;
    COLORREF secTextColor = secEnabled ? RGB(28, 28, 30) : RGB(180, 180, 185);
    COLORREF secBtnTextColor = secEnabled ? RGB(60, 60, 67) : RGB(200, 200, 205);
    BOOL secEditing = (g_editingField == EDIT_SECONDS);
    if (secEditing) {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rSD.left, rSD.top, rSD.right - rSD.left, rSD.bottom - rSD.top, 230, 240, 255, 255);
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rSD.left, rSD.top, rSD.right - rSD.left, rSD.bottom - rSD.top,
            2, 74, 144, 217, 255);
        DrawTextSDF(g_hdcBuffer, g_editBuffer, &rSDt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontNum, RGB(28, 28, 30));
        if (g_cursorVisible) {
            SIZE sz;
            HFONT hOld = (HFONT)SelectObject(g_hdcBuffer, hFontNum);
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, g_editCursorPos, &sz);
            int textFullWidth;
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, (int)wcslen(g_editBuffer), &sz);
            textFullWidth = sz.cx;
            int startX = (rSDt.left + rSDt.right - textFullWidth) / 2;
            GetTextExtentPoint32W(g_hdcBuffer, g_editBuffer, g_editCursorPos, &sz);
            int cursorX = startX + sz.cx;
            RECT cursorRect = {cursorX, rSDt.top + 3, cursorX + 2, rSDt.bottom - 3};
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 1, 1, cursorRect.left, cursorRect.top, cursorRect.right - cursorRect.left, cursorRect.bottom - cursorRect.top, 74, 144, 217, 255);
            SelectObject(g_hdcBuffer, hOld);
        }
    } else {
        DrawTextSDF(g_hdcBuffer, buf, &rSDt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontNum, secTextColor);
    }

    RECT rSMt = rSM;
    DrawTextSDF(g_hdcBuffer, L"-", &rSMt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontLabel, secBtnTextColor);
    RECT rSPt = rSP;
    DrawTextSDF(g_hdcBuffer, L"+", &rSPt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontLabel, secBtnTextColor);

    // Separator
    RECT rSep = {cardX + 20, cardY + 142, cardX + cardW - 20, cardY + 144};
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 1, 1, rSep.left, rSep.top, rSep.right - rSep.left, rSep.bottom - rSep.top, 225, 225, 230, 255);

    // Section: Time Display Format
    RECT rcSec2 = {cardX + 20, cardY + 150, cardX + cardW - 20, cardY + 170};
    DrawTextSDF(g_hdcBuffer, texts->formatTitle, &rcSec2, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontSection, RGB(28, 28, 30));

    // Toggle switches
    BOOL enH = CanToggleHour(g_tempShowHours, g_tempShowMinutes, g_tempShowSeconds, g_tempShowMilliseconds);
    BOOL enM = CanToggleMinute(g_tempShowHours, g_tempShowMinutes, g_tempShowSeconds, g_tempShowMilliseconds);
    BOOL enS = TRUE;
    BOOL enMS = CanToggleMS(g_tempShowHours, g_tempShowMinutes, g_tempShowSeconds, g_tempShowMilliseconds);

    int swX1 = S + 30, swX2 = S + 200, swY1 = S + 180, swY2 = S + 222, swW = 34, swH = 18;

    // Row 1: Hour + Minute
    DrawToggleSwitch(g_pBits, DLG_WIDTH, DLG_HEIGHT, swX1, swY1 + (30 - swH) / 2, g_tempShowHours, enH);
    RECT rcTH = {swX1 + swW + 12, swY1, swX1 + 160, swY1 + 30};
    DrawTextSDF(g_hdcBuffer, texts->hours, &rcTH, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel,
        enH ? RGB(28, 28, 30) : UI_LIGHT_TEXT_DISABLED);

    DrawToggleSwitch(g_pBits, DLG_WIDTH, DLG_HEIGHT, swX2, swY1 + (30 - swH) / 2, g_tempShowMinutes, enM);
    RECT rcTM = {swX2 + swW + 12, swY1, swX2 + 160, swY1 + 30};
    DrawTextSDF(g_hdcBuffer, texts->minutes, &rcTM, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel,
        enM ? RGB(28, 28, 30) : UI_LIGHT_TEXT_DISABLED);

    // Row 2: Second + Millisecond
    DrawToggleSwitch(g_pBits, DLG_WIDTH, DLG_HEIGHT, swX1, swY2 + (30 - swH) / 2, g_tempShowSeconds, enS);
    RECT rcTS = {swX1 + swW + 12, swY2, swX1 + 160, swY2 + 30};
    DrawTextSDF(g_hdcBuffer, texts->seconds, &rcTS, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(28, 28, 30));

    DrawToggleSwitch(g_pBits, DLG_WIDTH, DLG_HEIGHT, swX2, swY2 + (30 - swH) / 2, g_tempShowMilliseconds, enMS);
    RECT rcTMS = {swX2 + swW + 12, swY2, swX2 + 160, swY2 + 30};
    DrawTextSDF(g_hdcBuffer, texts->milliseconds, &rcTMS, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel,
        enMS ? RGB(28, 28, 30) : UI_LIGHT_TEXT_DISABLED);

    // Buttons (centered)
    BOOL bCH = (g_hoverId == HIT_BTN_CONFIRM), bCP = (g_pressedId == HIT_BTN_CONFIRM);
    COLORREF bCC = bCP ? UI_PRIMARY_PRESSED : (bCH ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR);
    RECT rBtnC = {S + 85, S + 300, S + 175, S + 336};
    if (bCP) { rBtnC.top += 1; rBtnC.bottom += 1; }
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT, rBtnC.left, rBtnC.top, rBtnC.right - rBtnC.left, rBtnC.bottom - rBtnC.top, 18, 10, 2, 0.15f);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 18, 18, rBtnC.left, rBtnC.top, rBtnC.right - rBtnC.left, rBtnC.bottom - rBtnC.top,
        GetRValue(bCC), GetGValue(bCC), GetBValue(bCC), 255);
    RECT rBtnCT = rBtnC;
    DrawTextSDF(g_hdcBuffer, texts->ok, &rBtnCT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(255, 255, 255));

    BOOL bXH = (g_hoverId == HIT_BTN_CANCEL), bXP = (g_pressedId == HIT_BTN_CANCEL);
    COLORREF bXC = bXP ? UI_LIGHT_BUTTON_PRESSED : (bXH ? UI_LIGHT_BUTTON_HOVER : UI_LIGHT_BG_SECONDARY);
    RECT rBtnX = {S + 185, S + 300, S + 275, S + 336};
    if (bXP) { rBtnX.top += 1; rBtnX.bottom += 1; }
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT, rBtnX.left, rBtnX.top, rBtnX.right - rBtnX.left, rBtnX.bottom - rBtnX.top, 18, 10, 2, 0.10f);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 18, 18, rBtnX.left, rBtnX.top, rBtnX.right - rBtnX.left, rBtnX.bottom - rBtnX.top,
        GetRValue(bXC), GetGValue(bXC), GetBValue(bXC), 255);
    RECT rBtnXT = rBtnX;
    DrawTextSDF(g_hdcBuffer, texts->cancel, &rBtnXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, UI_LIGHT_TEXT_PRIMARY);

    // Fix alpha for chevron/text lines - ensure full opacity on drawn areas
    // (GDI-drawn text on DIB may have alpha < 255; fix for crisp display)
    for (int yy = 0; yy < DLG_HEIGHT; yy++) {
        BYTE* row = g_pBits + yy * DLG_WIDTH * 4;
        for (int xx = 0; xx < DLG_WIDTH; xx++) {
            if (row[xx * 4 + 3] > 0 && row[xx * 4 + 3] < 255) {
                // text areas from DrawThemeTextEx: boost alpha for readability
            }
        }
    }

    DeleteObject(hFontTitle);
    DeleteObject(hFontSection);
    DeleteObject(hFontLabel);
    DeleteObject(hFontBtn);
    DeleteObject(hFontNum);
}

static void ApplyAndSave(void) {
    // Apply time value
    SetTimerSeconds(g_tempHours * 3600 + g_tempMinutes * 60 + g_tempSeconds);
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
            g_tempHours = totalSec / 3600;
            g_tempMinutes = (totalSec % 3600) / 60;
            g_tempSeconds = totalSec % 60;
            g_tempShowHours = g_timerState.showHours;
            g_tempShowMinutes = g_timerState.showMinutes;
            g_tempShowSeconds = g_timerState.showSeconds;
            g_tempShowMilliseconds = g_timerState.showMilliseconds;
            g_origHours = g_tempHours; g_origMinutes = g_tempMinutes; g_origSeconds = g_tempSeconds;
            g_origShowHours = g_tempShowHours; g_origShowMinutes = g_tempShowMinutes;
            g_origShowSeconds = g_tempShowSeconds; g_origShowMilliseconds = g_tempShowMilliseconds;

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
            if (g_isDraggingDlg) { g_isDraggingDlg = FALSE; ReleaseCapture(); }

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
                        if (g_tempShowMinutes && g_tempMinutes < 59) g_tempMinutes++;
                        break;
                    case HIT_SEC_MINUS:
                        if (g_tempShowSeconds && g_tempSeconds > 0) g_tempSeconds--;
                        break;
                    case HIT_SEC_PLUS:
                        if (g_tempShowSeconds && g_tempSeconds < 59) g_tempSeconds++;
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
                            g_tempShowHours = !h;
                            if (!g_tempShowHours) g_tempHours = 0;
                        }
                        break;
                    case HIT_TOGGLE_MINUTE:
                        if (CanToggleMinute(h, !m, s, ms)) {
                            g_tempShowMinutes = !m;
                            if (!g_tempShowMinutes) g_tempMinutes = 0;
                        }
                        break;
                    case HIT_TOGGLE_SECOND:
                        g_tempShowSeconds = !s;
                        if (!g_tempShowSeconds) g_tempSeconds = 0;
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
