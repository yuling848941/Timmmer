#include "timer_dialog_internal.h"

static HWND g_hAboutHoverBtn = NULL;
static HWND g_hAboutPressedBtn = NULL;
static HFONT hAboutFont = NULL;

// ==================== 关于对话框 ====================

void ShowAboutDialog(void) {
    const MenuTexts* texts = GetMenuTexts();
    
    // 创建关于对话框
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"STATIC",
        texts->aboutTitle,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        0, 0, 500, 400,
        g_timerState.hMainWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!hDlg) return;
    
    // 居中显示
    RECT rect;
    GetWindowRect(hDlg, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    SetWindowPos(hDlg, HWND_TOP, x, y, width, height, SWP_SHOWWINDOW);
    
    // 设置窗口过程
    SetWindowLongPtrW(hDlg, GWLP_WNDPROC, (LONG_PTR)AboutDialogProc);
    
    // 禁用主窗口
    EnableWindow(g_timerState.hMainWnd, FALSE);
    
    // 创建多行文本控件（带滚动条）
    HWND hTextBox = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        NULL,
        WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        10, 10, 460, 320,
        hDlg, (HMENU)1001, GetModuleHandle(NULL), NULL
    );
    
    // 设置字体
    // 根据透明背景模式选择字体质量
    DWORD fontQuality = g_timerState.transparentBackground ? NONANTIALIASED_QUALITY : CLEARTYPE_QUALITY;
    if (hAboutFont) { DeleteObject(hAboutFont); }
    hAboutFont = CreateFontW(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        fontQuality, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei"
    );
    SendMessage(hTextBox, WM_SETFONT, (WPARAM)hAboutFont, TRUE);
    
    // 构建完整的关于信息文本
    wchar_t aboutText[2048];
    swprintf(aboutText, 2048,
        L"Timer 计时器应用程序\r\n"
        L"Version 1.2.0\r\n"
        L"\r\n"
        L"作者 / Author: 面包哥\r\n"
        L"年份 / Year: 2025年\r\n"
        L"\r\n"
        L"版权信息 / Copyright Information:\r\n"
        L"Copyright (C) 2025 面包哥\r\n"
        L"All rights reserved.\r\n"
        L"\r\n"
        L"许可证信息 / License Information:\r\n"
        L"本软件为专有软件，受版权法保护。\r\n"
        L"This software is proprietary and protected by copyright law.\r\n"
        L"\r\n"
        L"第三方组件 / Third-Party Components:\r\n"
        L"\r\n"
        L"字体许可证 / Font Licenses:\r\n"
        L"• MIT许可证字体:\r\n"
        L"  - ProFontWindows Essence\r\n"
        L"\r\n"
        L"• SIL Open Font License (OFL) 字体:\r\n"
        L"  - Jacquard 12 Essence\r\n"
        L"  - Jacquarda Bastarda 9 Essence\r\n"
        L"  - Pixelify Sans Essence\r\n"
        L"  - Rubik Burned Essence\r\n"
        L"  - Rubik Glitch Essence\r\n"
        L"  - Rubik Marker Hatch Essence\r\n"
        L"  - Rubik Puddles Essence\r\n"
        L"  - Wallpoet Essence\r\n"
        L"\r\n"
        L"• SIL许可证字体:\r\n"
        L"  - DaddyTimeMono Essence\r\n"
        L"  - DepartureMono Essence\r\n"
        L"  - Rec Mono Casual Essence\r\n"
        L"  - Terminess Nerd Font Essence\r\n"
        L"\r\n"
        L"系统库 / System Libraries:\r\n"
        L"使用Windows标准系统库，包括但不限于:\r\n"
        L"• user32.dll - 用户界面API\r\n"
        L"• gdi32.dll - 图形设备接口\r\n"
        L"• kernel32.dll - 核心系统功能\r\n"
        L"• shell32.dll - Shell功能\r\n"
        L"• comctl32.dll - 通用控件\r\n"
        L"• comdlg32.dll - 通用对话框\r\n"
        L"• uxtheme.dll - 主题支持\r\n"
        L"• winmm.dll - 多媒体API (包括MCI接口)\r\n"
        L"• dwmapi.dll - 桌面窗口管理器\r\n"
        L"• ole32.dll - OLE支持\r\n"
        L"• uuid.lib - UUID支持\r\n"
        L"\r\n"
        L"这些系统库均为Windows操作系统的标准组件，\r\n"
        L"可以安全地用于商业软件开发。\r\n"
        L"\r\n"
        L"免责声明 / Disclaimer:\r\n"
        L"本软件按\"现状\"提供，不提供任何明示或暗示的保证。\r\n"
        L"在任何情况下，作者或版权持有人均不对任何索赔、\r\n"
        L"损害或其他责任负责。\r\n"
        L"\r\n"
        L"感谢使用Timer计时器应用程序！\r\n"
        L"\r\n"
        L"项目主页 / GitHub Repo:\r\n"
        L"https://github.com/yuling848941/Timmmer"
    );
    
    // 设置文本内容
    SetWindowTextW(hTextBox, aboutText);
    
    // 恢复出厂设置按钮（左下角）
    CreateWindowW(L"BUTTON", texts->factoryReset,
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        20, 340, 120, 30, hDlg, (HMENU)ID_ABOUT_BTN_FACTORY_RESET, GetModuleHandle(NULL), NULL);
    
    // 确定按钮
    CreateWindowW(L"BUTTON", texts->ok,
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        160, 340, 80, 30, hDlg, (HMENU)ID_ABOUT_BTN_OK, GetModuleHandle(NULL), NULL);

    // GitHub 按钮
    CreateWindowW(L"BUTTON", L"GitHub",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        250, 340, 80, 30, hDlg, (HMENU)1002, GetModuleHandle(NULL), NULL);
}

