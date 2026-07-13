#include "timer_dialog_internal.h"
#include "timer_render_utils.h"
#include "timer_window_utils.h"
#include "timer_buffer.h"
#include "ios_menu.h"
#include <commdlg.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdlib.h>

#define DLG_WIDTH 460
#define DLG_HEIGHT 560
#define DLG_SHADOW 30
#define DLG_RADIUS 8

// Layout Rects (Virtual hit-test areas, relative to window)
static RECT rcCard = {DLG_SHADOW, DLG_SHADOW, DLG_WIDTH - DLG_SHADOW, DLG_HEIGHT - DLG_SHADOW};
static RECT rcFontColorBox = {DLG_SHADOW + 20, DLG_SHADOW + 50, DLG_SHADOW + 190, DLG_SHADOW + 100};
static RECT rcBgColorBox = {DLG_SHADOW + 210, DLG_SHADOW + 50, DLG_SHADOW + 380, DLG_SHADOW + 100};
static RECT rcResetText = {DLG_SHADOW + 140, DLG_SHADOW + 105, DLG_SHADOW + 190, DLG_SHADOW + 125};
static RECT rcRandomText = {DLG_SHADOW + 310, DLG_SHADOW + 105, DLG_SHADOW + 380, DLG_SHADOW + 125};

static RECT rcSliderTrack = {DLG_SHADOW + 20, DLG_SHADOW + 170, DLG_SHADOW + 320, DLG_SHADOW + 180};
static int sliderThumbR = 10;

static RECT rcFontCombo = {DLG_SHADOW + 20, DLG_SHADOW + 240, DLG_SHADOW + 380, DLG_SHADOW + 280};
static RECT rcToggleCard = {DLG_SHADOW + 20, DLG_SHADOW + 310, DLG_SHADOW + 380, DLG_SHADOW + 370};
static RECT rcToggleSwitch = {DLG_SHADOW + 380 - 15 - 34, DLG_SHADOW + 310 + (60-18)/2, DLG_SHADOW + 380 - 15, DLG_SHADOW + 310 + (60-18)/2 + 18};

static RECT rcBtnApply = {DLG_SHADOW + 20, DLG_SHADOW + 400, DLG_SHADOW + 190, DLG_SHADOW + 440};
static RECT rcBtnCancel = {DLG_SHADOW + 210, DLG_SHADOW + 400, DLG_SHADOW + 380, DLG_SHADOW + 440};

typedef enum {
    HIT_NONE,
    HIT_CARD_BG,
    HIT_FONT_COLOR,
    HIT_BG_COLOR,
    HIT_RESET,
    HIT_RANDOM,
    HIT_SLIDER_TRACK,
    HIT_SLIDER_THUMB,
    HIT_FONT_COMBO,
    HIT_TOGGLE_BG,
    HIT_BTN_APPLY,
    HIT_BTN_CANCEL
} HitTestID;

static HWND g_hAppearanceDialog = NULL;
static HDC g_hdcBuffer = NULL;
static HBITMAP g_hbmBuffer = NULL;
static BYTE* g_pBits = NULL;

static COLORREF g_tempFontColor;
static COLORREF g_tempBackgroundColor;
static BYTE g_tempOpacity;
static BOOL g_tempTransparentBackground;
static wchar_t g_tempFontName[256];

static COLORREF g_originalFontColor;
static COLORREF g_originalBackgroundColor;
static BYTE g_originalOpacity;
static BOOL g_originalTransparentBackground;
static wchar_t g_originalFontName[256];

static HitTestID g_hoverId = HIT_NONE;
static HitTestID g_pressedId = HIT_NONE;
static BOOL g_isDraggingSlider = FALSE;
static BOOL g_isDraggingDlg = FALSE;
static POINT g_dragStartScreen;
static RECT g_dlgStartRect;

static IosMenuItem* g_fontMenuItems = NULL;
static int g_fontMenuCount = 0;
static BOOL g_isTrackingMouse = FALSE;

