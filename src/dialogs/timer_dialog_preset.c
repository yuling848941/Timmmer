#include "timer_dialog_internal.h"

static HWND g_hPresetHoverBtn = NULL;
static HWND g_hPresetPressedBtn = NULL;

// 预设时间编辑对话框
static HWND g_hPresetEditDialog = NULL;

void ShowPresetEditDialog(void) {
    if (g_hPresetEditDialog) {
        SetForegroundWindow(g_hPresetEditDialog);
        return;
    }
    
    CreatePresetEditDialog();
}

void CreatePresetEditDialog(void) {
    // 注册窗口类（只注册一次）
    static BOOL classRegistered = FALSE;
    if (!classRegistered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = PresetEditDialogProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"PresetEditDialogClass";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        
        if (RegisterClassW(&wc)) {
            classRegistered = TRUE;
        }
    }
    
    const MenuTexts* texts = GetMenuTexts();
    
    g_hPresetEditDialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"PresetEditDialogClass",
        texts->presetEditTitle,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        0, 0, 450, 350,
        g_timerState.hMainWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_hPresetEditDialog) {
        return;
    }
    
    // 禁用主窗口
    EnableWindow(g_timerState.hMainWnd, FALSE);
    
    // 居中显示
    RECT parentRect, dialogRect;
    GetWindowRect(g_timerState.hMainWnd, &parentRect);
    GetWindowRect(g_hPresetEditDialog, &dialogRect);
    
    int x = parentRect.left + (parentRect.right - parentRect.left - (dialogRect.right - dialogRect.left)) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - (dialogRect.bottom - dialogRect.top)) / 2;
    
    SetWindowPos(g_hPresetEditDialog, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    SetFocus(g_hPresetEditDialog);
}