LRESULT CALLBACK AboutDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_ABOUT_BTN_OK:
                    DestroyWindow(hDlg);
                    return 0;
                    
                case ID_ABOUT_BTN_FACTORY_RESET: {
                    int result = MessageBoxW(hDlg,
                        L"确定要恢复出厂设置吗？所有自定义设置将被重置。",
                        L"恢复出厂设置",
                        MB_YESNO | MB_ICONWARNING);
                    if (result == IDYES) {
                        PerformFactoryReset();
                        InvalidateRect(g_timerState.hMainWnd, NULL, TRUE);
                        DestroyWindow(hDlg);
                    }
                    return 0;
                }
                case 1002: { // GitHub 按钮
                    ShellExecuteW(NULL, L"open", L"https://github.com/yuling848941/Timmmer", NULL, NULL, SW_SHOWNORMAL);
                    return 0;
                }
            }
            break;

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
                if (g_hAboutHoverBtn != hHoverCtrl) {
                    g_hAboutHoverBtn = hHoverCtrl;
                    InvalidateRect(hDlg, NULL, FALSE);
                }
            } else {
                if (g_hAboutHoverBtn != NULL) {
                    g_hAboutHoverBtn = NULL;
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
                g_hAboutPressedBtn = hHoverCtrl;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            // 清除按钮按下状态并重绘
            if (g_hAboutPressedBtn != NULL) {
                g_hAboutPressedBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hAboutHoverBtn != NULL) {
                g_hAboutHoverBtn = NULL;
                InvalidateRect(hDlg, NULL, FALSE);
            }
            return 0;
        }

        // 绘制按钮
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

            if (dis->CtlID == ID_ABOUT_BTN_OK || dis->CtlID == ID_ABOUT_BTN_FACTORY_RESET || dis->CtlID == 1002) {
                BOOL isHover = (g_hAboutHoverBtn == dis->hwndItem);
                BOOL isPressed = (g_hAboutPressedBtn == dis->hwndItem);
                BOOL isOK = (dis->CtlID == ID_ABOUT_BTN_OK || dis->CtlID == 1002);

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
                    case ID_ABOUT_BTN_OK: btnText = texts->ok; break;
                    case ID_ABOUT_BTN_FACTORY_RESET: btnText = texts->factoryReset; break;
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
            if (hAboutFont) { DeleteObject(hAboutFont); hAboutFont = NULL; }
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
    }
    
    return DefWindowProcW(hDlg, message, wParam, lParam);
}
