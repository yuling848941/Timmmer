#include "timer_dialog_internal.h"
#include "timer_render_utils.h"
#include "timer_buffer.h"
#include <uxtheme.h>

#define DLG_WIDTH  480
#define DLG_HEIGHT 504
#define DLG_SHADOW 30
#define DLG_RADIUS 12

// Layout rects (relative to window, already include shadow offset)
static RECT rcCard       = {DLG_SHADOW, DLG_SHADOW, DLG_WIDTH - DLG_SHADOW, DLG_HEIGHT - DLG_SHADOW};
static RECT rcInput      = {DLG_SHADOW + 20, DLG_SHADOW + 90, DLG_SHADOW + 350, DLG_SHADOW + 126};
static RECT rcBtnAdd     = {DLG_SHADOW + 360, DLG_SHADOW + 90, DLG_SHADOW + 420, DLG_SHADOW + 126};
static RECT rcListArea   = {DLG_SHADOW + 20, DLG_SHADOW + 194, DLG_SHADOW + 400, DLG_SHADOW + 400};
static RECT rcBtnOK      = {DLG_SHADOW + 240, DLG_SHADOW + 410, DLG_SHADOW + 320, DLG_SHADOW + 444};
static RECT rcBtnCancel  = {DLG_SHADOW + 330, DLG_SHADOW + 410, DLG_SHADOW + 400, DLG_SHADOW + 444};
static RECT rcScrollbar  = {DLG_SHADOW + 392, DLG_SHADOW + 194, DLG_SHADOW + 400, DLG_SHADOW + 400};

#define ITEM_HEIGHT  36
#define ITEM_GAP     4
#define ITEM_CONTENT_W  336  // list area width minus scrollbar and padding
#define ICON_SIZE    16
#define SCROLLBAR_W  8
#define MAX_PRESETS  20

static wchar_t g_inputBuf[16] = {0};
static BOOL g_inputFocused = FALSE;
static BOOL g_cursorVisible = FALSE;
static BOOL g_showToast = FALSE;
static DWORD g_toastStart = 0;
static const DWORD TOAST_DURATION = 2000; // 2 seconds

// Backup for Cancel
static int g_origPresetCount = 0;
static int g_origPresets[MAX_PRESETS] = {0};

// Forward declarations
static void UpdatePresetWindow(void);
static void RefreshList(void);

// Draw text with proper alpha compositing for layered windows
static void DrawTextSDF(HDC hdc, const wchar_t* text, RECT* rc, int format, HFONT hFont, COLORREF color) {
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    HTHEME hTheme = OpenThemeData(NULL, L"WINDOW");
    if (hTheme) {
        DTTOPTS dttOpts = {sizeof(DTTOPTS)};
        dttOpts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;
        dttOpts.crText = color;
        DrawThemeTextEx(hTheme, hdc, 0, 0, text, -1, format, rc, &dttOpts);
        CloseThemeData(hTheme);
    } else {
        // Fallback: regular GDI text (less pretty but won't crash)
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, color);
        DrawTextW(hdc, text, -1, rc, format);
    }
    SelectObject(hdc, hOldFont);
}

typedef enum {
    HIT_NONE,
    HIT_TITLE_BAR,
    HIT_INPUT,
    HIT_BTN_ADD,
    HIT_ITEM_EDIT,
    HIT_ITEM_DELETE,
    HIT_SCROLLBAR,
    HIT_BTN_OK,
    HIT_BTN_CANCEL
} HitTestID;

static HWND g_hPresetDialog = NULL;
static HDC  g_hdcBuffer = NULL;
static HBITMAP g_hbmBuffer = NULL;
static BYTE* g_pBits = NULL;

static int g_hoverRow = -1;
static int g_pressedId = HIT_NONE;
static BOOL g_draggingScrollbar = FALSE;
static BOOL g_draggingDlg = FALSE;
static POINT g_dragStartScreen;
static RECT  g_dlgStartRect;

static int g_scrollOffset = 0;
static int g_editingIndex = -1;
static HWND g_hEditCtrl = NULL;
static int g_editOriginalMinutes = 0;

static int GetVisibleCount(void) {
    int h = rcListArea.bottom - rcListArea.top;
    return h / (ITEM_HEIGHT + ITEM_GAP);
}

static int GetMaxScroll(void) {
    int total = g_timerState.presetCount;
    int vis = GetVisibleCount();
    return (total > vis) ? (total - vis) : 0;
}

