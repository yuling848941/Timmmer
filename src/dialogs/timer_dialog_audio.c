#include "timer_dialog_internal.h"
#include "timer_render_utils.h"
#include "ios_menu.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================
// Layout Constants
// ============================================
#define DLG_WIDTH  500
#define DLG_HEIGHT 380
#define DLG_SHADOW 20
#define DLG_RADIUS 12

// Hit test IDs
typedef enum {
    HIT_NONE,
    HIT_CARD_BG,
    HIT_TOGGLE,
    HIT_SEGMENT_CH,
    HIT_SEGMENT_EN,
    HIT_SEGMENT_CUSTOM,
    HIT_BROWSE,
    HIT_BTN_OK,
    HIT_BTN_CANCEL
} HitTestID;

// ============================================
// State
// ============================================
static HWND g_hAudioDialog = NULL;
static HDC  g_hdcBuffer  = NULL;
static HBITMAP g_hbmBuffer = NULL;
static BYTE* g_pBits = NULL;

// Temporary state (mirrors g_timerState)
static BOOL  g_tempEnableAudio;
static int   g_tempAudioType; // 0=Chinese, 1=English, 2=Custom
static wchar_t g_tempCustomPath[MAX_PATH];

// Original state (for cancel rollback)
static BOOL  g_originalEnableAudio;
static int   g_originalAudioType;
static wchar_t g_originalCustomPath[MAX_PATH];

// Interaction
static HitTestID g_hoverId = HIT_NONE;
static HitTestID g_pressedId = HIT_NONE;
static BOOL g_isDraggingDlg = FALSE;
static POINT g_dragStartScreen;
static RECT g_dlgStartRect;

// Segment layout
static RECT rcSegmented = {DLG_SHADOW + 50, DLG_SHADOW + 130, DLG_SHADOW + 450, DLG_SHADOW + 165};

// Toggle layout
static RECT rcToggleSwitch = {DLG_SHADOW + 380 - 15 - 34, DLG_SHADOW + 58, DLG_SHADOW + 380 - 15, DLG_SHADOW + 58 + 18};

// Input layout
static RECT rcInputBox = {DLG_SHADOW + 50, DLG_SHADOW + 220, DLG_SHADOW + 400, DLG_SHADOW + 255};
static RECT rcBrowseBtn = {DLG_SHADOW + 410, DLG_SHADOW + 220, DLG_SHADOW + 450, DLG_SHADOW + 255};

// Button layout
static RECT rcBtnOk     = {DLG_SHADOW + 310, DLG_SHADOW + 320, DLG_SHADOW + 390, DLG_SHADOW + 360};
static RECT rcBtnCancel = {DLG_SHADOW + 400, DLG_SHADOW + 320, DLG_SHADOW + 460, DLG_SHADOW + 360};

// ============================================
// Drawing Helpers
// ============================================

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

static void DrawSwitchWin11(int x, int y, BOOL isOn) {
    int sw = 34, sh = 18, ts = 10;
    int cy = y + (rcToggleSwitch.bottom - rcToggleSwitch.top - sh) / 2;
    if (isOn) {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, sh / 2, sh / 2, x, cy, sw, sh, 0, 103, 192, 255);
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, ts / 2, ts / 2, x + sw - ts - 4, cy + 4, ts, ts, 255, 255, 255, 255);
    } else {
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, sh / 2, sh / 2, x, cy, sw, sh, 180, 180, 180, 255);
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, (sh - 2) / 2, (sh - 2) / 2, x + 1, cy + 1, sw - 2, sh - 2, 255, 255, 255, 255);
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, ts / 2, ts / 2, x + 5, cy + 4, ts, ts, 120, 120, 120, 255);
    }
}