static void FreeFontMenu() {
    if (g_fontMenuItems) {
        for(int i=0; i<g_fontMenuCount; i++) {
            if(g_fontMenuItems[i].label) free((void*)g_fontMenuItems[i].label);
        }
        free(g_fontMenuItems);
        g_fontMenuItems = NULL;
        g_fontMenuCount = 0;
    }
}

static void RebuildFontMenu() {
    FreeFontMenu();
    if (InitializeFontResourceSystem()) {
        wchar_t fontNames[32][256];
        g_fontMenuCount = GetMixedFontList(fontNames, 32);
        g_fontMenuItems = (IosMenuItem*)calloc(g_fontMenuCount, sizeof(IosMenuItem));
        for (int i = 0; i < g_fontMenuCount; i++) {
            g_fontMenuItems[i].type = IOS_MENU_ITEM_NORMAL;
            g_fontMenuItems[i].label = _wcsdup(fontNames[i]);
            g_fontMenuItems[i].commandId = 1000 + i;
        }
    } else {
        g_fontMenuCount = GetAvailableFontCount();
        g_fontMenuItems = (IosMenuItem*)calloc(g_fontMenuCount, sizeof(IosMenuItem));
        for (int i = 0; i < g_fontMenuCount; i++) {
            const FontInfo* fontInfo = GetFontInfoByIndex(i);
            g_fontMenuItems[i].type = IOS_MENU_ITEM_NORMAL;
            g_fontMenuItems[i].label = _wcsdup(fontInfo->displayName);
            g_fontMenuItems[i].commandId = 1000 + i;
        }
    }
}

static void UpdateAppearanceLayeredWindow();

static void DrawTextSDF(HDC hdc, const wchar_t* text, RECT* rc, int format, HFONT hFont, COLORREF color) {
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    HTHEME hTheme = OpenThemeData(NULL, L"WINDOW");
    DTTOPTS dttOpts = {sizeof(DTTOPTS)};
    dttOpts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;
    dttOpts.crText = color;
    DrawThemeTextEx(hTheme, hdc, 0, 0, text, -1, format, rc, &dttOpts);
    CloseThemeData(hTheme);
    
    SelectObject(hdc, hOldFont);
}