// -----------------------------------------------------------
// Subclass: inline edit control (handle Enter/Escape)
// -----------------------------------------------------------
static WNDPROC g_origEditProc = NULL;
static LRESULT CALLBACK InlineEditSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CHAR) {
        if (wParam == VK_RETURN) {
            wchar_t buf[16];
            GetWindowTextW(hwnd, buf, 16);
            int minutes = _wtoi(buf);
            if (minutes > 0 && minutes <= 999) {
                ModifyPresetTime(g_editingIndex, minutes);
            }
            DestroyWindow(hwnd);
            g_hEditCtrl = NULL;
            g_editingIndex = -1;
            SetFocus(g_hPresetDialog);
            RefreshList();
            return 0;
        } else if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            g_hEditCtrl = NULL;
            g_editingIndex = -1;
            SetFocus(g_hPresetDialog);
            UpdatePresetWindow();
            return 0;
        }
    }
    return CallWindowProcW(g_origEditProc, hwnd, msg, wParam, lParam);
}

// -----------------------------------------------------------
// Draw pencil/edit icon (16x16) at (x, y)
// Simple bold pencil: thick diagonal body + tip
// -----------------------------------------------------------
static void DrawPencilIcon(int x, int y) {
    COLORREF c = UI_PRIMARY_COLOR;
    BYTE r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);

    // Thick diagonal shaft: use small filled rects along the diagonal
    // From (x+3, y+2) to (x+10, y+9) — a bold diagonal bar
    // Draw as a parallelogram using AA fills
    // Top-left to bottom-right diagonal
    for (int i = 0; i < 9; i++) {
        int px = x + 3 + i;
        int py = y + 2 + i;
        // 2-pixel wide bar at each step
        for (int dx = 0; dx <= 2; dx++) {
            for (int dy = 0; dy <= 2; dy++) {
                int ix = px + dx;
                int iy = py + dy;
                int idx = iy * DLG_WIDTH + ix;
                if (idx >= 0 && idx < DLG_WIDTH * DLG_HEIGHT) {
                    g_pBits[idx * 4]     = b;
                    g_pBits[idx * 4 + 1] = g;
                    g_pBits[idx * 4 + 2] = r;
                    g_pBits[idx * 4 + 3] = 255;
                }
            }
        }
    }
    // Tip: small dark triangle at bottom-left of the shaft
    // Draw a small filled rect for the tip
    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 0; dx < 4; dx++) {
            int ix = x + 2 + dx;
            int iy = y + 11 + dy;
            int idx = iy * DLG_WIDTH + ix;
            if (idx >= 0 && idx < DLG_WIDTH * DLG_HEIGHT) {
                g_pBits[idx * 4]     = b;
                g_pBits[idx * 4 + 1] = g;
                g_pBits[idx * 4 + 2] = r;
                g_pBits[idx * 4 + 3] = 255;
            }
        }
    }
    // Eraser: pink rect at top-right
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 3, 3, x + 9, y + 1, 6, 4, 210, 130, 130, 255);
}

// -----------------------------------------------------------
// Draw trash can icon (16x16) at (x, y)
// -----------------------------------------------------------
static void DrawTrashIcon(int x, int y) {
    COLORREF c = UI_PRIMARY_COLOR;
    BYTE r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);

    // Body: rectangle
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 2, 2, x + 3, y + 5, 10, 10, r, g, b, 255);
    // Lid: horizontal bar
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 2, 2, x + 2, y + 3, 12, 3, r, g, b, 255);
    // Handle on top of lid
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 2, 2, x + 5, y + 1, 6, 3, 2, r, g, b, 255);
    // Two vertical lines inside body (slats)
    int sx = x + 6;
    for (int yy = y + 7; yy < y + 14; yy++) {
        for (int xx = sx - 1; xx <= sx + 1; xx++) {
            int idx = yy * DLG_WIDTH + xx;
            if (idx >= 0 && idx < DLG_WIDTH * DLG_HEIGHT) {
                g_pBits[idx * 4] = b; g_pBits[idx * 4 + 1] = g;
                g_pBits[idx * 4 + 2] = r; g_pBits[idx * 4 + 3] = 255;
            }
        }
    }
    sx = x + 9;
    for (int yy = y + 7; yy < y + 14; yy++) {
        for (int xx = sx - 1; xx <= sx + 1; xx++) {
            int idx = yy * DLG_WIDTH + xx;
            if (idx >= 0 && idx < DLG_WIDTH * DLG_HEIGHT) {
                g_pBits[idx * 4] = b; g_pBits[idx * 4 + 1] = g;
                g_pBits[idx * 4 + 2] = r; g_pBits[idx * 4 + 3] = 255;
            }
        }
    }
}

