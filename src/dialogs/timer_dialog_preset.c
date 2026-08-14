#include "timer_dialog_internal.h"
#include "timer_render_utils.h"
#include "timer_buffer.h"
#include <uxtheme.h>

#define DLG_WIDTH  480
#define DLG_HEIGHT 524
#define DLG_SHADOW 30
#define DLG_RADIUS 8

// Unified value limit for add/edit (minutes)
#define PRESET_MIN_MINUTES 1
#define PRESET_MAX_MINUTES 999

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
#define SCROLLBAR_W  8
// Match the real storage cap in timer_types.h (presetTimes[10])
#define MAX_PRESETS  10
#define DELETE_HIT_W 24   // width of the hover delete (x) hit area on each row

static wchar_t g_inputBuf[16] = {0};     // top "add" field buffer
static wchar_t g_editBuf[16] = {0};      // inline row-edit buffer
static BOOL g_inputFocused = FALSE;
static BOOL g_cursorVisible = FALSE;
static BOOL g_showToast = FALSE;
static DWORD g_toastStart = 0;
static const DWORD TOAST_DURATION = 2000; // 2 seconds

// Backup for Cancel / Escape rollback
static int g_origPresetCount = 0;
static int g_origPresets[MAX_PRESETS] = {0};

// Forward declarations
static void UpdatePresetWindow(void);
static void RefreshList(void);
static void DoCancelRollback(HWND hwnd);

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
    HIT_ITEM_TEXT,    // click row body -> inline edit
    HIT_ITEM_DELETE,  // hover (x) button
    HIT_SCROLLBAR,
    HIT_BTN_OK,
    HIT_BTN_CANCEL
} HitTestID;

static HWND g_hPresetDialog = NULL;
static HDC  g_hdcBuffer = NULL;
static HBITMAP g_hbmBuffer = NULL;
static BYTE* g_pBits = NULL;

static int g_hoverRow = -1;
static HitTestID g_pressedId = HIT_NONE;
static BOOL g_draggingScrollbar = FALSE;
static BOOL g_draggingDlg = FALSE;
static POINT g_dragStartScreen;
static RECT  g_dlgStartRect;

static int g_scrollOffset = 0;
static int g_editingIndex = -1;   // -1 = not editing a row; otherwise the preset index being edited

// 拖拽排序状态
#define DRAG_THRESHOLD 4   // 移动超过这么多像素才算拖拽（否则视为点击）
static BOOL g_rowPressed = FALSE;     // 鼠标按在某个列表行上（待判定点击 vs 拖拽）
static int  g_pressedRow = -1;        // 按下的行索引
static POINT g_rowPressStart;         // 按下时的鼠标位置（用于判定阈值）
static BOOL g_isDraggingRow = FALSE;  // 已进入行拖拽状态
static int  g_dropTargetIndex = -1;   // 拖拽过程中当前插入目标索引（插入线位置）

static int GetVisibleCount(void) {
    int h = rcListArea.bottom - rcListArea.top;
    return h / (ITEM_HEIGHT + ITEM_GAP);
}

static int GetMaxScroll(void) {
    int total = g_timerState.presetCount;
    int vis = GetVisibleCount();
    return (total > vis) ? (total - vis) : 0;
}

// 根据 y 坐标返回拖拽落点对应的"插入索引"（0..presetCount）
// 落在屏幕上第 i 行上半部 → 插入到 i 之前；下半部 → 插入到 i+1 之前
static int PointToDropIndex(int y) {
    int visCount = GetVisibleCount();
    for (int i = 0; i < visCount && (g_scrollOffset + i) < g_timerState.presetCount; i++) {
        int itemY = rcListArea.top + 8 + i * (ITEM_HEIGHT + ITEM_GAP);
        int rowMid = itemY + ITEM_HEIGHT / 2;
        if (y < rowMid) {
            return g_scrollOffset + i;
        }
    }
    return g_timerState.presetCount;
}