static void RenderDialogUI() {
    memset(g_pBits, 0, DLG_WIDTH * DLG_HEIGHT * 4);
    
    // Solid panel: fill entire window (system provides drop shadow via DWM)
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, DLG_RADIUS, DLG_RADIUS, 0, 0, DLG_WIDTH, DLG_HEIGHT,
        GetRValue(UI_LIGHT_BG_PRIMARY), GetGValue(UI_LIGHT_BG_PRIMARY), GetBValue(UI_LIGHT_BG_PRIMARY), 255);

    // Prepare GDI for Text
    HFONT hFontLabel = CreateFontW(16, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
    HFONT hFontSmall = CreateFontW(14, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
    HFONT hFontBtn = CreateFontW(16, 0,0,0, FW_SEMIBOLD, 0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
    
    const MenuTexts* texts = GetMenuTexts();
    
    // Row 1: Font Color
    RECT rcLabelFC = {rcFontColorBox.left, rcFontColorBox.top - 25, rcFontColorBox.right, rcFontColorBox.top};
    DrawTextSDF(g_hdcBuffer, texts->fontColor, &rcLabelFC, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_PRIMARY);
    
    BYTE r = GetRValue(g_tempFontColor), g = GetGValue(g_tempFontColor), b = GetBValue(g_tempFontColor);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8, rcFontColorBox.left, rcFontColorBox.top, rcFontColorBox.right-rcFontColorBox.left, rcFontColorBox.bottom-rcFontColorBox.top, r, g, b, 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8, rcFontColorBox.left, rcFontColorBox.top, rcFontColorBox.right-rcFontColorBox.left, rcFontColorBox.bottom-rcFontColorBox.top, 1, 0, 0, 0, 20); // subtle border
    
    DrawTextSDF(g_hdcBuffer, texts->resetButton, &rcResetText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, hFontSmall, (g_hoverId == HIT_RESET) ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR);

    // Row 1: Bg Color
    RECT rcLabelBC = {rcBgColorBox.left, rcBgColorBox.top - 25, rcBgColorBox.right, rcBgColorBox.top};
    DrawTextSDF(g_hdcBuffer, texts->backgroundColor, &rcLabelBC, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_PRIMARY);
    
    r = GetRValue(g_tempBackgroundColor), g = GetGValue(g_tempBackgroundColor), b = GetBValue(g_tempBackgroundColor);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8, rcBgColorBox.left, rcBgColorBox.top, rcBgColorBox.right-rcBgColorBox.left, rcBgColorBox.bottom-rcBgColorBox.top, r, g, b, 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8, rcBgColorBox.left, rcBgColorBox.top, rcBgColorBox.right-rcBgColorBox.left, rcBgColorBox.bottom-rcBgColorBox.top, 1, 0, 0, 0, 20); // subtle border
    
    DrawTextSDF(g_hdcBuffer, texts->randomButton, &rcRandomText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, hFontSmall, (g_hoverId == HIT_RANDOM) ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR);

    // Row 2: Opacity Slider
    RECT rcLabelOp = {rcSliderTrack.left, rcSliderTrack.top - 25, rcSliderTrack.right, rcSliderTrack.top};
    DrawTextSDF(g_hdcBuffer, texts->opacity, &rcLabelOp, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_PRIMARY);
    
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 3, 3, rcSliderTrack.left, rcSliderTrack.top, rcSliderTrack.right-rcSliderTrack.left, rcSliderTrack.bottom-rcSliderTrack.top, 220, 220, 225, 255);
    float pct = (g_tempOpacity - 50) / 205.0f;
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    int fillW = (int)((rcSliderTrack.right - rcSliderTrack.left) * pct);
    if (fillW > 0) {
        FillStripedRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 3, 3, rcSliderTrack.left, rcSliderTrack.top, fillW, rcSliderTrack.bottom-rcSliderTrack.top, 0, 95, 184, 255);
    }
    
    int thumbX = rcSliderTrack.left + fillW;
    int thumbY = rcSliderTrack.top + (rcSliderTrack.bottom - rcSliderTrack.top)/2;
    // Thumb shadow
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT, thumbX - sliderThumbR, thumbY - sliderThumbR, sliderThumbR*2, sliderThumbR*2, sliderThumbR, 8, 2, 0.2f);
    // Thumb body
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, sliderThumbR, sliderThumbR, thumbX - sliderThumbR, thumbY - sliderThumbR, sliderThumbR*2, sliderThumbR*2, 255, 255, 255, 255);
    // Thumb border
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, sliderThumbR, sliderThumbR, thumbX - sliderThumbR, thumbY - sliderThumbR, sliderThumbR*2, sliderThumbR*2, 2, 0, 95, 184, 255);

    wchar_t opText[16]; swprintf(opText, 16, L"%d%%", (int)((g_tempOpacity/255.0f)*100));
    RECT rcOpText = {rcSliderTrack.right + 15, rcSliderTrack.top - 5, rcSliderTrack.right + 60, rcSliderTrack.bottom + 5};
    DrawTextSDF(g_hdcBuffer, opText, &rcOpText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontBtn, UI_LIGHT_TEXT_PRIMARY);

    // Row 3: Font Combobox
    RECT rcLabelFnt = {rcFontCombo.left, rcFontCombo.top - 25, rcFontCombo.right, rcFontCombo.top};
    DrawTextSDF(g_hdcBuffer, texts->font, &rcLabelFnt, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_PRIMARY);
    
    BOOL comboHover = (g_hoverId == HIT_FONT_COMBO);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcFontCombo.left, rcFontCombo.top, rcFontCombo.right-rcFontCombo.left, rcFontCombo.bottom-rcFontCombo.top, 255, 255, 255, 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcFontCombo.left, rcFontCombo.top, rcFontCombo.right-rcFontCombo.left, rcFontCombo.bottom-rcFontCombo.top, 2, comboHover?0:80, comboHover?120:100, comboHover?190:120, 255);
    
    RECT rcFntText = rcFontCombo; rcFntText.left += 15;
    DrawTextSDF(g_hdcBuffer, g_tempFontName, &rcFntText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_PRIMARY);
    
    // Draw Chevron Down
    HPEN hPenLine = CreatePen(PS_SOLID, 2, comboHover?UI_PRIMARY_COLOR:UI_LIGHT_BORDER);
    HGDIOBJ hOldPen = SelectObject(g_hdcBuffer, hPenLine);
    int cx = rcFontCombo.right - 20;
    int cy = rcFontCombo.top + (rcFontCombo.bottom - rcFontCombo.top)/2 - 2;
    MoveToEx(g_hdcBuffer, cx - 5, cy - 2, NULL);
    LineTo(g_hdcBuffer, cx, cy + 3);
    LineTo(g_hdcBuffer, cx + 5, cy - 2);
    SelectObject(g_hdcBuffer, hOldPen);
    DeleteObject(hPenLine);
    
    // Alpha fix for Chevron
    for (int y = cy - 4; y <= cy + 5; y++) {
        BYTE* row = g_pBits + (y * DLG_WIDTH * 4);
        for (int x = cx - 6; x <= cx + 6; x++) {
            row[x * 4 + 3] = 255;
        }
    }

    // Row 4: Transparency Toggle (flattened into panel, no floating card)
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rcToggleCard.left, rcToggleCard.top, rcToggleCard.right-rcToggleCard.left, rcToggleCard.bottom-rcToggleCard.top,
        1, GetRValue(UI_LIGHT_BORDER), GetGValue(UI_LIGHT_BORDER), GetBValue(UI_LIGHT_BORDER), 255);
    
    RECT rcToggleText = rcToggleCard; rcToggleText.left += 15;
    DrawTextSDF(g_hdcBuffer, texts->transparentBackground, &rcToggleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_PRIMARY);
    
    // Draw Switch
    int swX = rcToggleSwitch.left;
    int swY = rcToggleSwitch.top;
    int swW = rcToggleSwitch.right - rcToggleSwitch.left;
    int swH = rcToggleSwitch.bottom - rcToggleSwitch.top;
    if (g_tempTransparentBackground) {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, swH/2, swH/2, swX, swY, swW, swH, 0, 95, 184, 255);
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, (swH-4)/2, (swH-4)/2, swX + swW - (swH-4) - 2, swY + 2, swH-4, swH-4, 255, 255, 255, 255);
    } else {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, swH/2, swH/2, swX, swY, swW, swH, 180, 180, 180, 255);
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, (swH-4)/2, (swH-4)/2, swX + 2, swY + 2, swH-4, swH-4, 255, 255, 255, 255);
    }

    // Row 5: Action Buttons
    // Apply
    BOOL applyHover = (g_hoverId == HIT_BTN_APPLY);
    COLORREF applyFill = applyHover ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR;
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rcBtnApply.left, rcBtnApply.top, rcBtnApply.right-rcBtnApply.left, rcBtnApply.bottom-rcBtnApply.top,
        GetRValue(applyFill), GetGValue(applyFill), GetBValue(applyFill), 255);
    RECT rcApplyT = rcBtnApply;
    DrawTextSDF(g_hdcBuffer, L"Apply", &rcApplyT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(255,255,255));

    // Cancel
    BOOL cancelHover = (g_hoverId == HIT_BTN_CANCEL);
    COLORREF cancelFill = cancelHover ? UI_LIGHT_BUTTON_HOVER : UI_LIGHT_BG_SECONDARY;
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rcBtnCancel.left, rcBtnCancel.top, rcBtnCancel.right-rcBtnCancel.left, rcBtnCancel.bottom-rcBtnCancel.top,
        GetRValue(cancelFill), GetGValue(cancelFill), GetBValue(cancelFill), 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rcBtnCancel.left, rcBtnCancel.top, rcBtnCancel.right-rcBtnCancel.left, rcBtnCancel.bottom-rcBtnCancel.top,
        1, GetRValue(UI_LIGHT_BORDER), GetGValue(UI_LIGHT_BORDER), GetBValue(UI_LIGHT_BORDER), 255);
    RECT rcCancelT = rcBtnCancel;
    DrawTextSDF(g_hdcBuffer, L"Cancel", &rcCancelT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, UI_LIGHT_TEXT_PRIMARY);

    DeleteObject(hFontLabel);
    DeleteObject(hFontSmall);
    DeleteObject(hFontBtn);
}

