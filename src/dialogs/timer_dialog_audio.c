#include "timer_dialog_internal.h"

static HWND g_hAudioHoverBtn = NULL;
static HWND g_hAudioPressedBtn = NULL;
static HWND g_hAudioDialog = NULL;
static BOOL g_isAudioDlgDragging = FALSE;
static POINT g_audioDlgDragStart;
static RECT g_audioDlgRectStart;

LRESULT CALLBACK AudioDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    switch (message) {
        case WM_CREATE: {
            const MenuTexts* texts = GetMenuTexts();
            
            // 创建启用音频复选框
            CreateWindowW(L"BUTTON", texts->enableAudio,
                WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                20, 20, 200, 25,
                hDlg, (HMENU)ID_AUDIO_ENABLE_CHECKBOX, GetModuleHandle(NULL), NULL);
            
            // 创建音频语言标签
            CreateWindowW(L"STATIC", texts->audioLanguage,
                WS_VISIBLE | WS_CHILD,
                20, 55, 100, 20,
                hDlg, NULL, GetModuleHandle(NULL), NULL);
            
            // 创建中文音频单选按钮
            CreateWindowW(L"BUTTON", texts->chineseAudio,
                WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                130, 55, 80, 20,
                hDlg, (HMENU)ID_AUDIO_CHINESE_RADIO, GetModuleHandle(NULL), NULL);
            
            // 创建英文音频单选按钮
            CreateWindowW(L"BUTTON", texts->englishAudio,
                WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                220, 55, 80, 20,
                hDlg, (HMENU)ID_AUDIO_ENGLISH_RADIO, GetModuleHandle(NULL), NULL);
            
            // 创建自定义音频单选按钮
            CreateWindowW(L"BUTTON", texts->customAudio,
                WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                310, 55, 100, 20,
                hDlg, (HMENU)ID_AUDIO_CUSTOM_RADIO, GetModuleHandle(NULL), NULL);
            
            // 创建自定义音频路径标签
            CreateWindowW(L"STATIC", L"音频文件路径:",
                WS_VISIBLE | WS_CHILD,
                20, 85, 100, 20,
                hDlg, NULL, GetModuleHandle(NULL), NULL);
            
            // 创建自定义音频路径编辑框
            CreateWindowW(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
                130, 85, 200, 22,
                hDlg, (HMENU)ID_AUDIO_CUSTOM_PATH_EDIT, GetModuleHandle(NULL), NULL);
            
            // 创建浏览按钮
            CreateWindowW(L"BUTTON", texts->browseAudio,
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                340, 85, 70, 22,
                hDlg, (HMENU)ID_AUDIO_BROWSE_BUTTON, GetModuleHandle(NULL), NULL);
            
            // 创建测试按钮
            CreateWindowW(L"BUTTON", texts->testAudio,
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                20, 120, 100, 30,
                hDlg, (HMENU)ID_AUDIO_TEST_BUTTON, GetModuleHandle(NULL), NULL);
            
            // 创建确定按钮
            CreateWindowW(L"BUTTON", texts->ok,
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                250, 200, 70, 30,
                hDlg, (HMENU)ID_AUDIO_BTN_OK, GetModuleHandle(NULL), NULL);
            
            // 创建取消按钮
            CreateWindowW(L"BUTTON", texts->cancel,
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                330, 200, 70, 30,
                hDlg, (HMENU)ID_AUDIO_BTN_CANCEL, GetModuleHandle(NULL), NULL);
            
            // 设置当前配置状态
            CheckDlgButton(hDlg, ID_AUDIO_ENABLE_CHECKBOX, g_timerState.enableAudioAlert ? BST_CHECKED : BST_UNCHECKED);
            
            // 设置音频类型单选按钮
            if (g_timerState.useCustomAudio) {
                CheckDlgButton(hDlg, ID_AUDIO_CUSTOM_RADIO, BST_CHECKED);
                CheckDlgButton(hDlg, ID_AUDIO_CHINESE_RADIO, BST_UNCHECKED);
                CheckDlgButton(hDlg, ID_AUDIO_ENGLISH_RADIO, BST_UNCHECKED);
            } else if (g_timerState.useChineseAudio) {
                CheckDlgButton(hDlg, ID_AUDIO_CHINESE_RADIO, BST_CHECKED);
                CheckDlgButton(hDlg, ID_AUDIO_CUSTOM_RADIO, BST_UNCHECKED);
                CheckDlgButton(hDlg, ID_AUDIO_ENGLISH_RADIO, BST_UNCHECKED);
            } else {
                CheckDlgButton(hDlg, ID_AUDIO_ENGLISH_RADIO, BST_CHECKED);
                CheckDlgButton(hDlg, ID_AUDIO_CHINESE_RADIO, BST_UNCHECKED);
                CheckDlgButton(hDlg, ID_AUDIO_CUSTOM_RADIO, BST_UNCHECKED);
            }
            
            // 设置自定义音频路径
            SetWindowTextW(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_PATH_EDIT), g_timerState.customAudioPath);
            
            // 根据启用状态设置控件可用性
            BOOL enabled = g_timerState.enableAudioAlert;
            EnableWindow(GetDlgItem(hDlg, ID_AUDIO_CHINESE_RADIO), enabled);
            EnableWindow(GetDlgItem(hDlg, ID_AUDIO_ENGLISH_RADIO), enabled);
            EnableWindow(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_RADIO), enabled);
            EnableWindow(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_PATH_EDIT), enabled && g_timerState.useCustomAudio);
            EnableWindow(GetDlgItem(hDlg, ID_AUDIO_BROWSE_BUTTON), enabled && g_timerState.useCustomAudio);
            EnableWindow(GetDlgItem(hDlg, ID_AUDIO_TEST_BUTTON), enabled);

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
                g_hAudioPressedBtn = hClickedCtrl;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            // 如果点击的是空白区域或静态文本标签，则允许拖动
            else if (hClickedCtrl == NULL || hClickedCtrl == hDlg ||
                (GetWindowLong(hClickedCtrl, GWL_STYLE) & SS_LEFT)) {
                g_isAudioDlgDragging = TRUE;
                g_audioDlgDragStart = pt;
                GetWindowRect(hDlg, &g_audioDlgRectStart);
            }
            return 0;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_AUDIO_ENABLE_CHECKBOX: {
                    BOOL enabled = IsDlgButtonChecked(hDlg, ID_AUDIO_ENABLE_CHECKBOX) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hDlg, ID_AUDIO_CHINESE_RADIO), enabled);
                    EnableWindow(GetDlgItem(hDlg, ID_AUDIO_ENGLISH_RADIO), enabled);
                    EnableWindow(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_RADIO), enabled);
                    
                    BOOL customSelected = IsDlgButtonChecked(hDlg, ID_AUDIO_CUSTOM_RADIO) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_PATH_EDIT), enabled && customSelected);
                    EnableWindow(GetDlgItem(hDlg, ID_AUDIO_BROWSE_BUTTON), enabled && customSelected);
                    EnableWindow(GetDlgItem(hDlg, ID_AUDIO_TEST_BUTTON), enabled);
                    return 0;
                }
                
                case ID_AUDIO_CUSTOM_RADIO: {
                    BOOL enabled = IsDlgButtonChecked(hDlg, ID_AUDIO_ENABLE_CHECKBOX) == BST_CHECKED;
                    BOOL customSelected = IsDlgButtonChecked(hDlg, ID_AUDIO_CUSTOM_RADIO) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_PATH_EDIT), enabled && customSelected);
                    EnableWindow(GetDlgItem(hDlg, ID_AUDIO_BROWSE_BUTTON), enabled && customSelected);
                    return 0;
                }
                
                case ID_AUDIO_BROWSE_BUTTON: {
                    OPENFILENAMEW ofn = {0};
                    wchar_t szFile[MAX_PATH] = {0};
                    
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hDlg;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
                    ofn.lpstrFilter = L"音频文件\0*.wav;*.mp3;*.aac;*.wma;*.ogg\0WAV文件\0*.wav\0MP3文件\0*.mp3\0所有文件\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    
                    if (GetOpenFileNameW(&ofn)) {
                        SetWindowTextW(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_PATH_EDIT), szFile);
                    }
                    return 0;
                }
                
                case ID_AUDIO_TEST_BUTTON: {
                    BOOL useCustomAudio = IsDlgButtonChecked(hDlg, ID_AUDIO_CUSTOM_RADIO) == BST_CHECKED;
                    BOOL useChineseAudio = IsDlgButtonChecked(hDlg, ID_AUDIO_CHINESE_RADIO) == BST_CHECKED;
                    
                    BOOL testResult = FALSE;
                    if (useCustomAudio) {
                        wchar_t customPath[MAX_PATH];
                        GetWindowTextW(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_PATH_EDIT), customPath, MAX_PATH);
                        if (wcslen(customPath) > 0) {
                            testResult = TestCustomAudioPlayback(customPath);
                        } else {
                            const MenuTexts* texts = GetMenuTexts();
                            MessageBoxW(hDlg, L"请先选择自定义音频文件", texts->audioTitle, MB_OK | MB_ICONWARNING);
                            return 0;
                        }
                    } else {
                        testResult = TestAudioPlayback(useChineseAudio);
                    }
                    
                    if (!testResult) {
                        const MenuTexts* texts = GetMenuTexts();
                        MessageBoxW(hDlg, L"音频文件不存在或播放失败", texts->audioTitle, MB_OK | MB_ICONWARNING);
                    }
                    return 0;
                }
                
                case ID_AUDIO_BTN_OK: {
                    // 保存设置
                    g_timerState.enableAudioAlert = IsDlgButtonChecked(hDlg, ID_AUDIO_ENABLE_CHECKBOX) == BST_CHECKED;
                    g_timerState.useCustomAudio = IsDlgButtonChecked(hDlg, ID_AUDIO_CUSTOM_RADIO) == BST_CHECKED;
                    g_timerState.useChineseAudio = IsDlgButtonChecked(hDlg, ID_AUDIO_CHINESE_RADIO) == BST_CHECKED;
                    
                    // 获取自定义音频路径
                    GetWindowTextW(GetDlgItem(hDlg, ID_AUDIO_CUSTOM_PATH_EDIT), g_timerState.customAudioPath, MAX_PATH);
                    
                    // 保存到配置文件
                    SaveAudioConfig();
                    
                    DestroyWindow(hDlg);
                    return 0;
                }
                
                case ID_AUDIO_BTN_CANCEL:
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

        // 按钮悬停跟踪和拖拽
        case WM_MOUSEMOVE: {
            // 拖拽处理
            if (g_isAudioDlgDragging) {
                POINT currentPos;
                GetCursorPos(&currentPos);

                int deltaX = currentPos.x - (g_audioDlgRectStart.left + g_audioDlgDragStart.x);
                int deltaY = currentPos.y - (g_audioDlgRectStart.top + g_audioDlgDragStart.y);

                SetWindowPos(hDlg, NULL, g_audioDlgRectStart.left + deltaX,
                           g_audioDlgRectStart.top + deltaY,
                           0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }

            // 按钮悬停跟踪
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HWND hHoverCtrl = ChildWindowFromPoint(hDlg, pt);

            if (hHoverCtrl && (GetWindowLong(hHoverCtrl, GWL_STYLE) & BS_OWNERDRAW)) {
                if (g_hAudioHoverBtn != hHoverCtrl) {
                    g_hAudioHoverBtn = hHoverCtrl;
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            } else {
                if (g_hAudioHoverBtn != NULL) {
                    g_hAudioHoverBtn = NULL;
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_isAudioDlgDragging) {
                g_isAudioDlgDragging = FALSE;
                ReleaseCapture();
            }
            // 清除按钮按下状态并重绘
            if (g_hAudioPressedBtn != NULL) {
                g_hAudioPressedBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hAudioHoverBtn != NULL) {
                g_hAudioHoverBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        // 绘制按钮
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

            if (dis->CtlID == ID_AUDIO_BTN_OK || dis->CtlID == ID_AUDIO_BTN_CANCEL) {
                BOOL isHover = (g_hAudioHoverBtn == dis->hwndItem);
                BOOL isPressed = (g_hAudioPressedBtn == dis->hwndItem);
                BOOL isOK = (dis->CtlID == ID_AUDIO_BTN_OK);

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
                    case ID_AUDIO_BTN_OK: btnText = texts->ok; break;
                    case ID_AUDIO_BTN_CANCEL: btnText = texts->cancel; break;
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
            g_hAudioDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
    }
    
    return DefWindowProcW(hDlg, message, wParam, lParam);
}

void ShowAudioDialog(void) {
    if (g_hAudioDialog && IsWindow(g_hAudioDialog)) {
        SetForegroundWindow(g_hAudioDialog);
        return;
    }
    
    const MenuTexts* texts = GetMenuTexts();
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = AudioDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"AudioDialogClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    
    RECT parentRect; GetWindowRect(g_timerState.hMainWnd, &parentRect);
    int parentHeight = parentRect.bottom - parentRect.top;
    int dlgWidth = 420, dlgHeight = 270;
    int x = parentRect.right + 20;
    int y = parentRect.top + (parentHeight - dlgHeight) / 2;

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    if (x + dlgWidth > workArea.right) x = parentRect.left - dlgWidth - 20;
    if (x < workArea.left) x = workArea.left;
    if (y < workArea.top) y = workArea.top;
    if (x + dlgWidth > workArea.right) x = workArea.right - dlgWidth;
    if (y + dlgHeight > workArea.bottom) y = workArea.bottom - dlgHeight;

    g_hAudioDialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"AudioDialogClass",
        texts->audioTitle,
        WS_POPUP | WS_SYSMENU | WS_VISIBLE | WS_TABSTOP,
        x, y, dlgWidth, dlgHeight,
        g_timerState.hMainWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (g_hAudioDialog) {
        // 应用现代样式（圆角 + 阴影）
        ApplyModernDialogStyle(g_hAudioDialog);
        EnableWindow(g_timerState.hMainWnd, FALSE);
        SetForegroundWindow(g_hAudioDialog);
    }
}
