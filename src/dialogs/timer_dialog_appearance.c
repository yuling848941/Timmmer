#include "timer_dialog_internal.h"

static HWND g_hAppearanceHoverBtn = NULL;
static int g_hoverBtnId = 0;
static HWND g_hAppearancePressedBtn = NULL;
static HWND g_hAppearanceDialog = NULL;
static BOOL g_isAppearanceDlgDragging = FALSE;
static POINT g_appearanceDlgDragStart;
static RECT g_appearanceDlgRectStart;
static COLORREF g_tempFontColor;
static COLORREF g_tempBackgroundColor;
static COLORREF g_originalFontColor;
static COLORREF g_originalBackgroundColor;
static BYTE g_tempOpacity;
static BYTE g_originalOpacity;
static BOOL g_originalTransparentBackground;
static wchar_t g_originalFontName[256];
static BOOL g_fontPreviewActive = FALSE;
static BOOL g_fontSelectionMade = FALSE;
static HWND g_fontComboBox = NULL;
static WNDPROC g_originalComboProc = NULL;
static HWND g_fontListBox = NULL;
static WNDPROC g_originalListProc = NULL;

HWND GetAppearanceDialog(void) {
    return g_hAppearanceDialog;
}

// 外观设置对话框
void ShowAppearanceDialog() {
    if (g_hAppearanceDialog && IsWindow(g_hAppearanceDialog)) {
        SetForegroundWindow(g_hAppearanceDialog);
        return;
    }
    CreateAppearanceDialog();
}

static void DrawModernCard(HDC hdc, RECT* rect) {
    DrawRoundRect(hdc, rect, 12, RGB(255, 255, 255), RGB(230, 230, 230), 1);
}

// Draw iOS-style Switch
static void DrawSwitch(HDC hdc, RECT* rect, BOOL isOn) {
    int w = rect->right - rect->left;
    int h = rect->bottom - rect->top;
    if (isOn) {
        DrawRoundRect(hdc, rect, h/2, UI_PRIMARY_COLOR, UI_PRIMARY_COLOR, 1);
        RECT thumb = {rect->right - h + 2, rect->top + 2, rect->right - 2, rect->bottom - 2};
        DrawRoundRect(hdc, &thumb, (h-4)/2, RGB(255,255,255), RGB(255,255,255), 1);
    } else {
        DrawRoundRect(hdc, rect, h/2, RGB(220, 220, 225), RGB(200, 200, 205), 1);
        RECT thumb = {rect->left + 2, rect->top + 2, rect->left + h - 2, rect->bottom - 2};
        DrawRoundRect(hdc, &thumb, (h-4)/2, RGB(255,255,255), RGB(200,200,205), 1);
    }
}

static WNDPROC g_originalSliderProc = NULL;
static BOOL g_isSliderDragging = FALSE;
static LRESULT CALLBACK OpacitySliderSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);
            
            int trackH = 6;
            int trackY = (rc.bottom - rc.top - trackH) / 2;
            RECT trackRc = {rc.left + 10, trackY, rc.right - 10, trackY + trackH};
            DrawRoundRect(hdc, &trackRc, 3, RGB(230, 230, 235), RGB(230, 230, 235), 1);
            
            float percent = (g_tempOpacity - 50) / 205.0f;
            if (percent < 0) percent = 0; if (percent > 1) percent = 1;
            int fillW = (int)((trackRc.right - trackRc.left) * percent);
            RECT fillRc = {trackRc.left, trackY, trackRc.left + fillW, trackY + trackH};
            DrawRoundRect(hdc, &fillRc, 3, UI_PRIMARY_COLOR, UI_PRIMARY_COLOR, 1);
            
            int thumbX = fillRc.right;
            int thumbR = 10;
            RECT thumbRc = {thumbX - thumbR, (rc.bottom-rc.top)/2 - thumbR, thumbX + thumbR, (rc.bottom-rc.top)/2 + thumbR};
            DrawRoundRect(hdc, &thumbRc, thumbR, RGB(255, 255, 255), RGB(200, 200, 200), 1);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE: {
            if ((uMsg == WM_LBUTTONDOWN) || (wParam & MK_LBUTTON)) {
                if (uMsg == WM_LBUTTONDOWN) { SetCapture(hwnd); g_isSliderDragging = TRUE; }
                RECT rc; GetClientRect(hwnd, &rc);
                int x = (short)LOWORD(lParam);
                int trackL = 10;
                int trackR = rc.right - 10;
                float percent = (float)(x - trackL) / (trackR - trackL);
                if (percent < 0) percent = 0; if (percent > 1) percent = 1;
                g_tempOpacity = (BYTE)(50 + percent * 205);
                InvalidateRect(hwnd, NULL, FALSE);
                HWND hLabel = GetDlgItem(GetParent(hwnd), ID_APPEARANCE_OPACITY_LABEL);
                if (hLabel) {
                    wchar_t opacityText[16];
                    swprintf(opacityText, 16, L"%d%%", (int)((g_tempOpacity / 255.0) * 100));
                    SetWindowTextW(hLabel, opacityText);
                }
                if (g_timerState.transparentBackground) { UpdateLayeredWindow_Render(); } 
                else {
                    SetLayeredWindowAttributes(g_timerState.hMainWnd, 0, g_tempOpacity, LWA_ALPHA);
                    InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                    UpdateWindow(g_timerState.hMainWnd);
                }
                return 0;
            }
            break;
        }
        case WM_LBUTTONUP:
            if (g_isSliderDragging) { ReleaseCapture(); g_isSliderDragging = FALSE; }
            return 0;
    }
    return CallWindowProc(g_originalSliderProc, hwnd, uMsg, wParam, lParam);
}