static void UpdateAppearanceLayeredWindow() {
    RenderDialogUI();
    HDC hdc = GetDC(NULL); 
    POINT dst = {0,0}, src = {0,0}; 
    SIZE sz = {DLG_WIDTH, DLG_HEIGHT}; 
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT rc; GetWindowRect(g_hAppearanceDialog, &rc); 
    dst.x = rc.left; dst.y = rc.top;
    UpdateLayeredWindow(g_hAppearanceDialog, hdc, &dst, &sz, g_hdcBuffer, &src, 0, &bf, ULW_ALPHA); 
    ReleaseDC(NULL, hdc);
}

static void ApplyPreview() {
    g_timerState.fontColor = g_tempFontColor;
    g_timerState.backgroundColor = g_tempBackgroundColor;
    g_timerState.windowOpacity = g_tempOpacity;
    
    wcscpy_s(g_timerState.currentFontName, sizeof(g_timerState.currentFontName)/sizeof(wchar_t), g_tempFontName);
    ClearFontCache();
    g_timerState.cachedFontValid = FALSE;
    g_timerState.cachedHFont = NULL;

    if (g_tempTransparentBackground != g_timerState.transparentBackground) {
        g_timerState.transparentBackground = g_tempTransparentBackground;
        LONG exStyle = GetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE);
        SetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        SetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        if (g_tempTransparentBackground) {
            SetWindowRoundedCorners(g_timerState.hMainWnd, 0);
            SetWindowShadow(g_timerState.hMainWnd, FALSE);
            UpdateLayeredWindow_Render();
        } else {
            SetWindowRoundedCorners(g_timerState.hMainWnd, 6);
            SetWindowShadow(g_timerState.hMainWnd, TRUE);
            SetLayeredWindowAttributes(g_timerState.hMainWnd, 0, g_tempOpacity, LWA_ALPHA);
            InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
            UpdateWindow(g_timerState.hMainWnd);
        }
    } else {
        if (g_timerState.transparentBackground) {
            UpdateLayeredWindow_Render();
        } else {
            SetLayeredWindowAttributes(g_timerState.hMainWnd, 0, g_tempOpacity, LWA_ALPHA);
            InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
            UpdateWindow(g_timerState.hMainWnd);
        }
    }
}

