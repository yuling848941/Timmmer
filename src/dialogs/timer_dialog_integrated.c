#include "timer_dialog_internal.h"

static HWND g_hIntegratedHoverBtn = NULL;
static HWND g_hIntegratedPressedBtn = NULL;
static HWND g_hIntegratedDialog = NULL;
static BOOL g_isIntegratedDlgDragging = FALSE;
static POINT g_integratedDlgDragStart;
static RECT g_integratedDlgRectStart;

HWND GetIntegratedDialog(void) {
    return g_hIntegratedDialog;
}

void UpdateIntegratedCheckboxStates(HWND hDlg) {
    BOOL showSeconds = (GetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_SECONDS), GWLP_USERDATA) == BST_CHECKED);
    BOOL showMinutes = (GetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_MINUTES), GWLP_USERDATA) == BST_CHECKED);
    BOOL showHours = (GetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_HOURS), GWLP_USERDATA) == BST_CHECKED);
    BOOL showMilliseconds = (GetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_MILLISEC), GWLP_USERDATA) == BST_CHECKED);
    
    BOOL enableHours = showMinutes && showSeconds;
    EnableWindow(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_HOURS), enableHours);
    if (!enableHours && showHours) SetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_HOURS), GWLP_USERDATA, BST_UNCHECKED);

    BOOL enableMinutes = !showMilliseconds || showSeconds;
    EnableWindow(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_MINUTES), enableMinutes);
    if (!enableMinutes && showMinutes) SetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_MINUTES), GWLP_USERDATA, BST_UNCHECKED);
}

void UpdateTimeInputStates(HWND hwnd) {
    BOOL showSeconds = (GetWindowLongPtr(GetDlgItem(hwnd, ID_INTEGRATED_CHECK_SECONDS), GWLP_USERDATA) == BST_CHECKED);
    BOOL showMinutes = (GetWindowLongPtr(GetDlgItem(hwnd, ID_INTEGRATED_CHECK_MINUTES), GWLP_USERDATA) == BST_CHECKED);
    BOOL showHours = (GetWindowLongPtr(GetDlgItem(hwnd, ID_INTEGRATED_CHECK_HOURS), GWLP_USERDATA) == BST_CHECKED);
    
    EnableWindow(GetDlgItem(hwnd, ID_INTEGRATED_EDIT_HOURS), showHours);
    EnableWindow(GetDlgItem(hwnd, ID_INTEGRATED_EDIT_MINUTES), showMinutes);
    EnableWindow(GetDlgItem(hwnd, ID_INTEGRATED_EDIT_SECONDS), showSeconds);
}

// ============================================================================
// 整合设置对话框 (时间设置 + 格式设置)
// ============================================================================

void ShowIntegratedDialog(void) {
    if (g_hIntegratedDialog != NULL) {
        SetForegroundWindow(g_hIntegratedDialog);
        return;
    }
    CreateIntegratedDialog();
}

void CreateIntegratedDialog(void) {
    // 注册对话框窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = IntegratedDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"IntegratedDialogClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    // 获取主窗口位置以便居中显示
    RECT rectMain;
    GetWindowRect(g_timerState.hMainWnd, &rectMain);
    int mainWidth = rectMain.right - rectMain.left;
    int mainHeight = rectMain.bottom - rectMain.top;

    int dlgWidth = 500;
    int dlgHeight = 450;
    int x = rectMain.left + (mainWidth - dlgWidth) / 2;
    int y = rectMain.top + (mainHeight - dlgHeight) / 2;

    g_hIntegratedDialog = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        L"IntegratedDialogClass",
        GetMenuTexts()->integratedTitle,
        WS_POPUP,
        x, y, dlgWidth, dlgHeight,
        g_timerState.hMainWnd, NULL, GetModuleHandle(NULL), NULL
    );

    if (g_hIntegratedDialog) {
        // 设置窗口不透明度
        SetLayeredWindowAttributes(g_hIntegratedDialog, 0, 255, LWA_ALPHA);
        
        // 应用现代样式
        ApplyModernDialogStyle(g_hIntegratedDialog);
        
        ShowWindow(g_hIntegratedDialog, SW_SHOW);
        EnableWindow(g_timerState.hMainWnd, FALSE);
    }
}