static void RenderDialogUI() {
    memset(g_pBits, 0, DLG_WIDTH * DLG_HEIGHT * 4);

    // Card shadow + main card
    RECT rcCard = {DLG_SHADOW, DLG_SHADOW, DLG_WIDTH - DLG_SHADOW, DLG_HEIGHT - DLG_SHADOW};
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT, rcCard.left, rcCard.top, rcCard.right - rcCard.left, rcCard.bottom - rcCard.top, DLG_RADIUS, DLG_SHADOW, 4, 0.15f);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, DLG_RADIUS, DLG_RADIUS, rcCard.left, rcCard.top, rcCard.right - rcCard.left, rcCard.bottom - rcCard.top, 250, 250, 252, 255);

    const MenuTexts* texts = GetMenuTexts();

    HFONT hFontLabel = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontSmall  = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontBtn    = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontIcon   = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");

    // ---- Toggle Switch ----
    // Label "启用音频提示"
    RECT rcToggleLabel = {DLG_SHADOW + 50, DLG_SHADOW + 58, DLG_SHADOW + 200, DLG_SHADOW + 78};
    DrawTextSDF(g_hdcBuffer, texts->enableAudio, &rcToggleLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(30, 30, 30));
    // Switch control
    DrawSwitchWin11(rcToggleSwitch.left, DLG_SHADOW + 55, g_tempEnableAudio);

    // ---- Segment: 音频语言 ----
    RECT rcSegLabel = {DLG_SHADOW + 50, DLG_SHADOW + 105, DLG_SHADOW + 200, DLG_SHADOW + 125};
    DrawTextSDF(g_hdcBuffer, texts->audioLanguage, &rcSegLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontSmall, RGB(30, 30, 30));

    // Segmented Control background
    BOOL segHover = (g_hoverId == HIT_SEGMENT_CH || g_hoverId == HIT_SEGMENT_EN || g_hoverId == HIT_SEGMENT_CUSTOM);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8, rcSegmented.left, rcSegmented.top, rcSegmented.right - rcSegmented.left, rcSegmented.bottom - rcSegmented.top, 245, 245, 245, 255);
    if (segHover) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8, rcSegmented.left, rcSegmented.top, rcSegmented.right - rcSegmented.left, rcSegmented.bottom - rcSegmented.top, 2, 180, 200, 220, 80);
    }

    // Three segments (equal width)
    int segW = (rcSegmented.right - rcSegmented.left) / 3;
    const wchar_t* segLabels[3] = {L"中文", L"英文", L"自定义"};
    for (int i = 0; i < 3; i++) {
        if (i == g_tempAudioType) {
            // Selected: blue pill
            int px = rcSegmented.left + i * segW + 2;
            int pw = segW - 4;
            BOOL isPressed = (i == 0 && g_pressedId == HIT_SEGMENT_CH) || (i == 1 && g_pressedId == HIT_SEGMENT_EN) || (i == 2 && g_pressedId == HIT_SEGMENT_CUSTOM);
            if (isPressed) {
                FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, px, rcSegmented.top + 2, pw, rcSegmented.bottom - rcSegmented.top - 4, 0, 80, 170, 255);
            } else {
                FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, px, rcSegmented.top + 2, pw, rcSegmented.bottom - rcSegmented.top - 4, 0, 103, 192, 255);
            }
            RECT rcSegText = {px + 10, rcSegmented.top, px + pw - 10, rcSegmented.bottom};
            DrawTextSDF(g_hdcBuffer, segLabels[i], &rcSegText, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontSmall, RGB(255, 255, 255));
        } else {
            // Not selected
            HitTestID thisHit = (i == 0) ? HIT_SEGMENT_CH : (i == 1 ? HIT_SEGMENT_EN : HIT_SEGMENT_CUSTOM);
            COLORREF textColor = (g_hoverId == thisHit) ? RGB(0, 103, 192) : RGB(30, 30, 30);
            RECT rcSegText = {rcSegmented.left + i * segW + 10, rcSegmented.top, rcSegmented.left + (i + 1) * segW - 10, rcSegmented.bottom};
            DrawTextSDF(g_hdcBuffer, segLabels[i], &rcSegText, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontSmall, textColor);
        }
    }

    // ---- Input: 音频文件路径 ----
    RECT rcInputLabel = {DLG_SHADOW + 50, DLG_SHADOW + 190, DLG_SHADOW + 250, DLG_SHADOW + 210};
    DrawTextSDF(g_hdcBuffer, L"音频文件路径", &rcInputLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontSmall, RGB(30, 30, 30));

    // Input box
    BOOL browseHover = (g_hoverId == HIT_BROWSE);
    BOOL browsePressed = (g_pressedId == HIT_BROWSE);
    BOOL inputEnabled = g_tempEnableAudio && g_tempAudioType == 2;

    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcInputBox.left, rcInputBox.top, rcInputBox.right - rcInputBox.left, rcInputBox.bottom - rcInputBox.top,
        inputEnabled ? 255 : 248, inputEnabled ? 255 : 248, inputEnabled ? 255 : 248, 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcInputBox.left, rcInputBox.top, rcInputBox.right - rcInputBox.left, rcInputBox.bottom - rcInputBox.top,
        2, inputEnabled ? 200 : 220, inputEnabled ? 200 : 220, inputEnabled ? 200 : 220, inputEnabled ? 80 : 40);

    // Path text
    const wchar_t* pathText = wcslen(g_tempCustomPath) > 0 ? g_tempCustomPath : L"Select a custom audio file...";
    COLORREF pathColor = (wcslen(g_tempCustomPath) > 0) ? RGB(30, 30, 30) : RGB(160, 160, 160);
    RECT rcPathText = {rcInputBox.left + 12, rcInputBox.top, rcInputBox.right - 12, rcInputBox.bottom};
    DrawTextSDF(g_hdcBuffer, pathText, &rcPathText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontSmall, pathColor);

    // Browse button
    BOOL btnHover = (g_hoverId == HIT_BROWSE);
    COLORREF browseBg = browsePressed ? RGB(200, 200, 205) : (btnHover ? RGB(230, 230, 235) : RGB(240, 240, 242));
    if (!inputEnabled) browseBg = RGB(248, 248, 248);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcBrowseBtn.left, rcBrowseBtn.top, rcBrowseBtn.right - rcBrowseBtn.left, rcBrowseBtn.bottom - rcBrowseBtn.top,
        GetRValue(browseBg), GetGValue(browseBg), GetBValue(browseBg), 255);
    if (inputEnabled) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcBrowseBtn.left, rcBrowseBtn.top, rcBrowseBtn.right - rcBrowseBtn.left, rcBrowseBtn.bottom - rcBrowseBtn.top,
            2, btnHover ? 150 : 200, btnHover ? 150 : 200, btnHover ? 150 : 200, 100);
    }

    // Folder icon + "浏览"
    RECT rcBrowseInner = {rcBrowseBtn.left + 8, rcBrowseBtn.top, rcBrowseBtn.right - 8, rcBrowseBtn.bottom};
    if (inputEnabled) {
        DrawTextSDF(g_hdcBuffer, L"\xE8B7", &rcBrowseInner, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontIcon, RGB(80, 80, 80));
    }
    RECT rcBrowseText = {rcBrowseBtn.left + 28, rcBrowseBtn.top, rcBrowseBtn.right - 8, rcBrowseBtn.bottom};
    if (inputEnabled) {
        DrawTextSDF(g_hdcBuffer, texts->browseAudio, &rcBrowseText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontSmall, RGB(80, 80, 80));
    }

    // ---- Action Buttons ----
    BOOL okHover = (g_hoverId == HIT_BTN_OK);
    BOOL okPressed = (g_pressedId == HIT_BTN_OK);
    COLORREF okBg = okPressed ? RGB(0, 70, 150) : (okHover ? RGB(0, 85, 170) : RGB(0, 103, 192));
    int okY = rcBtnOk.top + (okPressed ? 1 : 0);
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT, rcBtnOk.left, okY, rcBtnOk.right - rcBtnOk.left, rcBtnOk.bottom - rcBtnOk.top, (rcBtnOk.bottom - rcBtnOk.top) / 2, 12, 4, 0.2f);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, (rcBtnOk.bottom - rcBtnOk.top) / 2, (rcBtnOk.bottom - rcBtnOk.top) / 2, rcBtnOk.left, okY, rcBtnOk.right - rcBtnOk.left, rcBtnOk.bottom - rcBtnOk.top, GetRValue(okBg), GetGValue(okBg), GetBValue(okBg), 255);
    RECT rcOkText = {rcBtnOk.left, okY, rcBtnOk.right, okY + (rcBtnOk.bottom - rcBtnOk.top)};
    DrawTextSDF(g_hdcBuffer, texts->ok, &rcOkText, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(255, 255, 255));

    BOOL cancelHover = (g_hoverId == HIT_BTN_CANCEL);
    BOOL cancelPressed = (g_pressedId == HIT_BTN_CANCEL);
    COLORREF cancelBg = cancelPressed ? RGB(130, 130, 135) : (cancelHover ? RGB(145, 145, 150) : RGB(160, 160, 165));
    int cancelY = rcBtnCancel.top + (cancelPressed ? 1 : 0);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, (rcBtnCancel.bottom - rcBtnCancel.top) / 2, (rcBtnCancel.bottom - rcBtnCancel.top) / 2, rcBtnCancel.left, cancelY, rcBtnCancel.right - rcBtnCancel.left, rcBtnCancel.bottom - rcBtnCancel.top, GetRValue(cancelBg), GetGValue(cancelBg), GetBValue(cancelBg), 255);
    RECT rcCancelText = {rcBtnCancel.left, cancelY, rcBtnCancel.right, cancelY + (rcBtnCancel.bottom - rcBtnCancel.top)};
    DrawTextSDF(g_hdcBuffer, texts->cancel, &rcCancelText, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(255, 255, 255));

    DeleteObject(hFontLabel);
    DeleteObject(hFontSmall);
    DeleteObject(hFontBtn);
    DeleteObject(hFontIcon);
}