static HitTestID HitTest(POINT pt) {
    if (PtInRect(&rcFontColorBox, pt)) return HIT_FONT_COLOR;
    if (PtInRect(&rcBgColorBox, pt)) return HIT_BG_COLOR;
    if (PtInRect(&rcResetText, pt)) return HIT_RESET;
    if (PtInRect(&rcRandomText, pt)) return HIT_RANDOM;
    if (PtInRect(&rcSliderTrack, pt)) return HIT_SLIDER_TRACK;
    
    int fillW = (int)((rcSliderTrack.right - rcSliderTrack.left) * ((g_tempOpacity - 50) / 205.0f));
    int thumbX = rcSliderTrack.left + fillW;
    int thumbY = rcSliderTrack.top + (rcSliderTrack.bottom - rcSliderTrack.top)/2;
    RECT rcThumb = {thumbX - sliderThumbR, thumbY - sliderThumbR, thumbX + sliderThumbR, thumbY + sliderThumbR};
    if (PtInRect(&rcThumb, pt)) return HIT_SLIDER_THUMB;
    
    if (PtInRect(&rcFontCombo, pt)) return HIT_FONT_COMBO;
    if (PtInRect(&rcToggleCard, pt)) return HIT_TOGGLE_BG;
    if (PtInRect(&rcBtnApply, pt)) return HIT_BTN_APPLY;
    if (PtInRect(&rcBtnCancel, pt)) return HIT_BTN_CANCEL;
    if (PtInRect(&rcCard, pt)) return HIT_CARD_BG;
    return HIT_NONE;
}