void CreateAppearanceDialog() {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = AppearanceDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"AppearanceDialogClass";
    wc.hbrBackground = CreateSolidBrush(RGB(245, 245, 247)); // Light modern background
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    const MenuTexts* texts = GetMenuTexts();
    RECT parentRect; GetWindowRect(g_timerState.hMainWnd, &parentRect);
    int dialogWidth = 460;
    int dialogHeight = 520;
    int x = parentRect.left + (parentRect.right - parentRect.left - dialogWidth) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - dialogHeight) / 2;

    g_hAppearanceDialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"AppearanceDialogClass", texts->appearanceTitle,
        WS_POPUP | WS_VISIBLE | WS_TABSTOP,
        x, y, dialogWidth, dialogHeight,
        g_timerState.hMainWnd, NULL, GetModuleHandle(NULL), NULL);

    if (g_hAppearanceDialog) {
        EnableWindow(g_timerState.hMainWnd, FALSE);
        SetForegroundWindow(g_hAppearanceDialog);
        DWORD cornerPref = DWMWCP_ROUND;
        DwmSetWindowAttribute(g_hAppearanceDialog, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
        MARGINS shadow = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(g_hAppearanceDialog, &shadow);
    }
}

// 字体预览辅助函数
void ApplyFontPreview(const wchar_t* fontName) {
    if (!fontName || wcslen(fontName) == 0) return;
    wcscpy_s(g_timerState.currentFontName, sizeof(g_timerState.currentFontName)/sizeof(wchar_t), fontName);
    ClearFontCache();
    g_timerState.cachedFontValid = FALSE;
    g_timerState.cachedHFont = NULL;
    if (g_timerState.hMainWnd && IsWindow(g_timerState.hMainWnd)) {
        if (g_timerState.transparentBackground) {
            UpdateLayeredWindow_Render();
        } else {
            InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
            UpdateWindow(g_timerState.hMainWnd);
        }
    }
}

void RestoreOriginalFont() {
    if (g_fontPreviewActive && wcslen(g_originalFontName) > 0) {
        wcscpy_s(g_timerState.currentFontName, sizeof(g_timerState.currentFontName)/sizeof(wchar_t), g_originalFontName);
        ClearFontCache();
        g_timerState.cachedFontValid = FALSE;
        g_timerState.cachedHFont = NULL;
        if (g_timerState.hMainWnd && IsWindow(g_timerState.hMainWnd)) {
            if (g_timerState.transparentBackground) {
                UpdateLayeredWindow_Render();
            } else {
                InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                UpdateWindow(g_timerState.hMainWnd);
            }
        }
        // DO NOT set g_fontPreviewActive to FALSE here, let CBN_CLOSEUP handle it.
    }
}