static void UpdateAudioLayeredWindow() {
    RenderDialogUI();
    HDC hdc = GetDC(NULL);
    POINT dst = {0, 0}, src = {0, 0};
    SIZE sz = {DLG_WIDTH, DLG_HEIGHT};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT rc; GetWindowRect(g_hAudioDialog, &rc);
    dst.x = rc.left; dst.y = rc.top;
    UpdateLayeredWindow(g_hAudioDialog, hdc, &dst, &sz, g_hdcBuffer, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, hdc);
}

static HitTestID HitTest(POINT pt) {
    // Toggle switch
    {
        int sw = 34, sh = 18;
        int cy = DLG_SHADOW + 55 + (rcToggleSwitch.bottom - rcToggleSwitch.top - sh) / 2;
        RECT rcSw = {rcToggleSwitch.left, cy, rcToggleSwitch.left + sw, cy + sh};
        if (PtInRect(&rcSw, pt)) return HIT_TOGGLE;
    }

    // Segmented control
    if (PtInRect(&rcSegmented, pt)) {
        int segW = (rcSegmented.right - rcSegmented.left) / 3;
        int idx = (pt.x - rcSegmented.left) / segW;
        if (idx == 0) return HIT_SEGMENT_CH;
        if (idx == 1) return HIT_SEGMENT_EN;
        return HIT_SEGMENT_CUSTOM;
    }

    // Browse button
    if (PtInRect(&rcBrowseBtn, pt)) return HIT_BROWSE;

    // Buttons
    if (PtInRect(&rcBtnOk, pt)) return HIT_BTN_OK;
    if (PtInRect(&rcBtnCancel, pt)) return HIT_BTN_CANCEL;

    // Card background
    RECT rcCard = {DLG_SHADOW, DLG_SHADOW, DLG_WIDTH - DLG_SHADOW, DLG_HEIGHT - DLG_SHADOW};
    if (PtInRect(&rcCard, pt)) return HIT_CARD_BG;

    return HIT_NONE;
}