LRESULT CALLBACK ModernAppearanceDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            g_hAppearanceDialog = hwnd;
            HDC hdc = GetDC(NULL); g_hdcBuffer = CreateCompatibleDC(hdc);
            BITMAPINFOHEADER bi = {sizeof(bi), DLG_WIDTH, -DLG_HEIGHT, 1, 32, BI_RGB};
            g_hbmBuffer = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&g_pBits, NULL, 0);
            SelectObject(g_hdcBuffer, g_hbmBuffer);
            ReleaseDC(NULL, hdc);
            
            g_originalFontColor = g_timerState.fontColor;
            g_originalBackgroundColor = g_timerState.backgroundColor;
            g_originalOpacity = g_timerState.windowOpacity;
            g_originalTransparentBackground = g_timerState.transparentBackground;
            wcscpy_s(g_originalFontName, 256, g_timerState.currentFontName);
            
            g_tempFontColor = g_originalFontColor;
            g_tempBackgroundColor = g_originalBackgroundColor;
            g_tempOpacity = g_originalOpacity;
            g_tempTransparentBackground = g_originalTransparentBackground;
            wcscpy_s(g_tempFontName, 256, g_originalFontName);
            
            RebuildFontMenu();
            UpdateAppearanceLayeredWindow();
            // 启动刷新定时器（ID=2），确保主窗口在面板存活期间持续重绘
            // （计时器暂停时主窗口没有 WM_TIMER 驱动，需要由此补偿）
            SetTimer(hwnd, 2, 100, NULL);
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 2 && g_timerState.hMainWnd) {
                // 驱动主窗口刷新，使字体悬停预览实时可见
                if (g_timerState.transparentBackground) {
                    UpdateLayeredWindow_Render();
                } else {
                    InvalidateRect(g_timerState.hMainWnd, NULL, FALSE);
                    UpdateWindow(g_timerState.hMainWnd);
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!g_isTrackingMouse) {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                g_isTrackingMouse = TRUE;
            }

            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            if (g_isDraggingDlg) {
                POINT cur; GetCursorPos(&cur);
                int dx = cur.x - g_dragStartScreen.x;
                int dy = cur.y - g_dragStartScreen.y;
                SetWindowPos(hwnd, NULL, g_dlgStartRect.left + dx, g_dlgStartRect.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            if (g_isDraggingSlider) {
                float pct = (float)(pt.x - rcSliderTrack.left) / (rcSliderTrack.right - rcSliderTrack.left);
                if (pct < 0) pct = 0; if (pct > 1) pct = 1;
                g_tempOpacity = (BYTE)(50 + pct * 205);
                ApplyPreview();
                UpdateAppearanceLayeredWindow();
                return 0;
            }
            
            HitTestID hit = HitTest(pt);
            if (hit != g_hoverId) {
                g_hoverId = hit;
                UpdateAppearanceLayeredWindow();
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            g_isTrackingMouse = FALSE;
            if (g_hoverId != HIT_NONE) {
                g_hoverId = HIT_NONE;
                UpdateAppearanceLayeredWindow();
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);
            g_pressedId = hit;
            if (hit == HIT_SLIDER_TRACK || hit == HIT_SLIDER_THUMB) {
                g_isDraggingSlider = TRUE;
                SetCapture(hwnd);
                float pct = (float)(pt.x - rcSliderTrack.left) / (rcSliderTrack.right - rcSliderTrack.left);
                if (pct < 0) pct = 0; if (pct > 1) pct = 1;
                g_tempOpacity = (BYTE)(50 + pct * 205);
                ApplyPreview();
            } else if (hit == HIT_CARD_BG) {
                g_isDraggingDlg = TRUE;
                GetCursorPos(&g_dragStartScreen);
                GetWindowRect(hwnd, &g_dlgStartRect);
                SetCapture(hwnd);
            } else if (hit == HIT_FONT_COMBO) {
                RECT rc; GetWindowRect(hwnd, &rc);
                int menuX = rc.left + rcFontCombo.left;
                int menuY = rc.top + rcFontCombo.bottom;
                IosMenu_Show(hwnd, menuX, menuY, g_fontMenuItems, g_fontMenuCount);
            }
            UpdateAppearanceLayeredWindow();
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_isDraggingSlider) { g_isDraggingSlider = FALSE; ReleaseCapture(); }
            if (g_isDraggingDlg) { g_isDraggingDlg = FALSE; ReleaseCapture(); }
            
            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);
            if (hit == g_pressedId && hit != HIT_NONE) {
                if (hit == HIT_FONT_COLOR) {
                    CHOOSECOLOR cc = {sizeof(CHOOSECOLOR), hwnd, NULL, g_tempFontColor, NULL, CC_FULLOPEN | CC_RGBINIT, 0, 0, NULL};
                    COLORREF customColors[16] = {0}; cc.lpCustColors = customColors;
                    if (ChooseColor(&cc)) {
                        g_tempFontColor = cc.rgbResult;
                        ApplyPreview();
                        UpdateAppearanceLayeredWindow();
                    }
                } else if (hit == HIT_BG_COLOR) {
                    CHOOSECOLOR cc = {sizeof(CHOOSECOLOR), hwnd, NULL, g_tempBackgroundColor, NULL, CC_FULLOPEN | CC_RGBINIT, 0, 0, NULL};
                    COLORREF customColors[16] = {0}; cc.lpCustColors = customColors;
                    if (ChooseColor(&cc)) {
                        g_tempBackgroundColor = cc.rgbResult;
                        ApplyPreview();
                        UpdateAppearanceLayeredWindow();
                    }
                } else if (hit == HIT_RESET) {
                    g_tempFontColor = RGB(255,255,255);
                    g_tempBackgroundColor = RGB(192,192,192);
                    g_tempOpacity = 255;
                    ApplyPreview();
                    UpdateAppearanceLayeredWindow();
                } else if (hit == HIT_RANDOM) {
                    g_tempFontColor = RGB(rand()%256, rand()%256, rand()%256);
                    g_tempBackgroundColor = RGB(rand()%256, rand()%256, rand()%256);
                    g_tempOpacity = 150 + (rand()%106);
                    ApplyPreview();
                    UpdateAppearanceLayeredWindow();
                } else if (hit == HIT_TOGGLE_BG) {
                    g_tempTransparentBackground = !g_tempTransparentBackground;
                    ApplyPreview();
                    UpdateAppearanceLayeredWindow();
                } else if (hit == HIT_BTN_APPLY) {
                    SaveAppearanceConfig();
                    DestroyWindow(hwnd);
                    return 0;
                } else if (hit == HIT_BTN_CANCEL) {
                    g_tempFontColor = g_originalFontColor;
                    g_tempBackgroundColor = g_originalBackgroundColor;
                    g_tempOpacity = g_originalOpacity;
                    g_tempTransparentBackground = g_originalTransparentBackground;
                    wcscpy_s(g_tempFontName, 256, g_originalFontName);
                    ApplyPreview();
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            g_pressedId = HIT_NONE;
            UpdateAppearanceLayeredWindow();
            return 0;
        }
        case WM_COMMAND: {
            int cmd = LOWORD(wParam);
            if (cmd == 9999) {
                // 字体菜单关闭，恢复主窗口字体到当前确认状态（g_tempFontName）
                wcscpy_s(g_timerState.currentFontName, sizeof(g_timerState.currentFontName)/sizeof(wchar_t), g_tempFontName);
                ClearFontCache();
                g_timerState.cachedFontValid = FALSE;
                g_timerState.cachedHFont = NULL;
                if (g_timerState.transparentBackground) {
                    UpdateLayeredWindow_Render();
                } else {
                    InvalidateRect(g_timerState.hMainWnd, NULL, FALSE);
                    UpdateWindow(g_timerState.hMainWnd);
                }
            } else if (cmd >= 11000 && cmd < 11000 + g_fontMenuCount) {
                // 悬停预览命令（10000 + commandId，即 10000+1000+i = 11000+i）
                int idx = cmd - 11000;
                // 临时切换字体预览（不修改 g_tempFontName，仅实时刷新主窗口显示）
                wcscpy_s(g_timerState.currentFontName, sizeof(g_timerState.currentFontName)/sizeof(wchar_t), g_fontMenuItems[idx].label);
                ClearFontCache();
                g_timerState.cachedFontValid = FALSE;
                g_timerState.cachedHFont = NULL;
                // 驱动主窗口立即重绘以呈现预览效果
                if (g_timerState.transparentBackground) {
                    UpdateLayeredWindow_Render();
                } else {
                    InvalidateRect(g_timerState.hMainWnd, NULL, FALSE);
                    UpdateWindow(g_timerState.hMainWnd);
                }
            } else if (cmd >= 1000 && cmd < 1000 + g_fontMenuCount) {
                // 正式选择：持久化到 g_tempFontName
                int idx = cmd - 1000;
                wcscpy_s(g_tempFontName, 256, g_fontMenuItems[idx].label);
                ApplyPreview();
                UpdateAppearanceLayeredWindow();
            }
            return 0;
        }
        case WM_CLOSE: {
            // Treat closing without applying as a Cancel operation
            g_tempFontColor = g_originalFontColor;
            g_tempBackgroundColor = g_originalBackgroundColor;
            g_tempOpacity = g_originalOpacity;
            g_tempTransparentBackground = g_originalTransparentBackground;
            wcscpy_s(g_tempFontName, 256, g_originalFontName);
            ApplyPreview();
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, 2); // 销毁刷新定时器
            FreeFontMenu();
            if (g_hbmBuffer) DeleteObject(g_hbmBuffer);
            if (g_hdcBuffer) DeleteDC(g_hdcBuffer);
            g_hAppearanceDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND GetAppearanceDialog(void) {
    return g_hAppearanceDialog;
}

void ShowAppearanceDialog() {
    if (g_hAppearanceDialog && IsWindow(g_hAppearanceDialog)) {
        SetForegroundWindow(g_hAppearanceDialog);
        return;
    }
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = ModernAppearanceDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ModernAppearanceDialogClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    RECT parentRect; GetWindowRect(g_timerState.hMainWnd, &parentRect);
    int parentWidth = parentRect.right - parentRect.left;
    int parentHeight = parentRect.bottom - parentRect.top;

    // 默认尝试放在主窗口右侧，保持 20 像素间隔
    int x = parentRect.right + 20;
    int y = parentRect.top + (parentHeight - DLG_HEIGHT) / 2;

    // 获取屏幕工作区尺寸（避开任务栏）
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    // 如果右边超出了屏幕边界，则尝试放在左边
    if (x + DLG_WIDTH > workArea.right) {
        x = parentRect.left - DLG_WIDTH - 20;
    }

    // 最终边界检查，确保不超出工作区范围
    if (x < workArea.left) x = workArea.left;
    if (y < workArea.top) y = workArea.top;
    if (x + DLG_WIDTH > workArea.right) x = workArea.right - DLG_WIDTH;
    if (y + DLG_HEIGHT > workArea.bottom) y = workArea.bottom - DLG_HEIGHT;

    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST, L"ModernAppearanceDialogClass", L"", WS_POPUP, x, y, DLG_WIDTH, DLG_HEIGHT, g_timerState.hMainWnd, NULL, GetModuleHandle(NULL), NULL);
    EnableWindow(g_timerState.hMainWnd, FALSE);
    ShowWindow(hwnd, SW_SHOW);
}

void CreateAppearanceDialog() {
}