// -----------------------------------------------------------
// Inline-edit helpers (reuse the self-drawn input mechanism)
// -----------------------------------------------------------

// Commit the current inline edit. Returns TRUE if committed.
static BOOL CommitInlineEdit(void) {
    if (g_editingIndex < 0) return FALSE;
    int minutes = _wtoi(g_editBuf);
    if (minutes >= PRESET_MIN_MINUTES && minutes <= PRESET_MAX_MINUTES) {
        ModifyPresetTime(g_editingIndex, minutes);
    } else if (g_editBuf[0] != L'\0') {
        // Invalid value — show error, discard edit
        g_showToast = TRUE;
        g_toastStart = GetTickCount();
    }
    g_editingIndex = -1;
    g_editBuf[0] = L'\0';
    return TRUE;
}

// Cancel the current inline edit without saving.
static void CancelInlineEdit(void) {
    g_editingIndex = -1;
    g_editBuf[0] = L'\0';
}

// Start inline-editing a row. Copies current value into g_editBuf.
static void StartInlineEdit(int presetIdx) {
    if (presetIdx < 0 || presetIdx >= g_timerState.presetCount) return;
    // If another row is being edited, commit it first
    if (g_editingIndex >= 0 && g_editingIndex != presetIdx) {
        CommitInlineEdit();
    }
    g_editingIndex = presetIdx;
    swprintf(g_editBuf, 16, L"%d", g_timerState.presetTimes[presetIdx]);
    g_cursorVisible = TRUE;
    // Move focus away from the top add field
    g_inputFocused = FALSE;
}