// -----------------------------------------------------------
// Render dialog UI to off-screen buffer
// -----------------------------------------------------------
static void RenderPresetDialog(void) {
    if (!g_pBits) return;
    memset(g_pBits, 0, DLG_WIDTH * DLG_HEIGHT * 4);

    // Shadow + card
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT,
        rcCard.left, rcCard.top,
        rcCard.right - rcCard.left, rcCard.bottom - rcCard.top,
        DLG_RADIUS, DLG_SHADOW, 4, 0.15f);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT,
        DLG_RADIUS, DLG_RADIUS,
        rcCard.left, rcCard.top,
        rcCard.right - rcCard.left, rcCard.bottom - rcCard.top,
        250, 250, 252, 255);

    const MenuTexts* texts = GetMenuTexts();

    HFONT hFontTitle = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontLabel = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontBtn   = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontItem  = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT hFontHint  = CreateFontW(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    // ---- Title ----
    RECT rcTitle = {rcCard.left + 20, rcCard.top + 20, rcCard.right - 20, rcCard.top + 55};
    DrawTextSDF(g_hdcBuffer, texts->presetEditTitle, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontTitle, RGB(30, 30, 30));

    // ---- New Preset label ----
    RECT rcNPLabel = {rcInput.left, rcInput.top - 22, rcInput.right, rcInput.top};
    DrawTextSDF(g_hdcBuffer, texts->newPreset, &rcNPLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(60, 60, 60));

    // ---- Input field ----
    BOOL inputHover = (g_pressedId == HIT_INPUT);
    COLORREF inputBorder = g_inputFocused ? UI_PRIMARY_COLOR : (inputHover ? UI_PRIMARY_HOVER : RGB(80, 100, 120));
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8,
        rcInput.left, rcInput.top, rcInput.right - rcInput.left, rcInput.bottom - rcInput.top,
        255, 255, 255, 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8,
        rcInput.left, rcInput.top, rcInput.right - rcInput.left, rcInput.bottom - rcInput.top,
        g_inputFocused ? 2 : 1, GetRValue(inputBorder), GetGValue(inputBorder), GetBValue(inputBorder), 255);

    // Input text
    if (g_inputBuf[0] != L'\0') {
        RECT rcInputText = {rcInput.left + 12, rcInput.top + 2, rcInput.right - 12, rcInput.bottom - 2};
        DrawTextSDF(g_hdcBuffer, g_inputBuf, &rcInputText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(28, 28, 30));
    } else if (g_inputFocused) {
        // Show placeholder when empty and focused
        RECT rcInputText = {rcInput.left + 12, rcInput.top + 2, rcInput.right - 12, rcInput.bottom - 2};
        DrawTextSDF(g_hdcBuffer, L"输入分钟数...", &rcInputText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(180, 180, 185));
    }

    // Cursor
    if (g_inputFocused && g_cursorVisible && g_inputBuf[0] != L'\0') {
        HFONT hOldFont = (HFONT)SelectObject(g_hdcBuffer, hFontLabel);
        SIZE sz;
        GetTextExtentPoint32W(g_hdcBuffer, g_inputBuf, (int)wcslen(g_inputBuf), &sz);
        SelectObject(g_hdcBuffer, hOldFont);
        int cursorX = rcInput.left + 12 + sz.cx;
        int cursorY = rcInput.top + 5;
        int cursorH = rcInput.bottom - rcInput.top - 10;
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 1, 1, cursorX, cursorY, 2, cursorH, UI_PRIMARY_COLOR & 0xFF, (UI_PRIMARY_COLOR >> 8) & 0xFF, (UI_PRIMARY_COLOR >> 16) & 0xFF, 255);
    }

    // ---- Add button ----
    BOOL addHover = (g_pressedId == HIT_BTN_ADD);
    COLORREF addFill = addHover ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR;
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8,
        rcBtnAdd.left, rcBtnAdd.top, rcBtnAdd.right - rcBtnAdd.left, rcBtnAdd.bottom - rcBtnAdd.top,
        GetRValue(addFill), GetGValue(addFill), GetBValue(addFill), 255);
    RECT rcAddT = rcBtnAdd;
    DrawTextSDF(g_hdcBuffer, texts->addPreset, &rcAddT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(255, 255, 255));

    // ---- Preset List label ----
    RECT rcPLLabel = {rcListArea.left, rcListArea.top - 24, rcListArea.right, rcListArea.top};
    DrawTextSDF(g_hdcBuffer, texts->presetList, &rcPLLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, RGB(60, 60, 60));

    // ---- List container ----
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8,
        rcListArea.left, rcListArea.top, rcListArea.right - rcListArea.left, rcListArea.bottom - rcListArea.top,
        255, 255, 255, 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 8, 8,
        rcListArea.left, rcListArea.top, rcListArea.right - rcListArea.left, rcListArea.bottom - rcListArea.top,
        1, 0, 0, 0, 30);

    // ---- Draw list items ----
    int visCount = GetVisibleCount();
    int maxScroll = GetMaxScroll();
    int contentW = rcListArea.right - rcListArea.left - SCROLLBAR_W - 8;
    int iconX = rcListArea.left + 8 + contentW - ICON_SIZE - 8 - ICON_SIZE - 8;  // trash X

    for (int i = 0; i < visCount && (g_scrollOffset + i) < g_timerState.presetCount; i++) {
        int idx = g_scrollOffset + i;
        int itemY = rcListArea.top + 8 + i * (ITEM_HEIGHT + ITEM_GAP);

        BOOL isHover = (g_hoverRow == idx);
        BOOL isEditing = (g_editingIndex == idx);

        // Item background
        if (isHover && !isEditing) {
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6,
                rcListArea.left + 4, itemY, contentW, ITEM_HEIGHT,
                220, 235, 248, 255);
        } else if (!isEditing) {
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 6, 6,
                rcListArea.left + 4, itemY, contentW, ITEM_HEIGHT,
                250, 250, 252, 255);
        }

        // Item text
        wchar_t itemText[32];
        swprintf(itemText, 32, L"%d 分钟", g_timerState.presetTimes[idx]);
        RECT rcItemText = {rcListArea.left + 14, itemY, rcListArea.left + 14 + contentW - ICON_SIZE * 2 - 24, itemY + ITEM_HEIGHT};
        DrawTextSDF(g_hdcBuffer, itemText, &rcItemText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontItem, RGB(40, 40, 40));

        // Icons (only when not editing this row)
        if (!isEditing) {
            int iconY = itemY + (ITEM_HEIGHT - ICON_SIZE) / 2;
            int pencilX = rcListArea.left + 8 + contentW - ICON_SIZE - 4 - ICON_SIZE - 4;
            DrawPencilIcon(pencilX, iconY);
            DrawTrashIcon(pencilX + ICON_SIZE + 8, iconY);
        }
    }

    // ---- Scrollbar ----
    if (maxScroll > 0) {
        int sbTrackH = rcScrollbar.bottom - rcScrollbar.top;
        int thumbH = max(20, (int)(sbTrackH * (float)visCount / g_timerState.presetCount));
        int thumbRange = sbTrackH - thumbH;
        int thumbY = rcScrollbar.top + (maxScroll > 0 ? (g_scrollOffset * thumbRange / maxScroll) : 0);

        // Track
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
            rcScrollbar.left, rcScrollbar.top, SCROLLBAR_W, sbTrackH,
            230, 230, 235, 255);
        // Thumb
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
            rcScrollbar.left, thumbY, SCROLLBAR_W, thumbH,
            160, 160, 165, 255);
    }

    // ---- OK button ----
    BOOL okHover = (g_pressedId == HIT_BTN_OK);
    COLORREF okFill = okHover ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR;
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT,
        rcBtnOK.left, rcBtnOK.top, rcBtnOK.right - rcBtnOK.left, rcBtnOK.bottom - rcBtnOK.top,
        (rcBtnOK.bottom - rcBtnOK.top) / 2, 12, 3, 0.15f);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT,
        (rcBtnOK.bottom - rcBtnOK.top) / 2, (rcBtnOK.bottom - rcBtnOK.top) / 2,
        rcBtnOK.left, rcBtnOK.top, rcBtnOK.right - rcBtnOK.left, rcBtnOK.bottom - rcBtnOK.top,
        GetRValue(okFill), GetGValue(okFill), GetBValue(okFill), 255);
    RECT rcOKT = rcBtnOK;
    DrawTextSDF(g_hdcBuffer, texts->ok, &rcOKT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(255, 255, 255));

    // ---- Cancel button ----
    BOOL cancelHover = (g_pressedId == HIT_BTN_CANCEL);
    COLORREF cancelFill = cancelHover ? RGB(200, 200, 205) : RGB(180, 180, 185);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT,
        (rcBtnCancel.bottom - rcBtnCancel.top) / 2, (rcBtnCancel.bottom - rcBtnCancel.top) / 2,
        rcBtnCancel.left, rcBtnCancel.top, rcBtnCancel.right - rcBtnCancel.left, rcBtnCancel.bottom - rcBtnCancel.top,
        GetRValue(cancelFill), GetGValue(cancelFill), GetBValue(cancelFill), 255);
    RECT rcCancelT = rcBtnCancel;
    DrawTextSDF(g_hdcBuffer, texts->cancel, &rcCancelT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(60, 60, 60));

    // ---- Hint text ----
    RECT rcHint = {rcCard.left + 20, rcBtnOK.bottom + 8, rcBtnOK.left - 10, rcCard.bottom - 10};
    DrawTextSDF(g_hdcBuffer, texts->doubleClickHint, &rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontHint, RGB(140, 140, 145));

    // ---- Inline error message below input ----
    if (g_showToast) {
        RECT rcErr = {rcInput.left, rcInput.bottom + 6, rcInput.right, rcInput.bottom + 28};
        DrawTextSDF(g_hdcBuffer, L"预设时间不能超过 60 分钟", &rcErr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontHint, RGB(200, 50, 50));
    }

    DeleteObject(hFontTitle);
    DeleteObject(hFontLabel);
    DeleteObject(hFontBtn);
    DeleteObject(hFontItem);
    DeleteObject(hFontHint);
}