LRESULT CALLBACK PresetEditDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            const MenuTexts* texts = GetMenuTexts();
            
            // 创建新预设输入区域
            CreateWindowW(L"STATIC", texts->newPreset,
                WS_VISIBLE | WS_CHILD,
                20, 20, 120, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);
            
            CreateWindowW(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_NUMBER,
                150, 18, 80, 25, hDlg, (HMENU)ID_PRESET_EDIT_NEW, GetModuleHandle(NULL), NULL);
            
            CreateWindowW(L"BUTTON", texts->addPreset,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                240, 18, 60, 25, hDlg, (HMENU)ID_PRESET_EDIT_ADD, GetModuleHandle(NULL), NULL);
            
            // 创建预设列表标签
            CreateWindowW(L"STATIC", texts->presetList,
                WS_VISIBLE | WS_CHILD,
                20, 60, 100, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);
            
            // 创建预设列表框
            CreateWindowW(L"LISTBOX", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                20, 85, 280, 180, hDlg, (HMENU)ID_PRESET_EDIT_LIST, GetModuleHandle(NULL), NULL);
            
            // 创建操作按钮
            CreateWindowW(L"BUTTON", texts->deletePreset,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                320, 85, 80, 30, hDlg, (HMENU)ID_PRESET_EDIT_DELETE, GetModuleHandle(NULL), NULL);
            
            CreateWindowW(L"BUTTON", texts->modifyPreset,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                320, 125, 80, 30, hDlg, (HMENU)ID_PRESET_EDIT_MODIFY, GetModuleHandle(NULL), NULL);
            
            // 创建确定和取消按钮
            CreateWindowW(L"BUTTON", texts->ok,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                280, 280, 60, 30, hDlg, (HMENU)ID_PRESET_EDIT_OK, GetModuleHandle(NULL), NULL);
            
            CreateWindowW(L"BUTTON", texts->cancel,
                WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                350, 280, 60, 30, hDlg, (HMENU)ID_PRESET_EDIT_CANCEL, GetModuleHandle(NULL), NULL);
            
            // 创建双击切换提示
            CreateWindowW(L"STATIC", texts->doubleClickHint,
                WS_VISIBLE | WS_CHILD,
                20, 285, 250, 40, hDlg, NULL, GetModuleHandle(NULL), NULL);
            
            // 填充预设列表
            HWND hListBox = GetDlgItem(hDlg, ID_PRESET_EDIT_LIST);
            for (int i = 0; i < g_timerState.presetCount; i++) {
                wchar_t presetText[64];
                swprintf(presetText, 64, L"%d 分钟", g_timerState.presetTimes[i]);
                SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)presetText);
            }
            
            return 0;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_PRESET_EDIT_ADD: {
                    wchar_t buffer[16];
                    GetWindowTextW(GetDlgItem(hDlg, ID_PRESET_EDIT_NEW), buffer, 16);
                    int minutes = _wtoi(buffer);
                    
                    if (minutes > 0 && minutes <= 999) {
                        AddPresetTime(minutes);
                        
                        // 更新列表框
                        HWND hListBox = GetDlgItem(hDlg, ID_PRESET_EDIT_LIST);
                        SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
                        for (int i = 0; i < g_timerState.presetCount; i++) {
                            wchar_t presetText[64];
                            swprintf(presetText, 64, L"%d 分钟", g_timerState.presetTimes[i]);
                            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)presetText);
                        }
                        
                        // 清空输入框
                        SetWindowTextW(GetDlgItem(hDlg, ID_PRESET_EDIT_NEW), L"");
                    }
                    break;
                }
                
                case ID_PRESET_EDIT_DELETE: {
                    HWND hListBox = GetDlgItem(hDlg, ID_PRESET_EDIT_LIST);
                    int selectedIndex = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                    
                    if (selectedIndex != LB_ERR) {
                        DeletePresetTime(selectedIndex);
                        
                        // 更新列表框
                        SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
                        for (int i = 0; i < g_timerState.presetCount; i++) {
                            wchar_t presetText[64];
                            swprintf(presetText, 64, L"%d 分钟", g_timerState.presetTimes[i]);
                            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)presetText);
                        }
                    }
                    break;
                }
                
                case ID_PRESET_EDIT_MODIFY: {
                    HWND hListBox = GetDlgItem(hDlg, ID_PRESET_EDIT_LIST);
                    int selectedIndex = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                    
                    if (selectedIndex != LB_ERR) {
                        wchar_t buffer[16];
                        GetWindowTextW(GetDlgItem(hDlg, ID_PRESET_EDIT_NEW), buffer, 16);
                        int newMinutes = _wtoi(buffer);
                        
                        if (newMinutes > 0 && newMinutes <= 999) {
                            ModifyPresetTime(selectedIndex, newMinutes);
                            
                            // 更新列表框
                            SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
                            for (int i = 0; i < g_timerState.presetCount; i++) {
                                wchar_t presetText[64];
                                swprintf(presetText, 64, L"%d 分钟", g_timerState.presetTimes[i]);
                                SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)presetText);
                            }
                            
                            // 清空输入框
                            SetWindowTextW(GetDlgItem(hDlg, ID_PRESET_EDIT_NEW), L"");
                        }
                    }
                    break;
                }
                
                case ID_PRESET_EDIT_LIST: {
                    if (HIWORD(wParam) == LBN_SELCHANGE) {
                        // 当选择列表项时，将其值填入输入框
                        HWND hListBox = GetDlgItem(hDlg, ID_PRESET_EDIT_LIST);
                        int selectedIndex = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                        
                        if (selectedIndex != LB_ERR && selectedIndex < g_timerState.presetCount) {
                            wchar_t buffer[16];
                            swprintf(buffer, 16, L"%d", g_timerState.presetTimes[selectedIndex]);
                            SetWindowTextW(GetDlgItem(hDlg, ID_PRESET_EDIT_NEW), buffer);
                        }
                    }
                    break;
                }
                
                case ID_PRESET_EDIT_OK:
                case ID_PRESET_EDIT_CANCEL:
                    DestroyWindow(hDlg);
                    return 0;
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

        // 按钮悬停跟踪
        case WM_MOUSEMOVE: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hHoverCtrl = ChildWindowFromPoint(hDlg, pt);

            if (hHoverCtrl && (GetWindowLong(hHoverCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                if (g_hPresetHoverBtn != hHoverCtrl) {
                    g_hPresetHoverBtn = hHoverCtrl;
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            } else {
                if (g_hPresetHoverBtn != NULL) {
                    g_hPresetHoverBtn = NULL;
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            // 检测是否点击了按钮，如果是则设置按下状态
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hHoverCtrl = ChildWindowFromPoint(hDlg, pt);
            if (hHoverCtrl && (GetWindowLong(hHoverCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                g_hPresetPressedBtn = hHoverCtrl;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            // 清除按钮按下状态并重绘
            if (g_hPresetPressedBtn != NULL) {
                g_hPresetPressedBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hPresetHoverBtn != NULL) {
                g_hPresetHoverBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        // 绘制按钮
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

            if (dis->CtlID == ID_PRESET_EDIT_ADD || dis->CtlID == ID_PRESET_EDIT_DELETE ||
                dis->CtlID == ID_PRESET_EDIT_MODIFY || dis->CtlID == ID_PRESET_EDIT_OK ||
                dis->CtlID == ID_PRESET_EDIT_CANCEL) {

                BOOL isHover = (g_hPresetHoverBtn == dis->hwndItem);
                BOOL isPressed = (g_hPresetPressedBtn == dis->hwndItem);
                BOOL isPrimary = (dis->CtlID == ID_PRESET_EDIT_OK);

                // 确定按钮：蓝色背景，悬停时更深，按下时最深；其他按钮：浅灰背景，悬停时中灰，按下时更深
                COLORREF fillColor = isPrimary ? UI_PRIMARY_COLOR : UI_LIGHT_BG_SECONDARY;
                if (isPressed) {
                    fillColor = isPrimary ? UI_PRIMARY_PRESSED : UI_LIGHT_BUTTON_PRESSED;
                } else if (isHover) {
                    fillColor = isPrimary ? UI_PRIMARY_HOVER : UI_LIGHT_BUTTON_HOVER;
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
                    case ID_PRESET_EDIT_ADD: btnText = texts->addPreset; break;
                    case ID_PRESET_EDIT_DELETE: btnText = texts->deletePreset; break;
                    case ID_PRESET_EDIT_MODIFY: btnText = texts->modifyPreset; break;
                    case ID_PRESET_EDIT_OK: btnText = texts->ok; break;
                    case ID_PRESET_EDIT_CANCEL: btnText = texts->cancel; break;
                }

                if (btnText) {
                    SetBkMode(dis->hDC, TRANSPARENT);
                    COLORREF textColor = isPrimary ? RGB(255, 255, 255) : UI_LIGHT_TEXT_PRIMARY;
                    SetTextColor(dis->hDC, textColor);
                    // 计算文本居中位置（按下时也偏移 1 像素）
                    RECT textRect = btnRect;
                    DrawTextW(dis->hDC, btnText, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }

                return TRUE;
            }
            return TRUE;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hDlg);
                return 0;
            }
            break;
        }
        
        case WM_DESTROY:
            g_hPresetEditDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
    }
    
    return DefWindowProcW(hDlg, message, wParam, lParam);
}
