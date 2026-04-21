#include "timer_dialog_internal.h"

static HWND g_hSetTimeHoverBtn = NULL;
static HWND g_hSetTimePressedBtn = NULL;
static HWND g_hSetTimeDialog = NULL;
static BOOL g_isSetTimeDlgDragging = FALSE;
static POINT g_setTimeDlgDragStart;
static RECT g_setTimeDlgRectStart;
static WNDPROC g_originalEditProcs[3] = {NULL, NULL, NULL};
static HWND g_hParentDialog = NULL;
static HWND g_hEditControls[3] = {NULL, NULL, NULL};

HWND GetSetTimeDialog(void) {
    return g_hSetTimeDialog;
}

// 时间设置对话框
void ShowSetTimeDialog() {
    ShowIntegratedDialog();
}

void CreateSetTimeDialog() {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = SetTimeDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"SetTimeDialogClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    
    RegisterClassW(&wc);
    
    const MenuTexts* texts = GetMenuTexts();
    
    g_hSetTimeDialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"SetTimeDialogClass",
        texts->setTimeTitle,
        WS_POPUP | WS_SYSMENU | WS_VISIBLE,
        0, 0, 320, 160,
        g_timerState.hMainWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (!g_hSetTimeDialog) {
        return;
    }

    // 应用现代样式（圆角 + 阴影）
    ApplyModernDialogStyle(g_hSetTimeDialog);

    EnableWindow(g_timerState.hMainWnd, FALSE);
    
    // 居中显示
    RECT parentRect, dialogRect;
    GetWindowRect(g_timerState.hMainWnd, &parentRect);
    GetWindowRect(g_hSetTimeDialog, &dialogRect);
    
    int x = parentRect.left + (parentRect.right - parentRect.left - (dialogRect.right - dialogRect.left)) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - (dialogRect.bottom - dialogRect.top)) / 2;
    
    SetWindowPos(g_hSetTimeDialog, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    SendMessage(g_hSetTimeDialog, WM_INITDIALOG, 0, 0);
    SetFocus(g_hSetTimeDialog);
}

