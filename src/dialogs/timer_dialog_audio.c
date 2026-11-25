#include "timer_dialog_internal.h"
#include "timer_render_utils.h"
#include "timer_theme.h"
#include "timer_dialog_glass.h"
#include "ios_menu.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================
// Layout Constants
// ============================================
#define DLG_WIDTH  500
#define DLG_HEIGHT 380
#define DLG_SHADOW 24
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
static DialogGlass g_glass;       // 玻璃底：背景快照 + 预渲染底图

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

// 统一内容边距：所有行共用左右边界，避免各行右缘参差不齐
#define CONTENT_L   (DLG_SHADOW + 28)                 // 48
#define CONTENT_R   (DLG_WIDTH - DLG_SHADOW - 28)     // 452
#define BTN_H       36
#define BTN_W       96
#define BTN_GAP     10
#define BROWSE_W    100

// Segment layout
static RECT rcSegmented = {CONTENT_L, DLG_SHADOW + 124, CONTENT_R, DLG_SHADOW + 160};

// Toggle layout（开关右缘对齐 CONTENT_R）
static RECT rcToggleSwitch = {CONTENT_R - 34, DLG_SHADOW + 55, CONTENT_R, DLG_SHADOW + 73};

// Input layout（浏览按钮加宽到 100px，输入框相应缩短，二者留 10px 间隙）
static RECT rcBrowseBtn = {CONTENT_R - BROWSE_W, DLG_SHADOW + 210, CONTENT_R, DLG_SHADOW + 246};
static RECT rcInputBox  = {CONTENT_L, DLG_SHADOW + 210, CONTENT_R - BROWSE_W - 10, DLG_SHADOW + 246};

// Button layout（右对齐，底部留 24px）
static RECT rcBtnCancel = {CONTENT_R - BTN_W, DLG_SHADOW + 300, CONTENT_R, DLG_SHADOW + 300 + BTN_H};
static RECT rcBtnOk     = {CONTENT_R - BTN_W * 2 - BTN_GAP, DLG_SHADOW + 300, CONTENT_R - BTN_W - BTN_GAP, DLG_SHADOW + 300 + BTN_H};

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