LRESULT CALLBACK FontListSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static int lastHoverIndex = -1;
    switch (uMsg) {
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            int itemIndex = (int)SendMessage(hwnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
            if (HIWORD(itemIndex) == 0 && LOWORD(itemIndex) != lastHoverIndex) {
                int realIndex = LOWORD(itemIndex);
                int totalCount = (int)SendMessage(hwnd, LB_GETCOUNT, 0, 0);
                if (realIndex >= 0 && realIndex < totalCount) {
                    wchar_t fontName[256];
                    if (SendMessageW(hwnd, LB_GETTEXT, realIndex, (LPARAM)fontName) != LB_ERR) {
                        ApplyFontPreview(fontName);
                        lastHoverIndex = realIndex;
                    }
                }
            }
            break;
        }
        case WM_MOUSELEAVE: lastHoverIndex = -1; break;
    }
    return CallWindowProc(g_originalListProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK FontComboSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static int lastHoverIndex = -1;
    switch (uMsg) {
        case WM_MOUSEMOVE: {
            if (!g_fontPreviewActive) break;
            TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            COMBOBOXINFO cbi = {sizeof(COMBOBOXINFO)};
            if (GetComboBoxInfo(hwnd, &cbi) && cbi.hwndList && IsWindowVisible(cbi.hwndList)) {
                POINT screenPt; GetCursorPos(&screenPt);
                RECT listWindowRect; GetWindowRect(cbi.hwndList, &listWindowRect);
                if (screenPt.x >= listWindowRect.left && screenPt.x <= listWindowRect.right && 
                    screenPt.y >= listWindowRect.top && screenPt.y <= listWindowRect.bottom) {
                    POINT listPt = screenPt; ScreenToClient(cbi.hwndList, &listPt);
                    int itemIndex = (int)SendMessage(cbi.hwndList, LB_ITEMFROMPOINT, 0, MAKELPARAM(listPt.x, listPt.y));
                    if (HIWORD(itemIndex) == 0 && LOWORD(itemIndex) != lastHoverIndex) {
                        int realIndex = LOWORD(itemIndex);
                        int totalCount = (int)SendMessage(hwnd, CB_GETCOUNT, 0, 0);
                        if (realIndex >= 0 && realIndex < totalCount) {
                            wchar_t fontName[256];
                            if (SendMessageW(hwnd, CB_GETLBTEXT, realIndex, (LPARAM)fontName) != CB_ERR) {
                                ApplyFontPreview(fontName);
                                lastHoverIndex = realIndex;
                            }
                        }
                    }
                }
            }
            break;
        }
        case WM_MOUSELEAVE: {
            lastHoverIndex = -1;
            break;
        }
    }
    return CallWindowProc(g_originalComboProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK AppearanceDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            const MenuTexts* texts = GetMenuTexts();
            
            g_originalFontColor = g_timerState.fontColor;
            g_originalBackgroundColor = g_timerState.backgroundColor;
            g_originalOpacity = g_timerState.windowOpacity;
            g_originalTransparentBackground = g_timerState.transparentBackground;
            
            g_tempFontColor = g_timerState.fontColor;
            g_tempBackgroundColor = g_timerState.backgroundColor;
            g_tempOpacity = g_timerState.windowOpacity;
            
            wcscpy_s(g_originalFontName, 256, g_timerState.currentFontName);
            g_fontPreviewActive = FALSE;
            
            // 字体选择下拉框
            HWND hFontCombo = CreateWindowW(L"COMBOBOX", L"", 
                WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                210, 50, 180, 200, hDlg, (HMENU)ID_APPEARANCE_FONT_COMBO, GetModuleHandle(NULL), NULL);
            
            if (hFontCombo) {
                if (InitializeFontResourceSystem()) {
                    wchar_t fontNames[32][256];
                    int fontCount = GetMixedFontList(fontNames, 32);
                    for (int i = 0; i < fontCount; i++) {
                        SendMessageW(hFontCombo, CB_ADDSTRING, 0, (LPARAM)fontNames[i]);
                    }
                } else {
                    for (int i = 0; i < GetAvailableFontCount(); i++) {
                        const FontInfo* fontInfo = GetFontInfoByIndex(i);
                        if (fontInfo) SendMessageW(hFontCombo, CB_ADDSTRING, 0, (LPARAM)fontInfo->displayName);
                    }
                }
                int currentIndex = SendMessageW(hFontCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)g_timerState.currentFontName);
                if (currentIndex == CB_ERR) currentIndex = 0;
                SendMessageW(hFontCombo, CB_SETCURSEL, currentIndex, 0);
                
                g_fontComboBox = hFontCombo;
                g_originalComboProc = (WNDPROC)SetWindowLongPtr(hFontCombo, GWLP_WNDPROC, (LONG_PTR)FontComboSubclassProc);
                
                TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hFontCombo, 0};
                TrackMouseEvent(&tme);
            }

            // 字体颜色按钮 (圆形)
            CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                350, 105, 36, 36, hDlg, (HMENU)ID_APPEARANCE_FONT_COLOR, GetModuleHandle(NULL), NULL);

            // 背景透明开关
            CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                340, 210, 46, 24, hDlg, (HMENU)ID_APPEARANCE_TRANSPARENT_BG, GetModuleHandle(NULL), NULL);

            // 背景颜色按钮 (圆形)
            CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                350, 265, 36, 36, hDlg, (HMENU)ID_APPEARANCE_BG_COLOR, GetModuleHandle(NULL), NULL);

            // 透明度滑块 - 静态控件子类化
            HWND hOpacitySlider = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_NOTIFY,
                60, 340, 290, 30, hDlg, (HMENU)ID_APPEARANCE_OPACITY_SLIDER, GetModuleHandle(NULL), NULL);
            if (hOpacitySlider) {
                g_originalSliderProc = (WNDPROC)SetWindowLongPtr(hOpacitySlider, GWLP_WNDPROC, (LONG_PTR)OpacitySliderSubclassProc);
            }
            
            // 透明度值标签
            wchar_t opacityText[16];
            swprintf(opacityText, 16, L"%d%%", (int)((g_timerState.windowOpacity / 255.0) * 100));
            CreateWindowW(L"STATIC", opacityText, WS_VISIBLE | WS_CHILD | SS_RIGHT,
                350, 345, 40, 20, hDlg, (HMENU)ID_APPEARANCE_OPACITY_LABEL, GetModuleHandle(NULL), NULL);

            // 底部操作按钮
            CreateWindowW(L"BUTTON", texts->resetButton, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                30, 450, 80, 32, hDlg, (HMENU)ID_APPEARANCE_BTN_RESET, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", texts->randomButton, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                120, 450, 80, 32, hDlg, (HMENU)ID_APPEARANCE_BTN_RANDOM, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", texts->cancel, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                260, 450, 80, 32, hDlg, (HMENU)ID_APPEARANCE_BTN_CANCEL, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", texts->ok, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                350, 450, 80, 32, hDlg, (HMENU)ID_APPEARANCE_BTN_OK, GetModuleHandle(NULL), NULL);

            TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hDlg, 0};
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MEASUREITEM: {
            LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
            if (mis->CtlID == ID_APPEARANCE_FONT_COMBO) {
                mis->itemHeight = 32; // Taller items
                return TRUE;
            }
            break;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect; GetClientRect(hDlg, &rect);
            HBRUSH hBrush = CreateSolidBrush(RGB(245, 245, 247));
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);

            // Draw Cards
            // Card 1: Typography
            RECT card1 = {20, 20, 440, 160};
            DrawModernCard(hdc, &card1);
            
            // Card 2: Background
            RECT card2 = {20, 180, 440, 390};
            DrawModernCard(hdc, &card2);

            // Draw Static Texts
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, UI_LIGHT_TEXT_PRIMARY);
            HFONT hFont = CreateFontW(14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            const MenuTexts* texts = GetMenuTexts();
            RECT rcLabel;
            
            // Card 1 Labels
            SetTextColor(hdc, RGB(100, 100, 105)); // Section Title
            rcLabel = (RECT){40, 35, 200, 55}; DrawTextW(hdc, L"字体与颜色 (Typography)", -1, &rcLabel, DT_LEFT);
            SetTextColor(hdc, UI_LIGHT_TEXT_PRIMARY);
            rcLabel = (RECT){40, 65, 200, 85}; DrawTextW(hdc, texts->font, -1, &rcLabel, DT_LEFT);
            rcLabel = (RECT){40, 115, 200, 135}; DrawTextW(hdc, texts->fontColor, -1, &rcLabel, DT_LEFT);

            // Card 2 Labels
            SetTextColor(hdc, RGB(100, 100, 105)); // Section Title
            rcLabel = (RECT){40, 195, 200, 215}; DrawTextW(hdc, L"背景与窗口 (Background)", -1, &rcLabel, DT_LEFT);
            SetTextColor(hdc, UI_LIGHT_TEXT_PRIMARY);
            rcLabel = (RECT){40, 225, 280, 245}; DrawTextW(hdc, texts->transparentBackground, -1, &rcLabel, DT_LEFT);
            rcLabel = (RECT){40, 275, 200, 295}; DrawTextW(hdc, texts->backgroundColor, -1, &rcLabel, DT_LEFT);
            rcLabel = (RECT){40, 325, 200, 345}; DrawTextW(hdc, texts->opacity, -1, &rcLabel, DT_LEFT);

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
            return TRUE;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, UI_LIGHT_TEXT_PRIMARY);
            SetBkColor(hdcStatic, RGB(255, 255, 255)); // Inside cards
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            
            if (dis->CtlID == ID_APPEARANCE_FONT_COMBO) {
                if (dis->itemID != -1) {
                    wchar_t fontName[256];
                    SendMessageW(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)fontName);
                    
                    // 背景
                    COLORREF bgColor = (dis->itemState & ODS_SELECTED) ? UI_PRIMARY_COLOR : RGB(255,255,255);
                    HBRUSH hBg = CreateSolidBrush(bgColor);
                    FillRect(dis->hDC, &dis->rcItem, hBg);
                    DeleteObject(hBg);
                    
                    // 文本
                    SetBkMode(dis->hDC, TRANSPARENT);
                    COLORREF textColor = (dis->itemState & ODS_SELECTED) ? RGB(255,255,255) : UI_LIGHT_TEXT_PRIMARY;
                    SetTextColor(dis->hDC, textColor);
                    
                    // 使用实际字体渲染预览
                    HFONT hItemFont = CreateFontW(18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,fontName);
                    HFONT hOldFont = (HFONT)SelectObject(dis->hDC, hItemFont);
                    
                    RECT textRect = dis->rcItem;
                    textRect.left += 10;
                    DrawTextW(dis->hDC, fontName, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    
                    SelectObject(dis->hDC, hOldFont);
                    DeleteObject(hItemFont);
                }
                return TRUE;
            }
            
            if (dis->CtlID == ID_APPEARANCE_TRANSPARENT_BG) {
                // Clear background
                HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
                FillRect(dis->hDC, &dis->rcItem, bgBrush);
                DeleteObject(bgBrush);
                DrawSwitch(dis->hDC, &dis->rcItem, g_timerState.transparentBackground);
                return TRUE;
            }
            
            if (dis->CtlID == ID_APPEARANCE_FONT_COLOR || dis->CtlID == ID_APPEARANCE_BG_COLOR) {
                COLORREF color = (dis->CtlID == ID_APPEARANCE_FONT_COLOR) ? g_tempFontColor : g_tempBackgroundColor;
                HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
                FillRect(dis->hDC, &dis->rcItem, bgBrush);
                DeleteObject(bgBrush);
                
                HBRUSH hColorBrush = CreateSolidBrush(color);
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
                HGDIOBJ hOldB = SelectObject(dis->hDC, hColorBrush);
                HGDIOBJ hOldP = SelectObject(dis->hDC, hPen);
                
                Ellipse(dis->hDC, dis->rcItem.left + 2, dis->rcItem.top + 2, dis->rcItem.right - 2, dis->rcItem.bottom - 2);
                
                SelectObject(dis->hDC, hOldB);
                SelectObject(dis->hDC, hOldP);
                DeleteObject(hColorBrush);
                DeleteObject(hPen);
                return TRUE;
            }

            if (dis->CtlID == ID_APPEARANCE_BTN_OK || dis->CtlID == ID_APPEARANCE_BTN_CANCEL ||
                dis->CtlID == ID_APPEARANCE_BTN_RESET || dis->CtlID == ID_APPEARANCE_BTN_RANDOM) {

                BOOL isHover = (g_hAppearanceHoverBtn == dis->hwndItem);
                BOOL isPressed = (g_hAppearancePressedBtn == dis->hwndItem);
                BOOL isOK = (dis->CtlID == ID_APPEARANCE_BTN_OK);

                COLORREF fillColor = isOK ? UI_PRIMARY_COLOR : RGB(235, 235, 240);
                if (isPressed) fillColor = isOK ? UI_PRIMARY_PRESSED : RGB(215, 215, 220);
                else if (isHover) fillColor = isOK ? UI_PRIMARY_HOVER : RGB(225, 225, 230);

                COLORREF borderColor = isOK ? UI_PRIMARY_COLOR : RGB(220, 220, 225);
                int radius = 8;
                RECT btnRect = dis->rcItem;
                if (isPressed) { btnRect.top++; btnRect.bottom++; }

                // Dialog background is light gray
                HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 247));
                FillRect(dis->hDC, &dis->rcItem, bgBrush);
                DeleteObject(bgBrush);

                DrawRoundRect(dis->hDC, &btnRect, radius, fillColor, borderColor, 1);

                const wchar_t* btnText = NULL;
                const MenuTexts* texts = GetMenuTexts();
                switch (dis->CtlID) {
                    case ID_APPEARANCE_BTN_OK: btnText = texts->ok; break;
                    case ID_APPEARANCE_BTN_CANCEL: btnText = texts->cancel; break;
                    case ID_APPEARANCE_BTN_RESET: btnText = texts->resetButton; break;
                    case ID_APPEARANCE_BTN_RANDOM: btnText = texts->randomButton; break;
                }

                if (btnText) {
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, isOK ? RGB(255, 255, 255) : UI_LIGHT_TEXT_PRIMARY);
                    DrawTextW(dis->hDC, btnText, -1, &btnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
                return TRUE;
            }
            return TRUE;
        }

        case WM_COMMAND: {
            if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == ID_APPEARANCE_FONT_COMBO) {
                HWND hFontCombo = GetDlgItem(hDlg, ID_APPEARANCE_FONT_COMBO);
                if (hFontCombo) {
                    int selectedIndex = SendMessageW(hFontCombo, CB_GETCURSEL, 0, 0);
                    if (selectedIndex != CB_ERR) {
                        wchar_t selectedFont[256] = {0};
                        int len = SendMessageW(hFontCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFont);
                        if (len != CB_ERR && len > 0) {
                            // DEBUG LOG
                            FILE* fp = fopen("font_debug.txt", "a");
                            if (fp) {
                                fwprintf(fp, L"CBN_SELCHANGE: index=%d, text=%s\n", selectedIndex, selectedFont);
                                fclose(fp);
                            }
                            
                            wcscpy_s(g_timerState.currentFontName, sizeof(g_timerState.currentFontName)/sizeof(wchar_t), selectedFont);
                            wcscpy_s(g_originalFontName, sizeof(g_originalFontName)/sizeof(wchar_t), selectedFont);
                            ClearFontCache();
                            g_timerState.cachedFontValid = FALSE;
                            g_timerState.cachedHFont = NULL;
                            g_fontSelectionMade = TRUE;
                            if (g_timerState.transparentBackground) {
                                UpdateLayeredWindow_Render();
                            } else {
                                InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                                UpdateWindow(g_timerState.hMainWnd);
                            }
                        }
                    }
                }
                return 0;
            }
            
            if (LOWORD(wParam) == ID_APPEARANCE_FONT_COMBO) {
                if (HIWORD(wParam) == CBN_DROPDOWN) {
                    HWND hFontCombo = GetDlgItem(hDlg, ID_APPEARANCE_FONT_COMBO);
                    int curSel = SendMessageW(hFontCombo, CB_GETCURSEL, 0, 0);
                    if (curSel != CB_ERR) {
                        wchar_t selFont[256] = {0};
                        int len = SendMessageW(hFontCombo, CB_GETLBTEXT, curSel, (LPARAM)selFont);
                        if (len != CB_ERR && len > 0) {
                            wcscpy_s(g_originalFontName, sizeof(g_originalFontName)/sizeof(wchar_t), selFont);
                            FILE* fp = fopen("font_debug.txt", "a");
                            if (fp) {
                                fwprintf(fp, L"CBN_DROPDOWN: saved %s\n", selFont);
                                fclose(fp);
                            }
                        } else {
                            wcscpy_s(g_originalFontName, sizeof(g_originalFontName)/sizeof(wchar_t), g_timerState.currentFontName);
                        }
                    } else {
                        wcscpy_s(g_originalFontName, sizeof(g_originalFontName)/sizeof(wchar_t), g_timerState.currentFontName);
                    }
                    g_fontPreviewActive = TRUE;
                    g_fontSelectionMade = FALSE;
                    
                    COMBOBOXINFO cbi = {sizeof(COMBOBOXINFO)};
                    if (GetComboBoxInfo(hFontCombo, &cbi) && cbi.hwndList) {
                        g_fontListBox = cbi.hwndList;
                        g_originalListProc = (WNDPROC)SetWindowLongPtr(g_fontListBox, GWLP_WNDPROC, (LONG_PTR)FontListSubclassProc);
                    }
                } else if (HIWORD(wParam) == CBN_CLOSEUP) {
                    if (g_fontListBox && g_originalListProc) {
                        SetWindowLongPtr(g_fontListBox, GWLP_WNDPROC, (LONG_PTR)g_originalListProc);
                        g_fontListBox = NULL;
                        g_originalListProc = NULL;
                    }
                    
                    g_fontPreviewActive = FALSE;
                    
                    // The simplest and most robust fix: just read whatever the combobox's 
                    // current selection is right now, and apply it to the timer!
                    // This perfectly handles both ESC (selection doesn't change) and clicks (selection changes).
                    HWND hFontCombo = GetDlgItem(hDlg, ID_APPEARANCE_FONT_COMBO);
                    if (hFontCombo) {
                        int curSel = SendMessageW(hFontCombo, CB_GETCURSEL, 0, 0);
                        if (curSel != CB_ERR) {
                            wchar_t finalFont[256] = {0};
                            if (SendMessageW(hFontCombo, CB_GETLBTEXT, curSel, (LPARAM)finalFont) != CB_ERR) {
                                wcscpy_s(g_timerState.currentFontName, sizeof(g_timerState.currentFontName)/sizeof(wchar_t), finalFont);
                                ClearFontCache();
                                g_timerState.cachedFontValid = FALSE;
                                g_timerState.cachedHFont = NULL;
                                
                                if (g_timerState.transparentBackground) {
                                    UpdateLayeredWindow_Render();
                                } else {
                                    InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                                    UpdateWindow(g_timerState.hMainWnd);
                                }
                            }
                        }
                    }
                }
                return 0;
            }
            
            switch (LOWORD(wParam)) {
                case ID_APPEARANCE_TRANSPARENT_BG: {
                    g_timerState.transparentBackground = !g_timerState.transparentBackground;
                    InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_TRANSPARENT_BG), NULL, FALSE);
                    
                    LONG exStyle = GetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE);
                    SetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
                    SetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                    
                    if (g_timerState.transparentBackground) {
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
                    return 0;
                }

                case ID_APPEARANCE_FONT_COLOR: {
                    CHOOSECOLOR cc = {sizeof(CHOOSECOLOR), hDlg, NULL, g_tempFontColor, NULL, CC_FULLOPEN | CC_RGBINIT, 0, 0, NULL};
                    COLORREF customColors[16] = {0}; cc.lpCustColors = customColors;
                    if (ChooseColor(&cc)) {
                        g_tempFontColor = cc.rgbResult;
                        g_timerState.fontColor = g_tempFontColor;
                        InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_FONT_COLOR), NULL, TRUE);
                        if (g_timerState.transparentBackground) {
                            UpdateLayeredWindow_Render();
                        } else {
                            InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                            UpdateWindow(g_timerState.hMainWnd);
                        }
                    }
                    return 0;
                }
                case ID_APPEARANCE_BG_COLOR: {
                    CHOOSECOLOR cc = {sizeof(CHOOSECOLOR), hDlg, NULL, g_tempBackgroundColor, NULL, CC_FULLOPEN | CC_RGBINIT, 0, 0, NULL};
                    COLORREF customColors[16] = {0}; cc.lpCustColors = customColors;
                    if (ChooseColor(&cc)) {
                        g_tempBackgroundColor = cc.rgbResult;
                        g_timerState.backgroundColor = g_tempBackgroundColor;
                        InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_BG_COLOR), NULL, TRUE);
                        if (g_timerState.transparentBackground) {
                            UpdateLayeredWindow_Render();
                        } else {
                            InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                            UpdateWindow(g_timerState.hMainWnd);
                        }
                    }
                    return 0;
                }
                case ID_APPEARANCE_BTN_RESET: {
                    g_tempFontColor = RGB(255, 255, 255);
                    if (!g_timerState.transparentBackground) g_tempBackgroundColor = RGB(0, 0, 0);
                    g_tempOpacity = 255;
                    g_timerState.fontColor = g_tempFontColor;
                    if (!g_timerState.transparentBackground) g_timerState.backgroundColor = g_tempBackgroundColor;
                    g_timerState.windowOpacity = g_tempOpacity;
                    
                    InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_FONT_COLOR), NULL, TRUE);
                    if (!g_timerState.transparentBackground) InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_BG_COLOR), NULL, TRUE);
                    InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_OPACITY_SLIDER), NULL, TRUE);
                    SetWindowTextW(GetDlgItem(hDlg, ID_APPEARANCE_OPACITY_LABEL), L"100%");
                    
                    if (g_timerState.transparentBackground) UpdateLayeredWindow_Render();
                    else {
                        SetLayeredWindowAttributes(g_timerState.hMainWnd, 0, g_tempOpacity, LWA_ALPHA);
                        InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                        UpdateWindow(g_timerState.hMainWnd);
                    }
                    return 0;
                }
                case ID_APPEARANCE_BTN_RANDOM: {
                    g_tempFontColor = RGB(rand() % 256, rand() % 256, rand() % 256);
                    if (!g_timerState.transparentBackground) g_tempBackgroundColor = RGB(rand() % 256, rand() % 256, rand() % 256);
                    g_tempOpacity = 150 + (rand() % 106);
                    g_timerState.fontColor = g_tempFontColor;
                    if (!g_timerState.transparentBackground) g_timerState.backgroundColor = g_tempBackgroundColor;
                    g_timerState.windowOpacity = g_tempOpacity;
                    
                    InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_FONT_COLOR), NULL, TRUE);
                    if (!g_timerState.transparentBackground) InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_BG_COLOR), NULL, TRUE);
                    InvalidateRect(GetDlgItem(hDlg, ID_APPEARANCE_OPACITY_SLIDER), NULL, TRUE);
                    wchar_t opacityText[16]; swprintf(opacityText, 16, L"%d%%", (int)((g_tempOpacity / 255.0) * 100));
                    SetWindowTextW(GetDlgItem(hDlg, ID_APPEARANCE_OPACITY_LABEL), opacityText);
                    
                    if (g_timerState.transparentBackground) UpdateLayeredWindow_Render();
                    else {
                        SetLayeredWindowAttributes(g_timerState.hMainWnd, 0, g_tempOpacity, LWA_ALPHA);
                        InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                        UpdateWindow(g_timerState.hMainWnd);
                    }
                    return 0;
                }
                case ID_APPEARANCE_BTN_OK: {
                    g_timerState.fontColor = g_tempFontColor;
                    g_timerState.backgroundColor = g_tempBackgroundColor;
                    g_timerState.windowOpacity = g_tempOpacity;
                    SaveAppearanceConfig();
                    DestroyWindow(hDlg);
                    return 0;
                }
                case ID_APPEARANCE_BTN_CANCEL: {
                    g_timerState.fontColor = g_originalFontColor;
                    g_timerState.backgroundColor = g_originalBackgroundColor;
                    g_timerState.windowOpacity = g_originalOpacity;
                    g_timerState.transparentBackground = g_originalTransparentBackground;
                    ClearFontCache();
                    g_timerState.cachedFontValid = FALSE;
                    g_timerState.cachedHFont = NULL;
                    LONG exStyle = GetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE);
                    SetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
                    SetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                    if (g_timerState.transparentBackground) {
                        SetWindowRoundedCorners(g_timerState.hMainWnd, 0);
                        SetWindowShadow(g_timerState.hMainWnd, FALSE);
                        UpdateLayeredWindow_Render();
                    } else {
                        SetWindowRoundedCorners(g_timerState.hMainWnd, 6);
                        SetWindowShadow(g_timerState.hMainWnd, TRUE);
                        SetLayeredWindowAttributes(g_timerState.hMainWnd, 0, g_originalOpacity, LWA_ALPHA);
                        InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                        UpdateWindow(g_timerState.hMainWnd);
                    }
                    DestroyWindow(hDlg);
                    return 0;
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (g_isAppearanceDlgDragging) {
                POINT currentPos; GetCursorPos(&currentPos);
                int deltaX = currentPos.x - (g_appearanceDlgRectStart.left + g_appearanceDlgDragStart.x);
                int deltaY = currentPos.y - (g_appearanceDlgRectStart.top + g_appearanceDlgDragStart.y);
                SetWindowPos(hDlg, NULL, g_appearanceDlgRectStart.left + deltaX, g_appearanceDlgRectStart.top + deltaY,
                           0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hHoverCtrl = ChildWindowFromPoint(hDlg, pt);
            if (hHoverCtrl && (GetWindowLong(hHoverCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                if (g_hAppearanceHoverBtn != hHoverCtrl) {
                    g_hAppearanceHoverBtn = hHoverCtrl;
                    g_hoverBtnId = GetWindowLongPtr(hHoverCtrl, GWLP_ID);
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            } else {
                if (g_hAppearanceHoverBtn != NULL) {
                    g_hAppearanceHoverBtn = NULL;
                    g_hoverBtnId = 0;
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hClickedCtrl = ChildWindowFromPoint(hDlg, pt);
            if (hClickedCtrl && (GetWindowLong(hClickedCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                g_hAppearancePressedBtn = hClickedCtrl;
                InvalidateRect(hDlg, NULL, FALSE);
            } else if (hClickedCtrl == NULL || hClickedCtrl == hDlg || GetWindowLong(hClickedCtrl, GWL_STYLE) & SS_LEFT) {
                g_isAppearanceDlgDragging = TRUE;
                g_appearanceDlgDragStart = pt;
                GetWindowRect(hDlg, &g_appearanceDlgRectStart);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_isAppearanceDlgDragging) { g_isAppearanceDlgDragging = FALSE; ReleaseCapture(); }
            if (g_hAppearancePressedBtn != NULL) { g_hAppearancePressedBtn = NULL; InvalidateRect(hDlg, NULL, FALSE); }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hAppearanceHoverBtn != NULL) {
                g_hAppearanceHoverBtn = NULL;
                g_hoverBtnId = 0;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        case WM_CHAR:
        case WM_KEYDOWN: {
            int key = (message == WM_CHAR) ? wParam : wParam;
            if (key == 27 || key == VK_ESCAPE) {
                g_timerState.fontColor = g_originalFontColor;
                g_timerState.backgroundColor = g_originalBackgroundColor;
                g_timerState.windowOpacity = g_originalOpacity;
                g_timerState.transparentBackground = g_originalTransparentBackground;
                ClearFontCache();
                g_timerState.cachedFontValid = FALSE;
                g_timerState.cachedHFont = NULL;
                LONG exStyle = GetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE);
                SetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
                SetWindowLong(g_timerState.hMainWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                if (g_timerState.transparentBackground) {
                    SetWindowRoundedCorners(g_timerState.hMainWnd, 0);
                    SetWindowShadow(g_timerState.hMainWnd, FALSE);
                    UpdateLayeredWindow_Render();
                } else {
                    SetWindowRoundedCorners(g_timerState.hMainWnd, 6);
                    SetWindowShadow(g_timerState.hMainWnd, TRUE);
                    SetLayeredWindowAttributes(g_timerState.hMainWnd, 0, g_originalOpacity, LWA_ALPHA);
                    InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                    UpdateWindow(g_timerState.hMainWnd);
                }
                DestroyWindow(hDlg);
                return (message == WM_CHAR) ? TRUE : 0;
            } else if (key == 13 || key == VK_RETURN) {
                g_timerState.fontColor = g_tempFontColor;
                g_timerState.backgroundColor = g_tempBackgroundColor;
                g_timerState.windowOpacity = g_tempOpacity;
                SaveAppearanceConfig();
                DestroyWindow(hDlg);
                return (message == WM_CHAR) ? TRUE : 0;
            }
            break;
        }

        case WM_DESTROY:
            g_hAppearanceDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
    }
    return DefWindowProcW(hDlg, message, wParam, lParam);
}