// ============================================
// Window Procedure
// ============================================

static LRESULT CALLBACK ModernAudioDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hAudioDialog = hwnd;

            // Create DIB buffer
            HDC hdc = GetDC(NULL);
            g_hdcBuffer = CreateCompatibleDC(hdc);
            BITMAPINFOHEADER bi = {sizeof(bi), DLG_WIDTH, -DLG_HEIGHT, 1, 32, BI_RGB};
            g_hbmBuffer = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&g_pBits, NULL, 0);
            SelectObject(g_hdcBuffer, g_hbmBuffer);
            ReleaseDC(NULL, hdc);

            // Save original state
            g_originalEnableAudio = g_timerState.enableAudioAlert;
            g_originalAudioType = g_timerState.useCustomAudio ? 2 : (g_timerState.useChineseAudio ? 0 : 1);
            wcscpy_s(g_originalCustomPath, MAX_PATH, g_timerState.customAudioPath);

            // Init temp state
            g_tempEnableAudio = g_originalEnableAudio;
            g_tempAudioType = g_originalAudioType;
            wcscpy_s(g_tempCustomPath, MAX_PATH, g_originalCustomPath);

            UpdateAudioLayeredWindow();
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};

            // Dragging
            if (g_isDraggingDlg) {
                POINT cur; GetCursorPos(&cur);
                int dx = cur.x - g_dragStartScreen.x;
                int dy = cur.y - g_dragStartScreen.y;
                SetWindowPos(hwnd, NULL, g_dlgStartRect.left + dx, g_dlgStartRect.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }

            // Hover
            HitTestID hit = HitTest(pt);
            if (hit != g_hoverId) {
                g_hoverId = hit;
                UpdateAudioLayeredWindow();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);
            g_pressedId = hit;

            if (hit == HIT_CARD_BG) {
                g_isDraggingDlg = TRUE;
                GetCursorPos(&g_dragStartScreen);
                GetWindowRect(hwnd, &g_dlgStartRect);
                SetCapture(hwnd);
            } else if (hit == HIT_TOGGLE) {
                if (g_tempEnableAudio) {
                    // Don't allow turning off by clicking toggle (match old behavior: checkbox toggles)
                    // Actually let's allow toggle
                }
                g_tempEnableAudio = !g_tempEnableAudio;
                UpdateAudioLayeredWindow();
                return 0;
            } else if (hit == HIT_SEGMENT_CH || hit == HIT_SEGMENT_EN || hit == HIT_SEGMENT_CUSTOM) {
                int newType = (hit == HIT_SEGMENT_CH) ? 0 : (hit == HIT_SEGMENT_EN ? 1 : 2);
                g_tempAudioType = newType;
                UpdateAudioLayeredWindow();
                return 0;
            } else if (hit == HIT_BROWSE) {
                // Only allow when enabled + custom selected
                if (g_tempEnableAudio && g_tempAudioType == 2) {
                    OPENFILENAMEW ofn = {0};
                    wchar_t szFile[MAX_PATH] = {0};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
                    ofn.lpstrFilter = L"音频文件\0*.wav;*.mp3;*.aac;*.wma;*.ogg\0WAV文件\0*.wav\0MP3文件\0*.mp3\0所有文件\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameW(&ofn)) {
                        wcscpy_s(g_tempCustomPath, MAX_PATH, szFile);
                        UpdateAudioLayeredWindow();
                    }
                }
                return 0;
            } else if (hit == HIT_BTN_OK) {
                // Save & close
                g_timerState.enableAudioAlert = g_tempEnableAudio;
                g_timerState.useCustomAudio = (g_tempAudioType == 2);
                g_timerState.useChineseAudio = (g_tempAudioType == 0);
                wcscpy_s(g_timerState.customAudioPath, MAX_PATH, g_tempCustomPath);
                SaveAudioConfig();
                DestroyWindow(hwnd);
                return 0;
            } else if (hit == HIT_BTN_CANCEL) {
                // Restore original
                g_timerState.enableAudioAlert = g_originalEnableAudio;
                g_timerState.useCustomAudio = (g_originalAudioType == 2);
                g_timerState.useChineseAudio = (g_originalAudioType == 0);
                wcscpy_s(g_timerState.customAudioPath, MAX_PATH, g_originalCustomPath);
                SaveAudioConfig();
                DestroyWindow(hwnd);
                return 0;
            }

            UpdateAudioLayeredWindow();
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_isDraggingDlg) {
                g_isDraggingDlg = FALSE;
                ReleaseCapture();
            }
            g_pressedId = HIT_NONE;
            UpdateAudioLayeredWindow();
            return 0;
        }

        case WM_DESTROY: {
            if (g_hbmBuffer) DeleteObject(g_hbmBuffer);
            if (g_hdcBuffer) DeleteDC(g_hdcBuffer);
            g_hAudioDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================
// Show / Create
// ============================================

void ShowAudioDialog(void) {
    if (g_hAudioDialog && IsWindow(g_hAudioDialog)) {
        SetForegroundWindow(g_hAudioDialog);
        return;
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = ModernAudioDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ModernAudioDialogClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    RECT parentRect; GetWindowRect(g_timerState.hMainWnd, &parentRect);
    int parentHeight = parentRect.bottom - parentRect.top;

    int x = parentRect.right + 20;
    int y = parentRect.top + (parentHeight - DLG_HEIGHT) / 2;

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    if (x + DLG_WIDTH > workArea.right) x = parentRect.left - DLG_WIDTH - 20;
    if (x < workArea.left) x = workArea.left;
    if (y < workArea.top) y = workArea.top;
    if (x + DLG_WIDTH > workArea.right) x = workArea.right - DLG_WIDTH;
    if (y + DLG_HEIGHT > workArea.bottom) y = workArea.bottom - DLG_HEIGHT;

    g_hAudioDialog = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        L"ModernAudioDialogClass",
        L"",
        WS_POPUP,
        x, y, DLG_WIDTH, DLG_HEIGHT,
        g_timerState.hMainWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (g_hAudioDialog) {
        EnableWindow(g_timerState.hMainWnd, FALSE);
        ShowWindow(g_hAudioDialog, SW_SHOW);
        UpdateAudioLayeredWindow();
    }
}

void CreateAudioDialog() {
}