LRESULT CALLBACK IntegratedDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            const MenuTexts* texts = GetMenuTexts();
            
            // 创建字体
            HFONT hFontTitle = CreateFontW(24, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
            HFONT hFontSection = CreateFontW(18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
            HFONT hFontNormal = CreateFontW(17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
            HFONT hFontLabel = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");

            // 1. 标题
            HWND hMainTitle = CreateWindowW(L"STATIC", texts->integratedTitle, WS_VISIBLE | WS_CHILD, 30, 25, 300, 35, hDlg, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hMainTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            // 2. 时间设置部分
            HWND hSub1 = CreateWindowW(L"STATIC", texts->setTimeTitle, WS_VISIBLE | WS_CHILD, 30, 80, 300, 25, hDlg, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hSub1, WM_SETFONT, (WPARAM)hFontSection, TRUE);

            int xStart = 35, yStart = 115, gap = 150;
            
            // 小时步进器
            HWND hL1 = CreateWindowW(L"STATIC", texts->hours, WS_VISIBLE | WS_CHILD, xStart, yStart, 100, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hL1, WM_SETFONT, (WPARAM)hFontLabel, TRUE);
            CreateWindowW(L"BUTTON", L"-", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, xStart, yStart + 25, 36, 32, hDlg, (HMENU)ID_BTN_HRS_MINUS, GetModuleHandle(NULL), NULL);
            HWND hEditHrs = CreateWindowW(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | ES_NUMBER, xStart + 36, yStart + 25, 50, 32, hDlg, (HMENU)ID_INTEGRATED_EDIT_HOURS, GetModuleHandle(NULL), NULL);
            SendMessage(hEditHrs, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            CreateWindowW(L"BUTTON", L"+", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, xStart + 86, yStart + 25, 36, 32, hDlg, (HMENU)ID_BTN_HRS_PLUS, GetModuleHandle(NULL), NULL);

            // 分钟步进器
            HWND hL2 = CreateWindowW(L"STATIC", texts->minutes, WS_VISIBLE | WS_CHILD, xStart + gap, yStart, 100, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hL2, WM_SETFONT, (WPARAM)hFontLabel, TRUE);
            CreateWindowW(L"BUTTON", L"-", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, xStart + gap, yStart + 25, 36, 32, hDlg, (HMENU)ID_BTN_MIN_MINUS, GetModuleHandle(NULL), NULL);
            HWND hEditMin = CreateWindowW(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | ES_NUMBER, xStart + gap + 36, yStart + 25, 50, 32, hDlg, (HMENU)ID_INTEGRATED_EDIT_MINUTES, GetModuleHandle(NULL), NULL);
            SendMessage(hEditMin, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            CreateWindowW(L"BUTTON", L"+", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, xStart + gap + 86, yStart + 25, 36, 32, hDlg, (HMENU)ID_BTN_MIN_PLUS, GetModuleHandle(NULL), NULL);

            // 秒钟步进器
            HWND hL3 = CreateWindowW(L"STATIC", texts->seconds, WS_VISIBLE | WS_CHILD, xStart + gap * 2, yStart, 100, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hL3, WM_SETFONT, (WPARAM)hFontLabel, TRUE);
            CreateWindowW(L"BUTTON", L"-", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, xStart + gap * 2, yStart + 25, 36, 32, hDlg, (HMENU)ID_BTN_SEC_MINUS, GetModuleHandle(NULL), NULL);
            HWND hEditSec = CreateWindowW(L"EDIT", L"00", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_CENTER | ES_NUMBER, xStart + gap * 2 + 36, yStart + 25, 50, 32, hDlg, (HMENU)ID_INTEGRATED_EDIT_SECONDS, GetModuleHandle(NULL), NULL);
            SendMessage(hEditSec, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            CreateWindowW(L"BUTTON", L"+", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, xStart + gap * 2 + 86, yStart + 25, 36, 32, hDlg, (HMENU)ID_BTN_SEC_PLUS, GetModuleHandle(NULL), NULL);

            // 初始化编辑框数值
            int h = g_timerState.seconds / 3600;
            int m = (g_timerState.seconds % 3600) / 60;
            int s = g_timerState.seconds % 60;
            wchar_t buf[16];
            swprintf(buf, 16, L"%02d", h); SetWindowTextW(hEditHrs, buf);
            swprintf(buf, 16, L"%02d", m); SetWindowTextW(hEditMin, buf);
            swprintf(buf, 16, L"%02d", s); SetWindowTextW(hEditSec, buf);

            // 3. 格式设置部分
            CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ, 30, 200, 440, 1, hDlg, NULL, GetModuleHandle(NULL), NULL);
            HWND hSub2 = CreateWindowW(L"STATIC", texts->formatTitle, WS_VISIBLE | WS_CHILD, 30, 220, 300, 25, hDlg, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(hSub2, WM_SETFONT, (WPARAM)hFontSection, TRUE);

            int checkY = 255;
            HWND hChk1 = CreateWindowW(L"BUTTON", texts->hours, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 45, checkY, 200, 36, hDlg, (HMENU)ID_INTEGRATED_CHECK_HOURS, GetModuleHandle(NULL), NULL);
            HWND hChk2 = CreateWindowW(L"BUTTON", texts->minutes, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 260, checkY, 200, 36, hDlg, (HMENU)ID_INTEGRATED_CHECK_MINUTES, GetModuleHandle(NULL), NULL);
            HWND hChk3 = CreateWindowW(L"BUTTON", texts->seconds, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 45, checkY + 45, 200, 36, hDlg, (HMENU)ID_INTEGRATED_CHECK_SECONDS, GetModuleHandle(NULL), NULL);
            HWND hChk4 = CreateWindowW(L"BUTTON", texts->milliseconds, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 260, checkY + 45, 200, 36, hDlg, (HMENU)ID_INTEGRATED_CHECK_MILLISEC, GetModuleHandle(NULL), NULL);

            // 设置复选框初始状态
            SetWindowLongPtr(hChk1, GWLP_USERDATA, g_timerState.showHours ? BST_CHECKED : BST_UNCHECKED);
            SetWindowLongPtr(hChk2, GWLP_USERDATA, g_timerState.showMinutes ? BST_CHECKED : BST_UNCHECKED);
            SetWindowLongPtr(hChk3, GWLP_USERDATA, g_timerState.showSeconds ? BST_CHECKED : BST_UNCHECKED);
            SetWindowLongPtr(hChk4, GWLP_USERDATA, g_timerState.showMilliseconds ? BST_CHECKED : BST_UNCHECKED);

            // 4. 底部按钮
            CreateWindowW(L"BUTTON", texts->ok, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 265, 375, 100, 36, hDlg, (HMENU)ID_INTEGRATED_BTN_OK, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", texts->cancel, WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 375, 375, 100, 36, hDlg, (HMENU)ID_INTEGRATED_BTN_CANCEL, GetModuleHandle(NULL), NULL);

            UpdateIntegratedCheckboxStates(hDlg);
            UpdateTimeInputStates(hDlg);
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case ID_BTN_HRS_MINUS: case ID_BTN_HRS_PLUS:
                case ID_BTN_MIN_MINUS: case ID_BTN_MIN_PLUS:
                case ID_BTN_SEC_MINUS: case ID_BTN_SEC_PLUS: {
                    int editId = (wmId <= ID_BTN_HRS_PLUS) ? ID_INTEGRATED_EDIT_HOURS : ((wmId <= ID_BTN_MIN_PLUS) ? ID_INTEGRATED_EDIT_MINUTES : ID_INTEGRATED_EDIT_SECONDS);
                    HWND hEdit = GetDlgItem(hDlg, editId);
                    wchar_t buf[16];
                    GetWindowTextW(hEdit, buf, 16);
                    int val = _wtoi(buf);
                    if (wmId == ID_BTN_HRS_MINUS || wmId == ID_BTN_MIN_MINUS || wmId == ID_BTN_SEC_MINUS) {
                        if (val > 0) val--;
                    } else {
                        if (editId == ID_INTEGRATED_EDIT_HOURS) { if (val < 99) val++; }
                        else { if (val < 59) val++; }
                    }
                    swprintf(buf, 16, L"%02d", val);
                    SetWindowTextW(hEdit, buf);
                    return 0;
                }

                case ID_INTEGRATED_CHECK_HOURS: case ID_INTEGRATED_CHECK_MINUTES: 
                case ID_INTEGRATED_CHECK_SECONDS: case ID_INTEGRATED_CHECK_MILLISEC: {
                    HWND hBtn = GetDlgItem(hDlg, wmId);
                    BOOL isChecked = (GetWindowLongPtr(hBtn, GWLP_USERDATA) == BST_CHECKED);
                    SetWindowLongPtr(hBtn, GWLP_USERDATA, isChecked ? BST_UNCHECKED : BST_CHECKED);
                    UpdateIntegratedCheckboxStates(hDlg);
                    UpdateTimeInputStates(hDlg);
                    InvalidateRect(hDlg, NULL, FALSE);
                    return 0;
                }

                case ID_INTEGRATED_BTN_OK: {
                    wchar_t buf[16];
                    GetWindowTextW(GetDlgItem(hDlg, ID_INTEGRATED_EDIT_HOURS), buf, 16); int h = _wtoi(buf);
                    GetWindowTextW(GetDlgItem(hDlg, ID_INTEGRATED_EDIT_MINUTES), buf, 16); int m = _wtoi(buf);
                    GetWindowTextW(GetDlgItem(hDlg, ID_INTEGRATED_EDIT_SECONDS), buf, 16); int s = _wtoi(buf);

                    // 验证
                    if (h > 99 || m > 59 || s > 59) {
                        const MenuTexts* texts = GetMenuTexts();
                        MessageBoxW(hDlg, texts->errorHoursMax, texts->errorTitle, MB_OK | MB_ICONWARNING);
                        return 0;
                    }

                    // 保存状态
                    g_timerState.seconds = h * 3600 + m * 60 + s;
                    g_timerState.showHours = (GetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_HOURS), GWLP_USERDATA) == BST_CHECKED);
                    g_timerState.showMinutes = (GetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_MINUTES), GWLP_USERDATA) == BST_CHECKED);
                    g_timerState.showSeconds = (GetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_SECONDS), GWLP_USERDATA) == BST_CHECKED);
                    g_timerState.showMilliseconds = (GetWindowLongPtr(GetDlgItem(hDlg, ID_INTEGRATED_CHECK_MILLISEC), GWLP_USERDATA) == BST_CHECKED);

                    SaveFormatConfig();
                    InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                    DestroyWindow(hDlg);
                    return 0;
                }

                case ID_INTEGRATED_BTN_CANCEL:
                    DestroyWindow(hDlg);
                    return 0;
            }
            break;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            const MenuTexts* texts = GetMenuTexts();
            BOOL isHover = (g_hIntegratedHoverBtn == dis->hwndItem);
            BOOL isPressed = (dis->itemState & ODS_SELECTED);

            // 1. 步进按钮
            if (dis->CtlID >= ID_BTN_HRS_MINUS && dis->CtlID <= ID_BTN_SEC_PLUS) {
                BOOL isPlus = (dis->CtlID == ID_BTN_HRS_PLUS || dis->CtlID == ID_BTN_MIN_PLUS || dis->CtlID == ID_BTN_SEC_PLUS);
                COLORREF bgColor = isPressed ? RGB(210, 210, 215) : (isHover ? RGB(235, 235, 240) : RGB(245, 245, 247));
                DrawRoundRect(dis->hDC, &dis->rcItem, 4, bgColor, RGB(220, 220, 225), 1);
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, RGB(60, 60, 67));
                DrawTextW(dis->hDC, isPlus ? L"+" : L"-", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }

            // 2. 复选框/开关 (Chips)
            if (dis->CtlID >= ID_INTEGRATED_CHECK_HOURS && dis->CtlID <= ID_INTEGRATED_CHECK_MILLISEC) {
                BOOL isOn = (GetWindowLongPtr(dis->hwndItem, GWLP_USERDATA) == BST_CHECKED);
                BOOL isEnabled = IsWindowEnabled(dis->hwndItem);
                
                const wchar_t* label = (dis->CtlID == ID_INTEGRATED_CHECK_HOURS) ? texts->hours : 
                                     ((dis->CtlID == ID_INTEGRATED_CHECK_MINUTES) ? texts->minutes : 
                                     ((dis->CtlID == ID_INTEGRATED_CHECK_SECONDS) ? texts->seconds : texts->milliseconds));
                
                // 背景
                COLORREF chipBg = isHover ? RGB(235, 235, 240) : RGB(245, 245, 247);
                DrawRoundRect(dis->hDC, &dis->rcItem, 6, chipBg, RGB(230, 230, 235), 1);

                // 开关部分
                int swW = 38, swH = 20;
                int swY = dis->rcItem.top + (dis->rcItem.bottom - dis->rcItem.top - swH) / 2;
                RECT swRect = {dis->rcItem.left + 10, swY, dis->rcItem.left + 10 + swW, swY + swH};
                COLORREF swBg = isOn ? UI_PRIMARY_COLOR : (isHover ? RGB(210, 210, 215) : RGB(230, 230, 235));
                if (!isEnabled) swBg = RGB(240, 240, 245);
                DrawRoundRect(dis->hDC, &swRect, swH / 2, swBg, swBg, 1);

                // 滑块
                int ts = swH - 6;
                int tx = isOn ? (swRect.right - ts - 3) : (swRect.left + 3);
                RECT tRect = {tx, swY + 3, tx + ts, swY + 3 + ts};
                DrawRoundRect(dis->hDC, &tRect, ts / 2, RGB(255, 255, 255), RGB(255, 255, 255), 1);

                // 文本
                RECT tExt = dis->rcItem; tExt.left += 60;
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, isEnabled ? UI_LIGHT_TEXT_PRIMARY : UI_LIGHT_TEXT_DISABLED);
                DrawTextW(dis->hDC, label, -1, &tExt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }

            // 3. 确定/取消按钮
            if (dis->CtlID == ID_INTEGRATED_BTN_OK || dis->CtlID == ID_INTEGRATED_BTN_CANCEL) {
                BOOL isOK = (dis->CtlID == ID_INTEGRATED_BTN_OK);
                COLORREF fill = isOK ? UI_PRIMARY_COLOR : UI_LIGHT_BG_SECONDARY;
                if (isPressed) fill = isOK ? UI_PRIMARY_PRESSED : UI_LIGHT_BUTTON_PRESSED;
                else if (isHover) fill = isOK ? UI_PRIMARY_HOVER : UI_LIGHT_BUTTON_HOVER;
                DrawRoundRect(dis->hDC, &dis->rcItem, 8, fill, UI_LIGHT_BORDER, 1);
                SetTextColor(dis->hDC, isOK ? RGB(255, 255, 255) : UI_LIGHT_TEXT_PRIMARY);
                SetBkMode(dis->hDC, TRANSPARENT);
                DrawTextW(dis->hDC, isOK ? texts->ok : texts->cancel, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            return TRUE;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hHover = ChildWindowFromPoint(hDlg, pt);
            if (hHover && (GetWindowLong(hHover, GWL_STYLE) & BS_OWNERDRAW)) {
                if (g_hIntegratedHoverBtn != hHover) {
                    g_hIntegratedHoverBtn = hHover;
                    InvalidateRect(hDlg, NULL, FALSE);
                    TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hDlg, 0};
                    TrackMouseEvent(&tme);
                }
            } else if (g_hIntegratedHoverBtn) {
                g_hIntegratedHoverBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            
            if (g_isIntegratedDlgDragging) {
                POINT cur; GetCursorPos(&cur);
                SetWindowPos(hDlg, NULL, g_integratedDlgRectStart.left + (cur.x - g_integratedDlgDragStart.x), g_integratedDlgRectStart.top + (cur.y - g_integratedDlgDragStart.y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (ChildWindowFromPoint(hDlg, pt) == hDlg) {
                g_isIntegratedDlgDragging = TRUE;
                GetCursorPos(&g_integratedDlgDragStart);
                GetWindowRect(hDlg, &g_integratedDlgRectStart);
                SetCapture(hDlg);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_isIntegratedDlgDragging) {
                g_isIntegratedDlgDragging = FALSE;
                ReleaseCapture();
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hIntegratedHoverBtn) {
                g_hIntegratedHoverBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, UI_LIGHT_TEXT_PRIMARY);
            SetBkColor(hdc, UI_LIGHT_BG_PRIMARY);
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }

        case WM_ERASEBKGND: {
            RECT r; GetClientRect(hDlg, &r);
            HBRUSH b = CreateSolidBrush(UI_LIGHT_BG_PRIMARY);
            FillRect((HDC)wParam, &r, b);
            DeleteObject(b);
            return TRUE;
        }

        case WM_DESTROY:
            g_hIntegratedDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
    }
    return DefWindowProcW(hDlg, message, wParam, lParam);
}