static void RenderDialogUI() {
    memset(g_pBits, 0, DLG_WIDTH * DLG_HEIGHT * 4);
    const UiTheme* theme = Ui_GetTheme();

    // 面板：投影 + 玻璃底 + 描边（预先算好的底图，这里只拷贝）
    DialogGlass_Blit(&g_glass, g_pBits, DLG_WIDTH, DLG_HEIGHT);

    const MenuTexts* texts = GetMenuTexts();

    HFONT hFontLabel = CreateUiFont(15, FW_NORMAL, FALSE);
    HFONT hFontSmall  = CreateUiFont(13, FW_NORMAL, FALSE);
    HFONT hFontBtn    = CreateUiFont(14, FW_SEMIBOLD, FALSE);
    HFONT hFontIcon   = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, GetIconFontFamily());

    // ---- Toggle Switch ----
    // Label "启用音频提示"
    RECT rcToggleLabel = {CONTENT_L, DLG_SHADOW + 52, CONTENT_R - 60, DLG_SHADOW + 76};
    DrawTextSDF(g_hdcBuffer, texts->enableAudio, &rcToggleLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, hFontLabel, UI_LIGHT_TEXT_PRIMARY);
    // Switch control（主题化，右缘对齐 CONTENT_R）
    DrawSwitchThemed(g_pBits, DLG_WIDTH, DLG_HEIGHT, CONTENT_R - 38, DLG_SHADOW + 52, 24, g_tempEnableAudio, 96, theme);

    // ---- Segment: 音频语言 ----
    RECT rcSegLabel = {CONTENT_L, DLG_SHADOW + 100, CONTENT_R, DLG_SHADOW + 120};
    DrawTextSDF(g_hdcBuffer, texts->audioLanguage, &rcSegLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, hFontSmall, UI_LIGHT_TEXT_SECONDARY);

    // Segmented Control background
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8, rcSegmented.left, rcSegmented.top, rcSegmented.right - rcSegmented.left, rcSegmented.bottom - rcSegmented.top,
        GetRValue(theme->hover), GetGValue(theme->hover), GetBValue(theme->hover), theme->hoverAlpha);

    // Three segments (equal width)
    int segW = (rcSegmented.right - rcSegmented.left) / 3;
    BOOL english = (g_timerState.currentLanguage == TIMER_LANG_ENGLISH);
    const wchar_t* segLabels[3] = { english ? L"Chinese" : L"中文", english ? L"English" : L"英文", english ? L"Custom" : L"自定义" };
    for (int i = 0; i < 3; i++) {
        if (i == g_tempAudioType) {
            // Selected: 强调色实心
            int px = rcSegmented.left + i * segW + 2;
            int pw = segW - 4;
            BOOL isPressed = (i == 0 && g_pressedId == HIT_SEGMENT_CH) || (i == 1 && g_pressedId == HIT_SEGMENT_EN) || (i == 2 && g_pressedId == HIT_SEGMENT_CUSTOM);
            COLORREF segFill = isPressed ? theme->accentPressed : theme->accentFill;
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, px, rcSegmented.top + 2, pw, rcSegmented.bottom - rcSegmented.top - 4,
                GetRValue(segFill), GetGValue(segFill), GetBValue(segFill), 255);
            RECT rcSegText = {px + 10, rcSegmented.top, px + pw - 10, rcSegmented.bottom};
            DrawTextSDF(g_hdcBuffer, segLabels[i], &rcSegText, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontSmall, theme->onAccent);
        } else {
            // Not selected
            HitTestID thisHit = (i == 0) ? HIT_SEGMENT_CH : (i == 1 ? HIT_SEGMENT_EN : HIT_SEGMENT_CUSTOM);
            COLORREF textColor = (g_hoverId == thisHit) ? UI_PRIMARY_COLOR : UI_LIGHT_TEXT_PRIMARY;
            RECT rcSegText = {rcSegmented.left + i * segW + 10, rcSegmented.top, rcSegmented.left + (i + 1) * segW - 10, rcSegmented.bottom};
            DrawTextSDF(g_hdcBuffer, segLabels[i], &rcSegText, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontSmall, textColor);
        }
    }

    // ---- Input: 音频文件路径 ----
    RECT rcInputLabel = {CONTENT_L, DLG_SHADOW + 186, CONTENT_R, DLG_SHADOW + 206};
    DrawTextSDF(g_hdcBuffer, english ? L"Audio file path" : L"音频文件路径", &rcInputLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, hFontSmall, UI_LIGHT_TEXT_PRIMARY);

    // Input box
    BOOL browseHover = (g_hoverId == HIT_BROWSE);
    BOOL browsePressed = (g_pressedId == HIT_BROWSE);
    BOOL inputEnabled = g_tempEnableAudio && g_tempAudioType == 2;

    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcInputBox.left, rcInputBox.top, rcInputBox.right - rcInputBox.left, rcInputBox.bottom - rcInputBox.top,
        GetRValue(theme->field), GetGValue(theme->field), GetBValue(theme->field), inputEnabled ? theme->fieldAlpha : (BYTE)(theme->fieldAlpha / 2));
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcInputBox.left, rcInputBox.top, rcInputBox.right - rcInputBox.left, rcInputBox.bottom - rcInputBox.top,
        1, GetRValue(theme->ctrlLine), GetGValue(theme->ctrlLine), GetBValue(theme->ctrlLine), inputEnabled ? theme->ctrlLineAlpha : (BYTE)(theme->ctrlLineAlpha / 2));

    // Path text（长路径用路径省略号，保留文件名尾部）
    const wchar_t* pathText = wcslen(g_tempCustomPath) > 0 ? g_tempCustomPath : (english ? L"Select a custom audio file..." : L"选择自定义音频文件...");
    COLORREF pathColor = (wcslen(g_tempCustomPath) > 0) ? UI_LIGHT_TEXT_PRIMARY : UI_LIGHT_TEXT_DISABLED;
    RECT rcPathText = {rcInputBox.left + 12, rcInputBox.top, rcInputBox.right - 12, rcInputBox.bottom};
    DrawTextSDF(g_hdcBuffer, pathText, &rcPathText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX, hFontSmall, pathColor);

    // Browse button
    BOOL btnHover = (g_hoverId == HIT_BROWSE);
    COLORREF browseBg = browsePressed ? UI_LIGHT_BUTTON_PRESSED : (btnHover ? UI_LIGHT_BUTTON_HOVER : UI_LIGHT_BG_SECONDARY);
    if (!inputEnabled) browseBg = UI_LIGHT_BG_PRIMARY;
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcBrowseBtn.left, rcBrowseBtn.top, rcBrowseBtn.right - rcBrowseBtn.left, rcBrowseBtn.bottom - rcBrowseBtn.top,
        GetRValue(browseBg), GetGValue(browseBg), GetBValue(browseBg), 255);
    if (inputEnabled) {
        DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcBrowseBtn.left, rcBrowseBtn.top, rcBrowseBtn.right - rcBrowseBtn.left, rcBrowseBtn.bottom - rcBrowseBtn.top,
            1, GetRValue(theme->ctrlLine), GetGValue(theme->ctrlLine), GetBValue(theme->ctrlLine), btnHover ? (BYTE)min(255, theme->ctrlLineAlpha + 50) : theme->ctrlLineAlpha);
    }

    // Folder icon + "浏览"（按钮加宽后图标与文字都有充足空间）
    if (inputEnabled) {
        RECT rcBrowseIcon = {rcBrowseBtn.left + 12, rcBrowseBtn.top, rcBrowseBtn.left + 30, rcBrowseBtn.bottom};
        DrawTextSDF(g_hdcBuffer, L"\xE8B7", &rcBrowseIcon, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontIcon, UI_LIGHT_TEXT_SECONDARY);
        RECT rcBrowseText = {rcBrowseBtn.left + 32, rcBrowseBtn.top, rcBrowseBtn.right - 10, rcBrowseBtn.bottom};
        DrawTextSDF(g_hdcBuffer, texts->browseAudio, &rcBrowseText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, hFontSmall, UI_LIGHT_TEXT_SECONDARY);
    }

    // ---- Action Buttons ----
    BOOL okHover = (g_hoverId == HIT_BTN_OK);
    BOOL okPressed = (g_pressedId == HIT_BTN_OK);
    COLORREF okBg = okPressed ? UI_PRIMARY_PRESSED : (okHover ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR);
    GlowRoundedRect(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcBtnOk.left, rcBtnOk.top, rcBtnOk.right - rcBtnOk.left, rcBtnOk.bottom - rcBtnOk.top,
        GetRValue(okBg), GetGValue(okBg), GetBValue(okBg), theme->glow / 255.0f * 0.5f, 16);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6, rcBtnOk.left, rcBtnOk.top, rcBtnOk.right - rcBtnOk.left, rcBtnOk.bottom - rcBtnOk.top, GetRValue(okBg), GetGValue(okBg), GetBValue(okBg), 255);
    RECT rcOkText = rcBtnOk;
    DrawTextSDF(g_hdcBuffer, texts->ok, &rcOkText, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, theme->onAccent);

    BOOL cancelHover = (g_hoverId == HIT_BTN_CANCEL);
    BOOL cancelPressed = (g_pressedId == HIT_BTN_CANCEL);
    COLORREF cancelBg = cancelPressed ? UI_LIGHT_BUTTON_PRESSED : (cancelHover ? UI_LIGHT_BUTTON_HOVER : UI_LIGHT_BG_SECONDARY);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rcBtnCancel.left, rcBtnCancel.top, rcBtnCancel.right - rcBtnCancel.left, rcBtnCancel.bottom - rcBtnCancel.top, GetRValue(cancelBg), GetGValue(cancelBg), GetBValue(cancelBg), 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4, rcBtnCancel.left, rcBtnCancel.top, rcBtnCancel.right - rcBtnCancel.left, rcBtnCancel.bottom - rcBtnCancel.top,
        1, GetRValue(UI_LIGHT_BORDER), GetGValue(UI_LIGHT_BORDER), GetBValue(UI_LIGHT_BORDER), 255);
    RECT rcCancelText = rcBtnCancel;
    DrawTextSDF(g_hdcBuffer, texts->cancel, &rcCancelText, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, UI_LIGHT_TEXT_PRIMARY);

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

            // 窗口此刻尚未显示：捕获背后的屏幕做玻璃底图
            DialogGlass_Init(&g_glass, hwnd, DLG_WIDTH, DLG_HEIGHT, DLG_SHADOW, DLG_RADIUS);
            UpdateAudioLayeredWindow();
            return 0;
        }

        case WM_UI_THEME_CHANGED: {
            DialogGlass_Rebuild(&g_glass, hwnd);
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

            // 请求离开通知，鼠标移出窗口时清掉悬停态
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);

            // Hover
            HitTestID hit = HitTest(pt);
            if (hit != g_hoverId) {
                g_hoverId = hit;
                UpdateAudioLayeredWindow();
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hoverId != HIT_NONE) {
                g_hoverId = HIT_NONE;
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
                g_tempEnableAudio = !g_tempEnableAudio;
                UpdateAudioLayeredWindow();
                return 0;
            } else if (hit == HIT_SEGMENT_CH || hit == HIT_SEGMENT_EN || hit == HIT_SEGMENT_CUSTOM) {
                g_tempAudioType = (hit == HIT_SEGMENT_CH) ? 0 : (hit == HIT_SEGMENT_EN ? 1 : 2);
                UpdateAudioLayeredWindow();
                return 0;
            } else if (hit == HIT_BROWSE || hit == HIT_BTN_OK || hit == HIT_BTN_CANCEL) {
                // 动作按钮：按下只记录并捕获，抬起命中同一按钮才执行（可拖出取消）
                SetCapture(hwnd);
                UpdateAudioLayeredWindow();
                return 0;
            }

            UpdateAudioLayeredWindow();
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_isDraggingDlg) {
                g_isDraggingDlg = FALSE;
                ReleaseCapture();
                g_pressedId = HIT_NONE;
                DialogGlass_Rebuild(&g_glass, hwnd);   // 拖动结束：玻璃底对齐到新位置背后的画面
                UpdateAudioLayeredWindow();
                return 0;
            }

            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);
            HitTestID pressed = g_pressedId;
            g_pressedId = HIT_NONE;
            if (GetCapture() == hwnd) ReleaseCapture();

            // 仅当抬起仍命中按下的那个动作按钮时才执行
            if (hit == pressed) {
                if (pressed == HIT_BROWSE) {
                    if (g_tempEnableAudio && g_tempAudioType == 2) {
                        OPENFILENAMEW ofn = {0};
                        wchar_t szFile[MAX_PATH] = {0};
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = hwnd;
                        ofn.lpstrFile = szFile;
                        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
                        ofn.lpstrFilter = L"音频文件\0*.wav;*.mp3;*.aac;*.wma;*.ogg\0WAV文件\0*.wav\0MP3文件\0*.mp3\0所有文件\0*.*\0";
                        ofn.nFilterIndex = 1;
                        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                        if (GetOpenFileNameW(&ofn)) {
                            wcscpy_s(g_tempCustomPath, MAX_PATH, szFile);
                        }
                    }
                } else if (pressed == HIT_BTN_OK) {
                    g_timerState.enableAudioAlert = g_tempEnableAudio;
                    g_timerState.useCustomAudio = (g_tempAudioType == 2);
                    g_timerState.useChineseAudio = (g_tempAudioType == 0);
                    wcscpy_s(g_timerState.customAudioPath, MAX_PATH, g_tempCustomPath);
                    SaveAudioConfig();
                    DestroyWindow(hwnd);
                    return 0;
                } else if (pressed == HIT_BTN_CANCEL) {
                    // 临时态从未写入全局，取消只需关闭，无需回写磁盘
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            UpdateAudioLayeredWindow();
            return 0;
        }

        case WM_CAPTURECHANGED: {
            // 捕获被夺走（如弹出文件对话框）：清掉按下/拖拽态，避免残留
            g_isDraggingDlg = FALSE;
            g_pressedId = HIT_NONE;
            UpdateAudioLayeredWindow();
            return 0;
        }

        case WM_DESTROY: {
            if (g_hbmBuffer) DeleteObject(g_hbmBuffer);
            if (g_hdcBuffer) DeleteDC(g_hdcBuffer);
            DialogGlass_Destroy(&g_glass);
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