// -----------------------------------------------------------
// Render dialog UI to off-screen buffer
// -----------------------------------------------------------
static void RenderPresetDialog(void) {
    if (!g_pBits) return;
    memset(g_pBits, 0, DLG_WIDTH * DLG_HEIGHT * 4);

    // Panel: soft shadow + fill + thin border (Win11 native dialog style)
    DrawSoftShadowSDF(g_pBits, DLG_WIDTH, DLG_HEIGHT,
        rcCard.left, rcCard.top, rcCard.right - rcCard.left, rcCard.bottom - rcCard.top,
        DLG_RADIUS, DLG_SHADOW, 3, 0.08f);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, DLG_RADIUS, DLG_RADIUS,
        rcCard.left, rcCard.top, rcCard.right - rcCard.left, rcCard.bottom - rcCard.top,
        GetRValue(UI_LIGHT_BG_PRIMARY), GetGValue(UI_LIGHT_BG_PRIMARY), GetBValue(UI_LIGHT_BG_PRIMARY), 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, DLG_RADIUS, DLG_RADIUS,
        rcCard.left, rcCard.top, rcCard.right - rcCard.left, rcCard.bottom - rcCard.top,
        1, GetRValue(UI_LIGHT_BORDER), GetGValue(UI_LIGHT_BORDER), GetBValue(UI_LIGHT_BORDER), 255);

    const MenuTexts* texts = GetMenuTexts();

    HFONT hFontTitle = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
    HFONT hFontLabel = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
    HFONT hFontBtn   = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
    HFONT hFontItem  = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");
    HFONT hFontHint  = CreateFontW(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable");

    // ---- Title ----
    RECT rcTitle = {rcCard.left + 20, rcCard.top + 20, rcCard.right - 20, rcCard.top + 55};
    DrawTextSDF(g_hdcBuffer, texts->presetEditTitle, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontTitle, UI_LIGHT_TEXT_PRIMARY);

    // ---- New Preset label ----
    RECT rcNPLabel = {rcInput.left, rcInput.top - 22, rcInput.right, rcInput.top};
    DrawTextSDF(g_hdcBuffer, texts->newPreset, &rcNPLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_SECONDARY);

    // ---- Input field ----
    BOOL inputHover = (g_pressedId == HIT_INPUT);
    COLORREF inputBorder = g_inputFocused ? UI_PRIMARY_COLOR : (inputHover ? UI_PRIMARY_HOVER : UI_LIGHT_BORDER);
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
        rcInput.left, rcInput.top, rcInput.right - rcInput.left, rcInput.bottom - rcInput.top,
        GetRValue(UI_LIGHT_SURFACE), GetGValue(UI_LIGHT_SURFACE), GetBValue(UI_LIGHT_SURFACE), 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
        rcInput.left, rcInput.top, rcInput.right - rcInput.left, rcInput.bottom - rcInput.top,
        g_inputFocused ? 2 : 1, GetRValue(inputBorder), GetGValue(inputBorder), GetBValue(inputBorder), 255);

    // Input text
    if (g_inputBuf[0] != L'\0') {
        RECT rcInputText = {rcInput.left + 12, rcInput.top + 2, rcInput.right - 12, rcInput.bottom - 2};
        DrawTextSDF(g_hdcBuffer, g_inputBuf, &rcInputText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_PRIMARY);
    } else if (g_inputFocused) {
        RECT rcInputText = {rcInput.left + 12, rcInput.top + 2, rcInput.right - 12, rcInput.bottom - 2};
        DrawTextSDF(g_hdcBuffer, L"1-999", &rcInputText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_DISABLED);
    }

    // Input cursor
    if (g_inputFocused && g_cursorVisible) {
        HFONT hOldFont = (HFONT)SelectObject(g_hdcBuffer, hFontLabel);
        SIZE sz;
        GetTextExtentPoint32W(g_hdcBuffer, g_inputBuf, (int)wcslen(g_inputBuf), &sz);
        SelectObject(g_hdcBuffer, hOldFont);
        int cursorX = rcInput.left + 12 + sz.cx;
        int cursorY = rcInput.top + 5;
        int cursorH = rcInput.bottom - rcInput.top - 10;
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 1, 1, cursorX, cursorY, 2, cursorH,
            UI_PRIMARY_COLOR & 0xFF, (UI_PRIMARY_COLOR >> 8) & 0xFF, (UI_PRIMARY_COLOR >> 16) & 0xFF, 255);
    }

    // ---- Add button ----
    BOOL addHover = (g_pressedId == HIT_BTN_ADD);
    COLORREF addFill = addHover ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR;
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
        rcBtnAdd.left, rcBtnAdd.top, rcBtnAdd.right - rcBtnAdd.left, rcBtnAdd.bottom - rcBtnAdd.top,
        GetRValue(addFill), GetGValue(addFill), GetBValue(addFill), 255);
    RECT rcAddT = rcBtnAdd;
    DrawTextSDF(g_hdcBuffer, texts->addPreset, &rcAddT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(255, 255, 255));

    // ---- Preset List label ----
    RECT rcPLLabel = {rcListArea.left, rcListArea.top - 24, rcListArea.right, rcListArea.top};
    DrawTextSDF(g_hdcBuffer, texts->presetList, &rcPLLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontLabel, UI_LIGHT_TEXT_SECONDARY);

    // ---- List container: flattened into panel (no separate card) ----

    // ---- Draw list items ----
    int visCount = GetVisibleCount();
    int contentW = rcListArea.right - rcListArea.left - SCROLLBAR_W - 8;

    for (int i = 0; i < visCount && (g_scrollOffset + i) < g_timerState.presetCount; i++) {
        int idx = g_scrollOffset + i;
        int itemY = rcListArea.top + 8 + i * (ITEM_HEIGHT + ITEM_GAP);

        BOOL isHover = (g_hoverRow == idx);
        BOOL isEditing = (g_editingIndex == idx);

        // Row background: subtle hover (only for non-editing rows)
        if (isHover && !isEditing) {
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
                rcListArea.left + 4, itemY, contentW, ITEM_HEIGHT,
                GetRValue(UI_LIGHT_BUTTON_HOVER), GetGValue(UI_LIGHT_BUTTON_HOVER), GetBValue(UI_LIGHT_BUTTON_HOVER), 255);
        }

        // 拖拽中的源行：盖一层半透明遮罩表示"正在被拖动"
        if (g_isDraggingRow && idx == g_pressedRow) {
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
                rcListArea.left + 4, itemY, contentW, ITEM_HEIGHT,
                UI_PRIMARY_COLOR & 0xFF, (UI_PRIMARY_COLOR >> 8) & 0xFF, (UI_PRIMARY_COLOR >> 16) & 0xFF, 40);
        }

        // Delete (x) button — shown on hover, right-aligned
        if (isHover && !isEditing) {
            int xCx = rcListArea.left + 4 + contentW - DELETE_HIT_W;
            int xCy = itemY + (ITEM_HEIGHT - DELETE_HIT_W) / 2;
            // (x) glyph drawn as text for crispness
            RECT rcX = {xCx, xCy, xCx + DELETE_HIT_W, xCy + DELETE_HIT_W};
            DrawTextSDF(g_hdcBuffer, L"\u2715", &rcX, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, UI_LIGHT_TEXT_SECONDARY);
        }

        if (isEditing) {
            // Inline-edit row: draw a focus-bordered input cell covering the text area
            RECT rcEditCell = {rcListArea.left + 14, itemY + 2, rcListArea.left + 4 + contentW - DELETE_HIT_W - 6, itemY + ITEM_HEIGHT - 2};
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
                rcEditCell.left, rcEditCell.top, rcEditCell.right - rcEditCell.left, rcEditCell.bottom - rcEditCell.top,
                GetRValue(UI_LIGHT_SURFACE), GetGValue(UI_LIGHT_SURFACE), GetBValue(UI_LIGHT_SURFACE), 255);
            DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
                rcEditCell.left, rcEditCell.top, rcEditCell.right - rcEditCell.left, rcEditCell.bottom - rcEditCell.top,
                2, UI_PRIMARY_COLOR & 0xFF, (UI_PRIMARY_COLOR >> 8) & 0xFF, (UI_PRIMARY_COLOR >> 16) & 0xFF, 255);

            // Edit text
            RECT rcEditText = {rcEditCell.left + 8, rcEditCell.top, rcEditCell.right - 8, rcEditCell.bottom};
            if (g_editBuf[0] != L'\0') {
                DrawTextSDF(g_hdcBuffer, g_editBuf, &rcEditText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontItem, UI_LIGHT_TEXT_PRIMARY);
            }
            // Edit cursor
            if (g_cursorVisible) {
                HFONT hOldFont = (HFONT)SelectObject(g_hdcBuffer, hFontItem);
                SIZE sz;
                GetTextExtentPoint32W(g_hdcBuffer, g_editBuf, (int)wcslen(g_editBuf), &sz);
                SelectObject(g_hdcBuffer, hOldFont);
                int cursorX = rcEditCell.left + 8 + sz.cx;
                int cursorY = rcEditCell.top + 4;
                int cursorH = rcEditCell.bottom - rcEditCell.top - 8;
                FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 1, 1, cursorX, cursorY, 2, cursorH,
                    UI_PRIMARY_COLOR & 0xFF, (UI_PRIMARY_COLOR >> 8) & 0xFF, (UI_PRIMARY_COLOR >> 16) & 0xFF, 255);
            }
        } else {
            // Normal row: "N 分钟"
            wchar_t itemText[32];
            swprintf(itemText, 32, L"%d %s", g_timerState.presetTimes[idx],
                     (g_timerState.currentLanguage == TIMER_LANG_ENGLISH) ? L"min" : L"分钟");
            RECT rcItemText = {rcListArea.left + 14, itemY, rcListArea.left + 4 + contentW - DELETE_HIT_W - 6, itemY + ITEM_HEIGHT};
            DrawTextSDF(g_hdcBuffer, itemText, &rcItemText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontItem, UI_LIGHT_TEXT_PRIMARY);
        }
    }

    // ---- 拖拽插入线 ----
    if (g_isDraggingRow && g_dropTargetIndex >= 0) {
        // 算出插入线 y 坐标：找到目标索引对应的可见行顶部
        int lineY = -1;
        int visCount2 = GetVisibleCount();
        int relIdx = g_dropTargetIndex - g_scrollOffset;
        if (relIdx >= 0 && relIdx <= visCount2) {
            lineY = rcListArea.top + 8 + relIdx * (ITEM_HEIGHT + ITEM_GAP) - ITEM_GAP / 2;
        } else if (relIdx < 0) {
            lineY = rcListArea.top + 8 - ITEM_GAP / 2;
        }
        if (lineY >= rcListArea.top && lineY <= rcListArea.bottom) {
            FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 1, 1,
                rcListArea.left + 6, lineY - 1, contentW - 4, 3,
                UI_PRIMARY_COLOR & 0xFF, (UI_PRIMARY_COLOR >> 8) & 0xFF, (UI_PRIMARY_COLOR >> 16) & 0xFF, 255);
        }
    }

    // ---- Empty-state hint inside list area ----
    if (g_timerState.presetCount == 0) {
        const wchar_t* emptyMsg = (g_timerState.currentLanguage == TIMER_LANG_ENGLISH)
            ? L"No presets yet" : L"暂无预设";
        RECT rcEmpty = {rcListArea.left, rcListArea.top, rcListArea.right - SCROLLBAR_W, rcListArea.bottom};
        DrawTextSDF(g_hdcBuffer, emptyMsg, &rcEmpty, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontHint, UI_LIGHT_TEXT_DISABLED);
    }

    // ---- Scrollbar ----
    int maxScroll = GetMaxScroll();
    if (maxScroll > 0) {
        int sbTrackH = rcScrollbar.bottom - rcScrollbar.top;
        int thumbH = max(20, (int)(sbTrackH * (float)visCount / g_timerState.presetCount));
        int thumbRange = sbTrackH - thumbH;
        int thumbY = rcScrollbar.top + (maxScroll > 0 ? (g_scrollOffset * thumbRange / maxScroll) : 0);

        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
            rcScrollbar.left, rcScrollbar.top, SCROLLBAR_W, sbTrackH,
            GetRValue(UI_LIGHT_BORDER), GetGValue(UI_LIGHT_BORDER), GetBValue(UI_LIGHT_BORDER), 255);
        FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
            rcScrollbar.left, thumbY, SCROLLBAR_W, thumbH,
            166, 166, 166, 255);
    }

    // ---- OK button ----
    BOOL okHover = (g_pressedId == HIT_BTN_OK);
    COLORREF okFill = okHover ? UI_PRIMARY_HOVER : UI_PRIMARY_COLOR;
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
        rcBtnOK.left, rcBtnOK.top, rcBtnOK.right - rcBtnOK.left, rcBtnOK.bottom - rcBtnOK.top,
        GetRValue(okFill), GetGValue(okFill), GetBValue(okFill), 255);
    RECT rcOKT = rcBtnOK;
    DrawTextSDF(g_hdcBuffer, texts->ok, &rcOKT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, RGB(255, 255, 255));

    // ---- Cancel button ----
    BOOL cancelHover = (g_pressedId == HIT_BTN_CANCEL);
    COLORREF cancelFill = cancelHover ? UI_LIGHT_BUTTON_PRESSED : UI_LIGHT_BG_SECONDARY;
    FillRoundedRectAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
        rcBtnCancel.left, rcBtnCancel.top, rcBtnCancel.right - rcBtnCancel.left, rcBtnCancel.bottom - rcBtnCancel.top,
        GetRValue(cancelFill), GetGValue(cancelFill), GetBValue(cancelFill), 255);
    DrawRoundedRectOutlineAA(g_pBits, DLG_WIDTH, DLG_HEIGHT, 4, 4,
        rcBtnCancel.left, rcBtnCancel.top, rcBtnCancel.right - rcBtnCancel.left, rcBtnCancel.bottom - rcBtnCancel.top,
        1, GetRValue(UI_LIGHT_BORDER), GetGValue(UI_LIGHT_BORDER), GetBValue(UI_LIGHT_BORDER), 255);
    RECT rcCancelT = rcBtnCancel;
    DrawTextSDF(g_hdcBuffer, texts->cancel, &rcCancelT, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hFontBtn, UI_LIGHT_TEXT_PRIMARY);

    // ---- Hint text ----
    RECT rcHint = {rcCard.left + 20, rcBtnOK.top, rcBtnOK.left - 10, rcBtnOK.bottom};
    DrawTextSDF(g_hdcBuffer, texts->doubleClickHint, &rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontHint, UI_LIGHT_TEXT_SECONDARY);

    // ---- Inline error message below input ----
    if (g_showToast) {
        RECT rcErr = {rcInput.left, rcInput.bottom + 6, rcInput.right, rcInput.bottom + 28};
        DrawTextSDF(g_hdcBuffer, texts->errorPresetMax, &rcErr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, hFontHint, RGB(200, 50, 50));
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
            // Delete (x) hit area on the right
            RECT rcDelete = {rcListArea.left + 4 + contentW - DELETE_HIT_W, itemY,
                             rcListArea.left + 4 + contentW, itemY + ITEM_HEIGHT};
            if (PtInRect(&rcDelete, pt)) return HIT_ITEM_DELETE;
            // Rest of the row -> inline edit
            return HIT_ITEM_TEXT;
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

// Rollback to the snapshot taken at WM_CREATE, then destroy the dialog.
static void DoCancelRollback(HWND hwnd) {
    g_timerState.presetCount = g_origPresetCount;
    for (int i = 0; i < g_origPresetCount && i < MAX_PRESETS; i++) {
        g_timerState.presetTimes[i] = g_origPresets[i];
    }
    SavePresetConfig();
    DestroyWindow(hwnd);
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
            g_inputFocused = FALSE;
            g_inputBuf[0] = L'\0';
            g_editBuf[0] = L'\0';

            // Backup presets for Cancel/Esc rollback
            g_origPresetCount = g_timerState.presetCount;
            for (int i = 0; i < g_origPresetCount && i < MAX_PRESETS; i++) {
                g_origPresets[i] = g_timerState.presetTimes[i];
            }

            UpdatePresetWindow();
            SetTimer(hwnd, 2, 100, NULL);  // Main window refresh
            SetTimer(hwnd, 3, 500, NULL);  // Cursor blink / toast
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
                // Cursor blink (active in either add or edit mode)
                if (g_inputFocused || g_editingIndex >= 0) {
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

            // 行拖拽排序：按下后移动超过阈值才算拖拽
            if (g_rowPressed && !g_isDraggingRow) {
                int dx = pt.x - g_rowPressStart.x;
                int dy = pt.y - g_rowPressStart.y;
                if (dx*dx + dy*dy >= DRAG_THRESHOLD * DRAG_THRESHOLD) {
                    g_isDraggingRow = TRUE;
                    g_dropTargetIndex = PointToDropIndex(pt.y);
                }
            }
            if (g_isDraggingRow) {
                int newDrop = PointToDropIndex(pt.y);
                if (newDrop != g_dropTargetIndex) {
                    g_dropTargetIndex = newDrop;
                    UpdatePresetWindow();
                }
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
            g_showToast = FALSE;

            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);

            // 点击任何地方前，先提交当前正在编辑的行（"点击别处即生效"）
            // 例外：点的是正在编辑的那一行本身（双击序列要继续编辑）
            BOOL hitIsEditingRow = (hit == HIT_ITEM_TEXT && g_hoverRow == g_editingIndex);
            if (g_editingIndex >= 0 && !hitIsEditingRow) {
                CommitInlineEdit();
                RefreshList();
            }

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
                // 开头已提交行编辑，这里只需切换焦点到添加框
                g_inputFocused = TRUE;
                g_cursorVisible = TRUE;
            } else if (hit == HIT_ITEM_TEXT) {
                // 行被按下：先记录，等松开时判定点击 vs 拖拽（拖拽阈值在 MOUSEMOVE 里判定）
                // 正在编辑的行不响应拖拽（避免编辑中误触排序）
                if (g_hoverRow >= 0 && g_hoverRow < g_timerState.presetCount && g_hoverRow != g_editingIndex) {
                    g_rowPressed = TRUE;
                    g_pressedRow = g_hoverRow;
                    g_rowPressStart = pt;
                    g_isDraggingRow = FALSE;
                    g_dropTargetIndex = -1;
                    SetCapture(hwnd);
                }
            }

            UpdatePresetWindow();
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_draggingScrollbar) { g_draggingScrollbar = FALSE; ReleaseCapture(); }
            if (g_draggingDlg) { g_draggingDlg = FALSE; ReleaseCapture(); }

            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};

            // 拖拽排序结束：执行移动
            if (g_isDraggingRow) {
                if (g_pressedRow >= 0 && g_dropTargetIndex >= 0 &&
                    g_dropTargetIndex <= g_timerState.presetCount) {
                    MovePresetTime(g_pressedRow, g_dropTargetIndex);
                    RefreshList();
                }
                g_isDraggingRow = FALSE;
                g_rowPressed = FALSE;
                g_pressedRow = -1;
                g_dropTargetIndex = -1;
                ReleaseCapture();
                UpdatePresetWindow();
                return 0;
            }
            if (g_rowPressed) {
                // 未超过阈值 = 普通点击（留给 WM_LBUTTONDBLCLK 处理编辑）
                g_rowPressed = FALSE;
                g_pressedRow = -1;
                ReleaseCapture();
                g_pressedId = HIT_NONE;
                UpdatePresetWindow();
                return 0;
            }

            HitTestID hit = HitTest(pt);

            if (hit == g_pressedId && hit != HIT_NONE) {
                if (hit == HIT_BTN_ADD) {
                    if (g_editingIndex >= 0) { CommitInlineEdit(); }
                    int minutes = _wtoi(g_inputBuf);
                    if (minutes >= PRESET_MIN_MINUTES && minutes <= PRESET_MAX_MINUTES) {
                        AddPresetTime(minutes);
                        g_inputBuf[0] = L'\0';
                        RefreshList();
                        SetFocus(hwnd);
                    } else if (g_editBuf[0] != L'\0' || g_inputBuf[0] != L'\0') {
                        g_showToast = TRUE;
                        g_toastStart = GetTickCount();
                    }
                } else if (hit == HIT_ITEM_DELETE) {
                    if (g_editingIndex >= 0) { CancelInlineEdit(); }
                    if (g_hoverRow >= 0 && g_hoverRow < g_timerState.presetCount) {
                        DeletePresetTime(g_hoverRow);
                        g_hoverRow = -1;
                        RefreshList();
                    }
                } else if (hit == HIT_BTN_OK) {
                    if (g_editingIndex >= 0) { CommitInlineEdit(); }
                    SavePresetConfig();
                    DestroyWindow(hwnd);
                    return 0;
                } else if (hit == HIT_BTN_CANCEL) {
                    DoCancelRollback(hwnd);
                    return 0;
                }
            }

            g_pressedId = HIT_NONE;
            UpdatePresetWindow();
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            // 双击行：进入就地编辑
            POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            HitTestID hit = HitTest(pt);
            if (hit == HIT_ITEM_TEXT) {
                // DBLCLK 前会有一次 DOWN/UP，重置待拖拽状态
                g_rowPressed = FALSE;
                g_pressedRow = -1;
                g_isDraggingRow = FALSE;
                g_dropTargetIndex = -1;
                if (g_hoverRow >= 0 && g_hoverRow < g_timerState.presetCount) {
                    StartInlineEdit(g_hoverRow);
                    SetFocus(hwnd);
                    UpdatePresetWindow();
                }
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int maxScroll = GetMaxScroll();

            if (delta > 0 && g_scrollOffset > 0) {
                g_scrollOffset--;
            } else if (delta < 0 && g_scrollOffset < maxScroll) {
                g_scrollOffset++;
            }

            UpdatePresetWindow();
            return 0;
        }

        case WM_CHAR: {
            // ---- Inline row edit mode ----
            if (g_editingIndex >= 0) {
                if (wParam == VK_RETURN) {
                    CommitInlineEdit();
                    RefreshList();
                    return 0;
                } else if (wParam == VK_ESCAPE) {
                    CancelInlineEdit();
                    UpdatePresetWindow();
                    return 0;
                } else if (wParam == VK_BACK) {
                    int len = (int)wcslen(g_editBuf);
                    if (len > 0) {
                        g_editBuf[len - 1] = L'\0';
                        g_cursorVisible = TRUE;
                        UpdatePresetWindow();
                    }
                    return 0;
                } else if (wParam >= L'0' && wParam <= L'9') {
                    int len = (int)wcslen(g_editBuf);
                    if (len < 3) {  // up to 3 digits (max 999)
                        g_editBuf[len] = (wchar_t)wParam;
                        g_editBuf[len + 1] = L'\0';
                        g_cursorVisible = TRUE;
                        UpdatePresetWindow();
                    }
                    return 0;
                }
                return 0;
            }

            // ---- Top "add" field mode ----
            if (g_inputFocused) {
                if (wParam == VK_RETURN) {
                    int minutes = _wtoi(g_inputBuf);
                    if (minutes >= PRESET_MIN_MINUTES && minutes <= PRESET_MAX_MINUTES) {
                        AddPresetTime(minutes);
                        g_inputBuf[0] = L'\0';
                        RefreshList();
                    } else if (g_inputBuf[0] != L'\0') {
                        g_showToast = TRUE;
                        g_toastStart = GetTickCount();
                    }
                    return 0;
                } else if (wParam == VK_ESCAPE) {
                    g_inputFocused = FALSE;
                    g_cursorVisible = FALSE;
                    g_inputBuf[0] = L'\0';
                    UpdatePresetWindow();
                    return 0;
                } else if (wParam == VK_BACK) {
                    int len = (int)wcslen(g_inputBuf);
                    if (len > 0) {
                        g_inputBuf[len - 1] = L'\0';
                        UpdatePresetWindow();
                    }
                    return 0;
                } else if (wParam >= L'0' && wParam <= L'9') {
                    int len = (int)wcslen(g_inputBuf);
                    if (len < 3) {  // up to 3 digits (max 999)
                        g_inputBuf[len] = (wchar_t)wParam;
                        g_inputBuf[len + 1] = L'\0';
                        UpdatePresetWindow();
                    }
                    return 0;
                }
            }
            break;
        }

        case WM_KILLFOCUS: {
            // Auto-commit inline edit if the dialog loses focus entirely
            if (g_editingIndex >= 0) {
                CommitInlineEdit();
                RefreshList();
            }
            if (g_inputFocused) {
                g_inputFocused = FALSE;
                g_cursorVisible = FALSE;
                UpdatePresetWindow();
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                if (g_editingIndex >= 0) {
                    // Escape while editing a row: cancel that edit only
                    CancelInlineEdit();
                    UpdatePresetWindow();
                    return 0;
                }
                // No active edit: Escape closes the dialog WITH rollback
                DoCancelRollback(hwnd);
                return 0;
            }
            if (wParam == VK_RETURN) {
                // Enter with no field focused: behave like OK
                if (g_editingIndex >= 0) { CommitInlineEdit(); }
                SavePresetConfig();
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, 2);
            KillTimer(hwnd, 3);
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
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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
