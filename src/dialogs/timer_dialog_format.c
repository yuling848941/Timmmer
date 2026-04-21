#include "timer_dialog_internal.h"

static HWND g_hFormatHoverBtn = NULL;
static HWND g_hFormatPressedBtn = NULL;
static HWND g_hFormatDialog = NULL;

HWND GetFormatDialog(void) {
    return g_hFormatDialog;
}

// 格式设置对话框
void ShowFormatDialog() {
    ShowIntegratedDialog();
}

void CreateFormatDialog() {
    // 注册对话框窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = FormatDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"FormatDialogClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassW(&wc);
    
    const MenuTexts* texts = GetMenuTexts();
    
    g_hFormatDialog = CreateWindowW(
        L"FormatDialogClass",
        texts->formatTitle,
        WS_POPUP | WS_VISIBLE | WS_TABSTOP,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        g_timerState.hMainWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (g_hFormatDialog) {
        EnableWindow(g_timerState.hMainWnd, FALSE);
        SetFocus(g_hFormatDialog);
    }
}

// 更新复选框的启用/禁用状态
void UpdateCheckboxStates(HWND hwnd) {
    BOOL showSeconds = (IsDlgButtonChecked(hwnd, ID_CHECK_SECONDS) == BST_CHECKED);
    BOOL showMinutes = (IsDlgButtonChecked(hwnd, ID_CHECK_MINUTES) == BST_CHECKED);
    BOOL showHours = (IsDlgButtonChecked(hwnd, ID_CHECK_HOURS) == BST_CHECKED);
    BOOL showMilliseconds = (IsDlgButtonChecked(hwnd, ID_CHECK_MILLISECONDS) == BST_CHECKED);
    
    // 层级依赖关系逻辑：
    // 1. 如果时间格式仅勾选了"秒"，"小时"的复选框会处于无法选中的状态，必须把"分钟"也勾选了，"小时"才能恢复可勾选状态
    // 2. 如果时间格式仅勾选了"分钟"，"毫秒"的复选框会处于无法选中的状态，必须把"秒"也勾选了，"毫秒"才能恢复可勾选状态  
    // 3. 如果时间格式仅勾选了"毫秒"，"分钟"、"小时"的复选框会处于无法选中的状态，把"秒"也勾选了，"分钟"才能恢复可勾选状态，把"秒"、"分钟"也勾选了，"小时"才能恢复可勾选状态
    
    // 小时的启用条件：必须同时勾选了分钟
    BOOL enableHours = showMinutes;
    
    // 分钟的启用条件：
    // - 如果仅勾选了毫秒（没有勾选秒），分钟不可选
    // - 其他情况下分钟可选
    BOOL enableMinutes = TRUE;
    if (showMilliseconds && !showSeconds) {
        enableMinutes = FALSE;
    }
    
    // 毫秒的启用条件：
    // - 如果仅勾选了分钟（没有勾选秒），毫秒不可选
    // - 必须勾选了秒，毫秒才可选
    BOOL enableMilliseconds = showSeconds;
    
    // 秒始终可以选择（作为基础单位）
    BOOL enableSeconds = TRUE;
    
    // 应用启用/禁用状态
    EnableWindow(GetDlgItem(hwnd, ID_CHECK_HOURS), enableHours);
    EnableWindow(GetDlgItem(hwnd, ID_CHECK_MINUTES), enableMinutes);
    EnableWindow(GetDlgItem(hwnd, ID_CHECK_SECONDS), enableSeconds);
    EnableWindow(GetDlgItem(hwnd, ID_CHECK_MILLISECONDS), enableMilliseconds);
    
    // 如果某个复选框被禁用且当前是选中状态，则自动取消选中
    if (!enableHours && showHours) {
        CheckDlgButton(hwnd, ID_CHECK_HOURS, BST_UNCHECKED);
    }
    if (!enableMinutes && showMinutes) {
        CheckDlgButton(hwnd, ID_CHECK_MINUTES, BST_UNCHECKED);
    }
    if (!enableMilliseconds && showMilliseconds) {
        CheckDlgButton(hwnd, ID_CHECK_MILLISECONDS, BST_UNCHECKED);
    }
}

LRESULT CALLBACK FormatDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_GETDLGCODE: {
            // 告诉系统我们要处理所有键盘消息，包括ESC和Enter键
            return DLGC_WANTALLKEYS;
        }
        
        case WM_CREATE: {
            const MenuTexts* texts = GetMenuTexts();
            
            // 创建复选框
            CreateWindowW(L"BUTTON", texts->hours,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
                20, 30, 150, 25, hwnd, (HMENU)ID_CHECK_HOURS, GetModuleHandle(NULL), NULL);
            
            CreateWindowW(L"BUTTON", texts->minutes,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
                20, 60, 150, 25, hwnd, (HMENU)ID_CHECK_MINUTES, GetModuleHandle(NULL), NULL);
            
            CreateWindowW(L"BUTTON", texts->seconds,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
                20, 90, 150, 25, hwnd, (HMENU)ID_CHECK_SECONDS, GetModuleHandle(NULL), NULL);
                
            CreateWindowW(L"BUTTON", texts->milliseconds,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
                20, 120, 150, 25, hwnd, (HMENU)ID_CHECK_MILLISECONDS, GetModuleHandle(NULL), NULL);
            
            // 创建按钮
            CreateWindowW(L"BUTTON", texts->ok,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                50, 200, 80, 30, hwnd, (HMENU)ID_BUTTON_OK, GetModuleHandle(NULL), NULL);
                
            CreateWindowW(L"BUTTON", texts->cancel,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                150, 200, 80, 30, hwnd, (HMENU)ID_BUTTON_CANCEL, GetModuleHandle(NULL), NULL);
            
            // 设置当前状态
            CheckDlgButton(hwnd, ID_CHECK_HOURS, GetShowHours() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, ID_CHECK_MINUTES, GetShowMinutes() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, ID_CHECK_SECONDS, GetShowSeconds() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, ID_CHECK_MILLISECONDS, GetShowMilliseconds() ? BST_CHECKED : BST_UNCHECKED);
            
            // 更新复选框状态
            UpdateCheckboxStates(hwnd);

            // 启用鼠标跟踪
            TRACKMOUSEEVENT tme = {0};
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            return 0;
        }

        case WM_MOUSEMOVE: {
            // 按钮悬停跟踪
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hHoverCtrl = ChildWindowFromPoint(hwnd, pt);

            if (hHoverCtrl && (GetWindowLong(hHoverCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                if (g_hFormatHoverBtn != hHoverCtrl) {
                    g_hFormatHoverBtn = hHoverCtrl;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            } else {
                if (g_hFormatHoverBtn != NULL) {
                    g_hFormatHoverBtn = NULL;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }

            return 0;
        }

        case WM_LBUTTONDOWN: {
            // 检测是否点击了按钮，如果是则设置按下状态
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hHoverCtrl = ChildWindowFromPoint(hwnd, pt);
            if (hHoverCtrl && (GetWindowLong(hHoverCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                g_hFormatPressedBtn = hHoverCtrl;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            // 清除按钮按下状态并重绘
            if (g_hFormatPressedBtn != NULL) {
                g_hFormatPressedBtn = NULL;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            // 鼠标离开对话框时清除悬停状态
            if (g_hFormatHoverBtn != NULL) {
                g_hFormatHoverBtn = NULL;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_CHECK_HOURS:
                case ID_CHECK_MINUTES:
                case ID_CHECK_SECONDS:
                case ID_CHECK_MILLISECONDS:
                    // 复选框状态改变时更新依赖关系
                    if (HIWORD(wParam) == BN_CLICKED) {
                        UpdateCheckboxStates(hwnd);
                    }
                    break;
                    
                case ID_BUTTON_OK: {
                    // 检查是否至少选择了一个显示选项
                    BOOL showHours = IsDlgButtonChecked(hwnd, ID_CHECK_HOURS) == BST_CHECKED;
                    BOOL showMinutes = IsDlgButtonChecked(hwnd, ID_CHECK_MINUTES) == BST_CHECKED;
                    BOOL showSeconds = IsDlgButtonChecked(hwnd, ID_CHECK_SECONDS) == BST_CHECKED;
                    BOOL showMilliseconds = IsDlgButtonChecked(hwnd, ID_CHECK_MILLISECONDS) == BST_CHECKED;
                    
                    if (!showHours && !showMinutes && !showSeconds && !showMilliseconds) {
                        const MenuTexts* texts = GetMenuTexts();
                        MessageBoxW(hwnd, texts->errorSelectOption, texts->errorTitle, MB_OK | MB_ICONWARNING);
                        return 0;
                    }
                    
                    // 保存设置
                    SetShowHours(showHours);
                    SetShowMinutes(showMinutes);
                    SetShowSeconds(showSeconds);
                    SetShowMilliseconds(showMilliseconds);
                    
                    SaveFormatConfig();
                    
                    // 立即更新显示
                    InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                    
                    DestroyWindow(hwnd);
                    return 0;
                }
                case ID_BUTTON_CANCEL:
                     DestroyWindow(hwnd);
                     return 0;
            }
            break;
        }
        
        case WM_CHAR: {
            switch (wParam) {
                case 27: // ESC键的ASCII码
                    DestroyWindow(hwnd);
                    return TRUE;
                    
                case 13: { // ENTER键的ASCII码
                    SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(ID_BUTTON_OK, BN_CLICKED), 0);
                    return TRUE;
                }
            }
            break;
        }
        
        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_ESCAPE:
                    // ESC键取消对话框
                    DestroyWindow(hwnd);
                    return 0;
                    
                case VK_RETURN: {
                    // ENTER键确认对话框
                    SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(ID_BUTTON_OK, BN_CLICKED), 0);
                    return 0;
                }
            }
            break;
        }

        // 设置静态文本控件背景色
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, UI_LIGHT_TEXT_PRIMARY);
            SetBkColor(hdcStatic, UI_LIGHT_BG_PRIMARY);
            return (LRESULT)CreateSolidBrush(UI_LIGHT_BG_PRIMARY);
        }

        // 设置按钮控件背景色
        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, UI_LIGHT_BG_PRIMARY);
            return (LRESULT)CreateSolidBrush(UI_LIGHT_BG_PRIMARY);
        }

        // 绘制对话框背景
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH hBrush = CreateSolidBrush(UI_LIGHT_BG_PRIMARY);
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
            return TRUE;
        }

        // 绘制按钮
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

            if (dis->CtlID == ID_BUTTON_OK || dis->CtlID == ID_BUTTON_CANCEL) {
                BOOL isHover = (g_hFormatHoverBtn == dis->hwndItem);
                BOOL isPressed = (g_hFormatPressedBtn == dis->hwndItem);
                BOOL isOK = (dis->CtlID == ID_BUTTON_OK);

                // 确定按钮：蓝色背景，悬停时更深，按下时最深；其他按钮：浅灰背景，悬停时中灰，按下时更深
                COLORREF fillColor = isOK ? UI_PRIMARY_COLOR : UI_LIGHT_BG_SECONDARY;
                if (isPressed) {
                    fillColor = isOK ? UI_PRIMARY_PRESSED : UI_LIGHT_BUTTON_PRESSED;
                } else if (isHover) {
                    fillColor = isOK ? UI_PRIMARY_HOVER : UI_LIGHT_BUTTON_HOVER;
                }

                COLORREF borderColor = UI_LIGHT_BORDER;
                int borderWidth = 1;
                int radius = 6; // 6px 圆角

                // 按下时按钮向下偏移 1 像素
                RECT btnRect = dis->rcItem;
                if (isPressed) {
                    btnRect.top += 1;
                    btnRect.bottom += 1;
                }

                DrawRoundRect(dis->hDC, &btnRect, radius, fillColor, borderColor, borderWidth);

                // 绘制按钮文本
                const wchar_t* btnText = NULL;
                const MenuTexts* texts = GetMenuTexts();

                switch (dis->CtlID) {
                    case ID_BUTTON_OK: btnText = texts->ok; break;
                    case ID_BUTTON_CANCEL: btnText = texts->cancel; break;
                }

                if (btnText) {
                    SetBkMode(dis->hDC, TRANSPARENT);

                    // 确定按钮：白色文本；其他按钮：深色文本
                    COLORREF textColor = isOK ? RGB(255, 255, 255) : UI_LIGHT_TEXT_PRIMARY;
                    SetTextColor(dis->hDC, textColor);

                    // 计算文本居中位置（按下时也偏移 1 像素）
                    RECT textRect = btnRect;
                    DrawTextW(dis->hDC, btnText, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }

                return TRUE;
            }
            return TRUE;
        }

        case WM_DESTROY:
            g_hFormatDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