// 编辑框子类化窗口过程
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_KEYDOWN:
            // 只转发ESC和ENTER键，让系统自己处理TAB键
            if (g_hParentDialog && (wParam == VK_ESCAPE || wParam == VK_RETURN)) {
                if (SendMessage(g_hParentDialog, WM_KEYDOWN, wParam, lParam) == 0) {
                    // 如果对话框处理了消息，返回0
                    return 0;
                }
            }
            break;
        case WM_CHAR:
            // 对于ESC和ENTER，转发给父对话框
            if (wParam == 27 || wParam == 13) { // ESC或ENTER
                if (g_hParentDialog) {
                    if (SendMessage(g_hParentDialog, WM_CHAR, wParam, lParam) == TRUE) {
                        return 0;
                    }
                }
            }
            break;
    }
    
    int i = 0;
    for (i = 0; i < 3; i++) {
        if (g_hEditControls[i] == hwnd && g_originalEditProcs[i]) break;
    }
    WNDPROC origProc = (i < 3) ? g_originalEditProcs[i] : NULL;
    if (origProc) {
        return CallWindowProc(origProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK SetTimeDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    
    switch (message) {
        case WM_GETDLGCODE: {
            // 只处理ESC和ENTER键，让系统自己处理TAB键
            return DLGC_WANTCHARS;
        }
        
        case WM_CREATE: {
            const MenuTexts* texts = GetMenuTexts();
            
            // 创建控件 - 标签在上方，编辑框在下方
            // Hours - 标签在上方
            CreateWindowW(L"STATIC", texts->hours,
                WS_VISIBLE | WS_CHILD,
                30, 20, 60, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);
            
            // Hours - 编辑框在下方
            HWND hHours = CreateWindowW(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_NUMBER,
                30, 45, 60, 25, hDlg, (HMENU)ID_EDIT_HOURS, GetModuleHandle(NULL), NULL);
            
            // Minutes - 标签在上方
            CreateWindowW(L"STATIC", texts->minutes,
                WS_VISIBLE | WS_CHILD,
                110, 20, 60, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);
            
            // Minutes - 编辑框在下方
            HWND hMinutes = CreateWindowW(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_NUMBER,
                110, 45, 60, 25, hDlg, (HMENU)ID_EDIT_MINUTES, GetModuleHandle(NULL), NULL);
            
            // Seconds - 标签在上方
            CreateWindowW(L"STATIC", texts->seconds,
                WS_VISIBLE | WS_CHILD,
                190, 20, 60, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);
            
            // Seconds - 编辑框在下方
            HWND hSeconds = CreateWindowW(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_NUMBER,
                190, 45, 60, 25, hDlg, (HMENU)ID_EDIT_SECONDS, GetModuleHandle(NULL), NULL);
            
            // 按钮位置向下调整
            CreateWindowW(L"BUTTON", texts->ok,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                100, 90, 80, 30, hDlg, (HMENU)ID_BUTTON_OK, GetModuleHandle(NULL), NULL);
            
            CreateWindowW(L"BUTTON", texts->cancel,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                200, 90, 80, 30, hDlg, (HMENU)ID_BUTTON_CANCEL, GetModuleHandle(NULL), NULL);
            
            // 设置父对话框句柄用于子类化
            g_hParentDialog = hDlg;
            g_hEditControls[0] = hHours;
            g_hEditControls[1] = hMinutes;
            g_hEditControls[2] = hSeconds;
            
            for (int i = 0; i < 3; i++) {
                if (g_hEditControls[i]) {
                    g_originalEditProcs[i] = (WNDPROC)SetWindowLongPtr(
                        g_hEditControls[i], GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
                }
            }

            // 启用鼠标跟踪
            TRACKMOUSEEVENT tme = {0};
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hDlg;
            TrackMouseEvent(&tme);

            return 0;
        }

        // 鼠标左键按下 - 开始拖拽
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            POINT pt = {x, y};
            HWND hClickedCtrl = ChildWindowFromPoint(hDlg, pt);

            // 如果点击的是按钮，设置按下状态
            if (hClickedCtrl && (GetWindowLong(hClickedCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                g_hSetTimePressedBtn = hClickedCtrl;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            // 如果点击的是空白区域或静态文本标签，则允许拖动
            else if (hClickedCtrl == NULL || hClickedCtrl == hDlg ||
                (GetWindowLong(hClickedCtrl, GWL_STYLE) & SS_LEFT)) {
                g_isSetTimeDlgDragging = TRUE;
                g_setTimeDlgDragStart = pt;
                GetWindowRect(hDlg, &g_setTimeDlgRectStart);
            }
            return 0;
        }
        
        case WM_INITDIALOG: {
            int totalSeconds = g_timerState.seconds;
            int currentHours = totalSeconds / 3600;
            int currentMinutes = (totalSeconds % 3600) / 60;
            int currentSecs = totalSeconds % 60;
            
            // 获取当前时间格式设置
            BOOL showHours = GetShowHours();
            BOOL showMinutes = GetShowMinutes();
            BOOL showSeconds = GetShowSeconds();
            
            // 根据时间格式设置启用/禁用输入框
            HWND hHoursEdit = GetDlgItem(hDlg, ID_EDIT_HOURS);
            HWND hMinutesEdit = GetDlgItem(hDlg, ID_EDIT_MINUTES);
            HWND hSecondsEdit = GetDlgItem(hDlg, ID_EDIT_SECONDS);
            
            EnableWindow(hHoursEdit, showHours);
            EnableWindow(hMinutesEdit, showMinutes);
            EnableWindow(hSecondsEdit, showSeconds);
            
            // 设置输入框的值（只有启用的输入框才设置值）
            wchar_t buffer[16];
            if (showHours) {
                swprintf(buffer, 16, L"%d", currentHours);
                SetWindowTextW(hHoursEdit, buffer);
            } else {
                SetWindowTextW(hHoursEdit, L"");
            }
            
            if (showMinutes) {
                swprintf(buffer, 16, L"%d", currentMinutes);
                SetWindowTextW(hMinutesEdit, buffer);
            } else {
                SetWindowTextW(hMinutesEdit, L"");
            }
            
            if (showSeconds) {
                swprintf(buffer, 16, L"%d", currentSecs);
                SetWindowTextW(hSecondsEdit, buffer);
            } else {
                SetWindowTextW(hSecondsEdit, L"");
            }
            
            // 设置焦点到第一个启用的输入框并全选内容
            HWND hFocusEdit = NULL;
            if (showHours) {
                hFocusEdit = hHoursEdit;
            } else if (showMinutes) {
                hFocusEdit = hMinutesEdit;
            } else if (showSeconds) {
                hFocusEdit = hSecondsEdit;
            }
            
            if (hFocusEdit) {
                SetFocus(hFocusEdit);
                SendMessage(hFocusEdit, EM_SETSEL, 0, -1);
            }
            
            return FALSE; // 返回FALSE让系统处理焦点设置
        }
        
        case WM_CHAR: {
            switch (wParam) {
                case 27: // ESC键的ASCII码
                    DestroyWindow(hDlg);
                    return TRUE;
                    
                case 13: { // ENTER键的ASCII码
                    SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(ID_BUTTON_OK, BN_CLICKED), 0);
                    return TRUE;
                }
                default:
                    // 让编辑框正常处理其他字符输入
                    break;
            }
            break;
        }
        
        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_ESCAPE:
                    // ESC键取消对话框
                    DestroyWindow(hDlg);
                    return 0; // 返回0表示消息已处理，不再传递
                    
                case VK_RETURN: {
                    // ENTER键确认对话框
                    HWND hFocus = GetFocus();
                    // 如果焦点在编辑框上，执行确认操作
                    if (hFocus == GetDlgItem(hDlg, ID_EDIT_HOURS) ||
                        hFocus == GetDlgItem(hDlg, ID_EDIT_MINUTES) ||
                        hFocus == GetDlgItem(hDlg, ID_EDIT_SECONDS)) {
                        SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(ID_BUTTON_OK, BN_CLICKED), 0);
                        return 0; // 返回0表示消息已处理，不再传递
                    }
                    // 如果焦点在取消按钮上，执行取消操作
                    if (hFocus == GetDlgItem(hDlg, ID_BUTTON_CANCEL)) {
                        DestroyWindow(hDlg);
                        return 0;
                    }
                    // 否则执行确认操作
                    SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(ID_BUTTON_OK, BN_CLICKED), 0);
                    return 0;
                }
                // 删除自定义TAB键处理，让Windows系统自己处理TAB键焦点切换
            }
            break;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {

                    
                case ID_BUTTON_OK: {
                    // 获取当前时间格式设置
                    BOOL showHours = GetShowHours();
                    BOOL showMinutes = GetShowMinutes();
                    BOOL showSeconds = GetShowSeconds();
                    
                    wchar_t buffer[16];
                    int hours = 0, minutes = 0, seconds = 0;
                    
                    // 只从启用的输入框获取值
                    if (showHours) {
                        GetWindowTextW(GetDlgItem(hDlg, ID_EDIT_HOURS), buffer, 16);
                        hours = _wtoi(buffer);
                        
                        // 输入验证
                        if (hours > 99) {
                            const MenuTexts* texts = GetMenuTexts();
                            MessageBoxW(hDlg, texts->errorHoursMax, texts->errorTitle, MB_OK | MB_ICONWARNING);
                            SetFocus(GetDlgItem(hDlg, ID_EDIT_HOURS));
                            return TRUE;
                        }
                    }
                    
                    if (showMinutes) {
                        GetWindowTextW(GetDlgItem(hDlg, ID_EDIT_MINUTES), buffer, 16);
                        minutes = _wtoi(buffer);
                        
                        // 输入验证
                        if (minutes > 60) {
                            const MenuTexts* texts = GetMenuTexts();
                            MessageBoxW(hDlg, texts->errorMinutesMax, texts->errorTitle, MB_OK | MB_ICONWARNING);
                            SetFocus(GetDlgItem(hDlg, ID_EDIT_MINUTES));
                            return TRUE;
                        }
                    }
                    
                    if (showSeconds) {
                        GetWindowTextW(GetDlgItem(hDlg, ID_EDIT_SECONDS), buffer, 16);
                        seconds = _wtoi(buffer);
                        
                        // 输入验证
                        if (seconds > 60) {
                            const MenuTexts* texts = GetMenuTexts();
                            MessageBoxW(hDlg, texts->errorSecondsMax, texts->errorTitle, MB_OK | MB_ICONWARNING);
                            SetFocus(GetDlgItem(hDlg, ID_EDIT_SECONDS));
                            return TRUE;
                        }
                    }
                    
                    int totalSeconds = hours * 3600 + minutes * 60 + seconds;
                    SetTimerSeconds(totalSeconds);
                    
                    DestroyWindow(hDlg);
                    return TRUE;
                }
                case ID_BUTTON_CANCEL:
                    DestroyWindow(hDlg);
                    return TRUE;
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
            GetClientRect(hDlg, &rect);
            HBRUSH hBrush = CreateSolidBrush(UI_LIGHT_BG_PRIMARY);
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
            return TRUE;
        }

        // 按钮悬停跟踪和拖拽
        case WM_MOUSEMOVE: {
            // 拖拽处理
            if (g_isSetTimeDlgDragging) {
                POINT currentPos;
                GetCursorPos(&currentPos);

                int deltaX = currentPos.x - (g_setTimeDlgRectStart.left + g_setTimeDlgDragStart.x);
                int deltaY = currentPos.y - (g_setTimeDlgRectStart.top + g_setTimeDlgDragStart.y);

                SetWindowPos(hDlg, NULL, g_setTimeDlgRectStart.left + deltaX,
                           g_setTimeDlgRectStart.top + deltaY,
                           0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }

            // 按钮悬停跟踪
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hHoverCtrl = ChildWindowFromPoint(hDlg, pt);

            if (hHoverCtrl && (GetWindowLong(hHoverCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                if (g_hSetTimeHoverBtn != hHoverCtrl) {
                    g_hSetTimeHoverBtn = hHoverCtrl;
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            } else {
                if (g_hSetTimeHoverBtn != NULL) {
                    g_hSetTimeHoverBtn = NULL;
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_isSetTimeDlgDragging) {
                g_isSetTimeDlgDragging = FALSE;
                ReleaseCapture();
            }
            // 清除按钮按下状态并重绘
            if (g_hSetTimePressedBtn != NULL) {
                g_hSetTimePressedBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hSetTimeHoverBtn != NULL) {
                g_hSetTimeHoverBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        // 绘制按钮
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

            if (dis->CtlID == ID_BUTTON_OK || dis->CtlID == ID_BUTTON_CANCEL) {
                BOOL isHover = (g_hSetTimeHoverBtn == dis->hwndItem);
                BOOL isPressed = (g_hSetTimePressedBtn == dis->hwndItem);
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
                int radius = 6;

                // 按下时按钮向下偏移 1 像素
                RECT btnRect = dis->rcItem;
                if (isPressed) {
                    btnRect.top += 1;
                    btnRect.bottom += 1;
                }

                DrawRoundRect(dis->hDC, &btnRect, radius, fillColor, borderColor, borderWidth);

                const wchar_t* btnText = NULL;
                const MenuTexts* texts = GetMenuTexts();

                switch (dis->CtlID) {
                    case ID_BUTTON_OK: btnText = texts->ok; break;
                    case ID_BUTTON_CANCEL: btnText = texts->cancel; break;
                }

                if (btnText) {
                    SetBkMode(dis->hDC, TRANSPARENT);
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
            for (int i = 0; i < 3; i++) {
                if (g_hEditControls[i] && g_originalEditProcs[i]) {
                    SetWindowLongPtr(g_hEditControls[i], GWLP_WNDPROC, (LONG_PTR)g_originalEditProcs[i]);
                    g_originalEditProcs[i] = NULL;
                    g_hEditControls[i] = NULL;
                }
            }
            g_hParentDialog = NULL;
            
            g_hSetTimeDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
    }
    return DefWindowProcW(hDlg, message, wParam, lParam);
}