// -----------------------------------------------------------
// Update layered window
// -----------------------------------------------------------
static void UpdatePresetWindow(void) {
    RenderPresetDialog();
    HDC hdc = GetDC(NULL);
    POINT dst = {0, 0}, src = {0, 0};
    SIZE sz = {DLG_WIDTH, DLG_HEIGHT};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT rc; GetWindowRect(g_hPresetDialog, &rc);
    dst.x = rc.left; dst.y = rc.top;
    UpdateLayeredWindow(g_hPresetDialog, hdc, &dst, &sz, g_hdcBuffer, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, hdc);
}

// -----------------------------------------------------------
// Hit test
// -----------------------------------------------------------
static HitTestID HitTest(POINT pt) {
    // Title bar (top 35px of card)
    RECT rcTitleBar = {rcCard.left, rcCard.top, rcCard.right, rcCard.top + 35};
    if (PtInRect(&rcTitleBar, pt)) return HIT_TITLE_BAR;

    // Input
    if (PtInRect(&rcInput, pt)) return HIT_INPUT;

    // Add button
    if (PtInRect(&rcBtnAdd, pt)) return HIT_BTN_ADD;

    // List items
    int visCount = GetVisibleCount();
    int maxScroll = GetMaxScroll();
    int contentW = rcListArea.right - rcListArea.left - SCROLLBAR_W - 8;

    for (int i = 0; i < visCount && (g_scrollOffset + i) < g_timerState.presetCount; i++) {
        int idx = g_scrollOffset + i;
        int itemY = rcListArea.top + 8 + i * (ITEM_HEIGHT + ITEM_GAP);
        RECT rcItem = {rcListArea.left + 4, itemY, rcListArea.left + 4 + contentW, itemY + ITEM_HEIGHT};

        if (PtInRect(&rcItem, pt)) {
            int iconY = itemY + (ITEM_HEIGHT - ICON_SIZE) / 2;
            int pencilX = rcListArea.left + 8 + contentW - ICON_SIZE - 4 - ICON_SIZE - 4;

            // Pencil hit area
            RECT rcPencil = {pencilX - 2, iconY - 2, pencilX + ICON_SIZE + 2, iconY + ICON_SIZE + 2};
            if (PtInRect(&rcPencil, pt)) return HIT_ITEM_EDIT;

            // Trash hit area
            int trashX = pencilX + ICON_SIZE + 8;
            RECT rcTrash = {trashX - 2, iconY - 2, trashX + ICON_SIZE + 2, iconY + ICON_SIZE + 2};
            if (PtInRect(&rcTrash, pt)) return HIT_ITEM_DELETE;
        }
    }

    // Scrollbar
    if (maxScroll > 0 && PtInRect(&rcScrollbar, pt)) return HIT_SCROLLBAR;

    // OK
    if (PtInRect(&rcBtnOK, pt)) return HIT_BTN_OK;

    // Cancel
    if (PtInRect(&rcBtnCancel, pt)) return HIT_BTN_CANCEL;

    return HIT_NONE;
}

// -----------------------------------------------------------
// Refresh list display
// -----------------------------------------------------------
static void RefreshList(void) {
    int maxScroll = GetMaxScroll();
    if (g_scrollOffset > maxScroll) g_scrollOffset = maxScroll;
    UpdatePresetWindow();
}

// -----------------------------------------------------------
// Window procedure
// -----------------------------------------------------------
LRESULT CALLBACK PresetEditDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hPresetDialog = hwnd;
            HDC hdc = GetDC(NULL);
            g_hdcBuffer = CreateCompatibleDC(hdc);
            BITMAPINFOHEADER bi = {sizeof(bi), DLG_WIDTH, -DLG_HEIGHT, 1, 32, BI_RGB};
            g_hbmBuffer = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&g_pBits, NULL, 0);
            SelectObject(g_hdcBuffer, g_hbmBuffer);
            ReleaseDC(NULL, hdc);

            g_scrollOffset = 0;
            g_hoverRow = -1;
            g_editingIndex = -1;
            g_hEditCtrl = NULL;

            // Backup presets for Cancel
            g_origPresetCount = g_timerState.presetCount;
            for (int i = 0; i < g_origPresetCount && i < MAX_PRESETS; i++) {
                g_origPresets[i] = g_timerState.presetTimes[i];
            }

            UpdatePresetWindow();
            SetTimer(hwnd, 2, 100, NULL);  // Main window refresh
            SetTimer(hwnd, 3, 500, NULL);  // Cursor blink
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 2 && g_timerState.hMainWnd && IsWindow(g_timerState.hMainWnd)) {
                if (g_timerState.transparentBackground) {
                    UpdateLayeredWindow_Render();
                } else {
                    InvalidateRect(g_timerState.hMainWnd, NULL, FALSE);
                    UpdateWindow(g_timerState.hMainWnd);
                }
            } else if (wParam == 3) {
                // Cursor blink
                if (g_inputFocused) {
                    g_cursorVisible = !g_cursorVisible;
                }
                // Toast timeout
                if (g_showToast && GetTickCount() - g_toastStart > TOAST_DURATION) {
                    g_showToast = FALSE;
                }
                UpdatePresetWindow();
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};

            if (g_draggingScrollbar) {
                int sbTrackH = rcScrollbar.bottom - rcScrollbar.top;
                int thumbH = max(20, (int)(sbTrackH * (float)GetVisibleCount() / max(g_timerState.presetCount, 1)));
                int thumbRange = sbTrackH - thumbH;
                int relY = pt.y - rcScrollbar.top - thumbH / 2;
                if (relY < 0) relY = 0;
                if (relY > thumbRange) relY = thumbRange;
                int maxScroll = GetMaxScroll();
                g_scrollOffset = (thumbRange > 0) ? (relY * maxScroll / thumbRange) : 0;
                if (g_scrollOffset > maxScroll) g_scrollOffset = maxScroll;
                UpdatePresetWindow();
                return 0;
            }

            if (g_draggingDlg) {
                POINT cur; GetCursorPos(&cur);
                int dx = cur.x - g_dragStartScreen.x;
                int dy = cur.y - g_dragStartScreen.y;
                SetWindowPos(hwnd, NULL, g_dlgStartRect.left + dx, g_dlgStartRect.top + dy,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }

            // Hover detection
            int prevHover = g_hoverRow;
            g_hoverRow = -1;

            int visCount = GetVisibleCount();
            int contentW = rcListArea.right - rcListArea.left - SCROLLBAR_W - 8;

            for (int i = 0; i < visCount && (g_scrollOffset + i) < g_timerState.presetCount; i++) {
                int idx = g_scrollOffset + i;
                int itemY = rcListArea.top + 8 + i * (ITEM_HEIGHT + ITEM_GAP);
                RECT rcItem = {rcListArea.left + 4, itemY, rcListArea.left + 4 + contentW, itemY + ITEM_HEIGHT};
                if (PtInRect(&rcItem, pt)) {
                    g_hoverRow = idx;
                    break;
                }
            }

            if (g_hoverRow != prevHover) {
                UpdatePresetWindow();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            // Dismiss toast on any click
            g_showToast = FALSE;

            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);
            g_pressedId = hit;

            if (hit == HIT_SCROLLBAR && GetMaxScroll() > 0) {
                g_draggingScrollbar = TRUE;
                SetCapture(hwnd);
            } else if (hit == HIT_TITLE_BAR) {
                g_draggingDlg = TRUE;
                GetCursorPos(&g_dragStartScreen);
                GetWindowRect(hwnd, &g_dlgStartRect);
                SetCapture(hwnd);
            } else if (hit == HIT_INPUT) {
                g_inputFocused = TRUE;
                g_cursorVisible = TRUE;
            }

            UpdatePresetWindow();
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_draggingScrollbar) { g_draggingScrollbar = FALSE; ReleaseCapture(); }
            if (g_draggingDlg) { g_draggingDlg = FALSE; ReleaseCapture(); }

            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);

            if (hit == g_pressedId && hit != HIT_NONE) {
                if (hit == HIT_BTN_ADD) {
                    int minutes = _wtoi(g_inputBuf);
                    if (minutes > 0 && minutes <= 60) {
                        AddPresetTime(minutes);
                        g_inputBuf[0] = L'\0';
                        RefreshList();
                        SetFocus(hwnd);
                    } else if (minutes > 60) {
                        // Value exceeds 60 — show error
                        g_showToast = TRUE;
                        g_toastStart = GetTickCount();
                    }
                    // Empty input or 0: silently do nothing
                } else if (hit == HIT_ITEM_DELETE) {
                    if (g_hoverRow >= 0 && g_hoverRow < g_timerState.presetCount) {
                        DeletePresetTime(g_hoverRow);
                        RefreshList();
                    }
                } else if (hit == HIT_ITEM_EDIT) {
                    if (g_hoverRow >= 0 && g_hoverRow < g_timerState.presetCount) {
                        // Clean up any stale inline edit
                        if (g_hEditCtrl && IsWindow(g_hEditCtrl)) {
                            DestroyWindow(g_hEditCtrl);
                        }
                        g_hEditCtrl = NULL;
                        g_editingIndex = g_hoverRow;
                        g_editOriginalMinutes = g_timerState.presetTimes[g_hoverRow];

                        // Create temporary EDIT control
                        int visCount = GetVisibleCount();
                        int contentW = rcListArea.right - rcListArea.left - SCROLLBAR_W - 8;
                        int rowIdx = g_hoverRow - g_scrollOffset;
                        int itemY = rcListArea.top + 8 + rowIdx * (ITEM_HEIGHT + ITEM_GAP);

                        RECT rcClient; GetClientRect(hwnd, &rcClient);
                        RECT rcEdit = {rcListArea.left + 14, itemY + 2, rcListArea.left + 14 + contentW - ICON_SIZE * 2 - 24, itemY + ITEM_HEIGHT - 2};

                        g_hEditCtrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
                            rcEdit.left, rcEdit.top, rcEdit.right - rcEdit.left, rcEdit.bottom - rcEdit.top,
                            hwnd, NULL, GetModuleHandle(NULL), NULL);

                        if (g_hEditCtrl) {
                            wchar_t buf[16];
                            swprintf(buf, 16, L"%d", g_timerState.presetTimes[g_hoverRow]);
                            SetWindowTextW(g_hEditCtrl, buf);
                            SendMessageW(g_hEditCtrl, EM_SETSEL, 0, -1);
                            g_origEditProc = (WNDPROC)SetWindowLongPtrW(g_hEditCtrl, GWLP_WNDPROC, (LONG_PTR)InlineEditSubclass);
                            SetFocus(g_hEditCtrl);
                        } else {
                            g_editingIndex = -1;
                        }

                        UpdatePresetWindow();
                    }
                } else if (hit == HIT_BTN_OK) {
                    // Save and close
                    SavePresetConfig();
                    DestroyWindow(hwnd);
                    return 0;
                } else if (hit == HIT_BTN_CANCEL) {
                    // Restore original presets
                    g_timerState.presetCount = g_origPresetCount;
                    for (int i = 0; i < g_origPresetCount && i < MAX_PRESETS; i++) {
                        g_timerState.presetTimes[i] = g_origPresets[i];
                    }
                    SavePresetConfig();
                    DestroyWindow(hwnd);
                    return 0;
                }
            }

            g_pressedId = HIT_NONE;
            UpdatePresetWindow();
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int visCount = GetVisibleCount();
            int maxScroll = GetMaxScroll();

            if (delta > 0 && g_scrollOffset > 0) {
                g_scrollOffset--;
            } else if (delta < 0 && g_scrollOffset < maxScroll) {
                g_scrollOffset++;
            }

            UpdatePresetWindow();
            return 0;
        }

        case WM_COMMAND: {
            int cmd = LOWORD(wParam);
            int code = HIWORD(wParam);

            // EDIT control notifications (inline edit)
            if (g_hEditCtrl && (HWND)lParam == g_hEditCtrl) {
                if (code == EN_UPDATE) {
                    // Auto-confirm on Enter
                }
            }

            return 0;
        }

        case WM_CHAR: {
            // Main input field
            if (g_inputFocused && g_editingIndex < 0) {
                if (wParam == VK_RETURN) {
                    // Submit: same as Add button
                    int minutes = _wtoi(g_inputBuf);
                    if (minutes > 0 && minutes <= 60) {
                        AddPresetTime(minutes);
                        g_inputBuf[0] = L'\0';
                        RefreshList();
                    } else if (minutes > 60) {
                        // Value exceeds 60 — show error
                        g_showToast = TRUE;
                        g_toastStart = GetTickCount();
                    }
                    // Empty input or 0: silently do nothing
                    return 0;
                } else if (wParam == VK_ESCAPE) {
                    g_inputFocused = FALSE;
                    g_cursorVisible = FALSE;
                    UpdatePresetWindow();
                    return 0;
                } else if (wParam == VK_BACK) {
                    // Backspace
                    int len = (int)wcslen(g_inputBuf);
                    if (len > 0) {
                        g_inputBuf[len - 1] = L'\0';
                        UpdatePresetWindow();
                    }
                    return 0;
                } else if (wParam >= L'0' && wParam <= L'9') {
                    // Digit input, max 60
                    int len = (int)wcslen(g_inputBuf);
                    if (len < 2) {
                        g_inputBuf[len] = (wchar_t)wParam;
                        g_inputBuf[len + 1] = L'\0';
                        // Reject if value exceeds 60
                        int val = _wtoi(g_inputBuf);
                        if (val > 60) {
                            g_inputBuf[len] = L'\0'; // undo
                            g_showToast = TRUE;
                            g_toastStart = GetTickCount();
                        } else {
                            UpdatePresetWindow();
                        }
                    }
                    return 0;
                }
            }

            // Inline edit control
            if (g_hEditCtrl && GetFocus() == g_hEditCtrl) {
                if (wParam == VK_RETURN) {
                    // Confirm edit
                    wchar_t buf[16];
                    GetWindowTextW(g_hEditCtrl, buf, 16);
                    int minutes = _wtoi(buf);
                    if (minutes > 0 && minutes <= 999) {
                        ModifyPresetTime(g_editingIndex, minutes);
                    }
                    DestroyWindow(g_hEditCtrl);
                    g_hEditCtrl = NULL;
                    g_editingIndex = -1;
                    SetFocus(hwnd);
                    RefreshList();
                    return 0;
                } else if (wParam == VK_ESCAPE) {
                    // Cancel edit
                    DestroyWindow(g_hEditCtrl);
                    g_hEditCtrl = NULL;
                    g_editingIndex = -1;
                    SetFocus(hwnd);
                    UpdatePresetWindow();
                    return 0;
                }
            }
            break;
        }

        case WM_KILLFOCUS: {
            // Unfocus the input field if focus moves elsewhere
            if (g_inputFocused) {
                g_inputFocused = FALSE;
                g_cursorVisible = FALSE;
                UpdatePresetWindow();
            }
            // If inline edit control loses focus (to something other than the dialog itself),
            // confirm the edit
            if (g_hEditCtrl && g_editingIndex >= 0 && IsWindow(g_hEditCtrl)) {
                HWND hFocus = (HWND)wParam;
                // Only auto-confirm if focus moved outside the edit control and not to dialog itself
                if (hFocus != g_hEditCtrl && hFocus != hwnd) {
                    wchar_t buf[16];
                    GetWindowTextW(g_hEditCtrl, buf, 16);
                    int minutes = _wtoi(buf);
                    if (minutes > 0 && minutes <= 999) {
                        ModifyPresetTime(g_editingIndex, minutes);
                    }
                    DestroyWindow(g_hEditCtrl);
                    // Don't set g_hEditCtrl to NULL here — the WM_DESTROY of the edit control
                    // (via subclass) already handles cleanup. Prevents double-destroy.
                }
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            HWND hEdit = (HWND)lParam;
            // Set white background and black text for EDIT controls
            SetBkColor(hdcEdit, RGB(255, 255, 255));
            SetTextColor(hdcEdit, RGB(28, 28, 30));
            // Return a white brush
            static HBRUSH hWhiteBrush = NULL;
            if (!hWhiteBrush) hWhiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            return (LRESULT)hWhiteBrush;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, 2);
            KillTimer(hwnd, 3);
            if (g_hEditCtrl && IsWindow(g_hEditCtrl)) { DestroyWindow(g_hEditCtrl); g_hEditCtrl = NULL; }
            if (g_hbmBuffer) DeleteObject(g_hbmBuffer);
            if (g_hdcBuffer) DeleteDC(g_hdcBuffer);
            g_hPresetDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// -----------------------------------------------------------
// Show / Create
// -----------------------------------------------------------
void ShowPresetEditDialog(void) {
    if (g_hPresetDialog && IsWindow(g_hPresetDialog)) {
        SetForegroundWindow(g_hPresetDialog);
        return;
    }
    CreatePresetEditDialog();
}

void CreatePresetEditDialog(void) {
    static BOOL classRegistered = FALSE;
    if (!classRegistered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = PresetEditDialogProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"ModernPresetDialogClass";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        if (RegisterClassW(&wc)) {
            classRegistered = TRUE;
        }
    }

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

    g_hPresetDialog = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST,
        L"ModernPresetDialogClass", L"", WS_POPUP,
        x, y, DLG_WIDTH, DLG_HEIGHT,
        g_timerState.hMainWnd, NULL, GetModuleHandle(NULL), NULL);

    if (!g_hPresetDialog) return;

    EnableWindow(g_timerState.hMainWnd, FALSE);
    ShowWindow(g_hPresetDialog, SW_SHOW);
}
