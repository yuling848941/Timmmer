#include "timer_dialog_internal.h"
#include "timer_render_utils.h"

// 关于对话框客户区布局（外框尺寸由 AdjustWindowRectEx 换算，避免标题栏/边框吃掉按钮行）
#define ABOUT_CW      480
#define ABOUT_CH      380
#define ABOUT_MARGIN  16
#define ABOUT_BTN_H   32
#define ABOUT_BTN_W   80

static HWND g_hAboutDialog = NULL;
static HWND g_hAboutHoverBtn = NULL;
static HFONT hAboutFont = NULL;
static WNDPROC g_origAboutBtnProc = NULL;

// 按钮子类：原生 BS_OWNERDRAW 按钮上的鼠标消息由子窗口自己接收，
// 父窗口的 WM_MOUSEMOVE 收不到，故在子窗口里跟踪悬停并重绘自身。
static LRESULT CALLBACK AboutBtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_MOUSEMOVE: {
            if (g_hAboutHoverBtn != h) {
                g_hAboutHoverBtn = h;
                InvalidateRect(h, NULL, FALSE);
            }
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, h, 0};
            TrackMouseEvent(&tme);
            break;
        }
        case WM_MOUSELEAVE:
            if (g_hAboutHoverBtn == h) {
                g_hAboutHoverBtn = NULL;
                InvalidateRect(h, NULL, FALSE);
            }
            break;
        case WM_DESTROY:
            if (g_hAboutHoverBtn == h) g_hAboutHoverBtn = NULL;
            break;
    }
    return CallWindowProcW(g_origAboutBtnProc, h, m, w, l);
}

static void SubclassAboutButton(HWND hBtn) {
    if (!g_origAboutBtnProc) {
        g_origAboutBtnProc = (WNDPROC)GetWindowLongPtrW(hBtn, GWLP_WNDPROC);
    }
    SetWindowLongPtrW(hBtn, GWLP_WNDPROC, (LONG_PTR)AboutBtnProc);
    SendMessageW(hBtn, WM_SETFONT, (WPARAM)hAboutFont, TRUE);
}

HWND GetAboutDialog(void) {
    return g_hAboutDialog;
}

// ==================== 关于对话框 ====================

void ShowAboutDialog(void) {
    // 单例：已打开则前置，避免多实例共享字体/悬停状态互相破坏
    if (g_hAboutDialog && IsWindow(g_hAboutDialog)) {
        SetForegroundWindow(g_hAboutDialog);
        return;
    }

    const MenuTexts* texts = GetMenuTexts();

    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST;

    // 按期望客户区反推外框尺寸，确保按钮行不会被标题栏/边框裁掉
    RECT rcClient = {0, 0, ABOUT_CW, ABOUT_CH};
    AdjustWindowRectEx(&rcClient, style, FALSE, exStyle);
    int outerW = rcClient.right - rcClient.left;
    int outerH = rcClient.bottom - rcClient.top;

    // 在父窗口所在显示器的工作区内居中
    HMONITOR mon = MonitorFromWindow(g_timerState.hMainWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - outerW) / 2;
    int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - outerH) / 2;

    HWND hDlg = CreateWindowExW(
        exStyle, L"STATIC", texts->aboutTitle, style,
        x, y, outerW, outerH,
        g_timerState.hMainWnd, NULL, GetModuleHandle(NULL), NULL
    );
    if (!hDlg) return;
    g_hAboutDialog = hDlg;

    SetWindowLongPtrW(hDlg, GWLP_WNDPROC, (LONG_PTR)AboutDialogProc);
    EnableWindow(g_timerState.hMainWnd, FALSE);

    // 字体：统一 UI 字体族 + ClearType（关于窗口始终不透明，无需点阵质量）
    if (hAboutFont) DeleteObject(hAboutFont);
    hAboutFont = CreateUiFont(14, FW_NORMAL, FALSE);

    int btnY = ABOUT_CH - ABOUT_MARGIN - ABOUT_BTN_H;

    // 多行只读文本（带滚动条），底部给按钮行留出空间
    HWND hTextBox = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", NULL,
        WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        ABOUT_MARGIN, ABOUT_MARGIN,
        ABOUT_CW - ABOUT_MARGIN * 2, btnY - ABOUT_MARGIN - 12,
        hDlg, (HMENU)1001, GetModuleHandle(NULL), NULL
    );
    SendMessage(hTextBox, WM_SETFONT, (WPARAM)hAboutFont, TRUE);

    wchar_t aboutText[2048];
    swprintf(aboutText, 2048,
        L"Timer 计时器应用程序\r\n"
        L"Version 1.3.0\r\n"
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
    SetWindowTextW(hTextBox, aboutText);

    // 按钮行：恢复出厂设置靠左，确定/GitHub 靠右对齐，消除右侧空白
    int rightEdge = ABOUT_CW - ABOUT_MARGIN;
    HWND hFactory = CreateWindowW(L"BUTTON", texts->factoryReset,
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        ABOUT_MARGIN, btnY, 120, ABOUT_BTN_H, hDlg, (HMENU)ID_ABOUT_BTN_FACTORY_RESET, GetModuleHandle(NULL), NULL);
    HWND hOk = CreateWindowW(L"BUTTON", texts->ok,
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        rightEdge - ABOUT_BTN_W * 2 - 10, btnY, ABOUT_BTN_W, ABOUT_BTN_H, hDlg, (HMENU)ID_ABOUT_BTN_OK, GetModuleHandle(NULL), NULL);
    HWND hGitHub = CreateWindowW(L"BUTTON", L"GitHub",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        rightEdge - ABOUT_BTN_W, btnY, ABOUT_BTN_W, ABOUT_BTN_H, hDlg, (HMENU)1002, GetModuleHandle(NULL), NULL);

    SubclassAboutButton(hFactory);
    SubclassAboutButton(hOk);
    SubclassAboutButton(hGitHub);

    ShowWindow(hDlg, SW_SHOW);
    SetForegroundWindow(hDlg);
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

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, UI_LIGHT_TEXT_PRIMARY);
            SetBkColor(hdcStatic, UI_LIGHT_BG_PRIMARY);
            static HBRUSH hDlgBgBrush = NULL;
            if (!hDlgBgBrush) hDlgBgBrush = CreateSolidBrush(UI_LIGHT_BG_PRIMARY);
            return (LRESULT)hDlgBgBrush;
        }

        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, UI_LIGHT_BG_PRIMARY);
            static HBRUSH hDlgBgBrush2 = NULL;
            if (!hDlgBgBrush2) hDlgBgBrush2 = CreateSolidBrush(UI_LIGHT_BG_PRIMARY);
            return (LRESULT)hDlgBgBrush2;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hDlg, &rect);
            HBRUSH hBrush = CreateSolidBrush(UI_LIGHT_BG_PRIMARY);
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
            return TRUE;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == ID_ABOUT_BTN_OK || dis->CtlID == ID_ABOUT_BTN_FACTORY_RESET || dis->CtlID == 1002) {
                BOOL isHover = (g_hAboutHoverBtn == dis->hwndItem);
                BOOL isPressed = (dis->itemState & ODS_SELECTED) != 0;
                BOOL isOK = (dis->CtlID == ID_ABOUT_BTN_OK || dis->CtlID == 1002);

                COLORREF fillColor = isOK ? UI_PRIMARY_COLOR : UI_LIGHT_BG_SECONDARY;
                if (isPressed) fillColor = isOK ? UI_PRIMARY_PRESSED : UI_LIGHT_BUTTON_PRESSED;
                else if (isHover) fillColor = isOK ? UI_PRIMARY_HOVER : UI_LIGHT_BUTTON_HOVER;

                RECT btnRect = dis->rcItem;
                if (isPressed) { btnRect.top += 1; btnRect.bottom += 1; }
                DrawRoundRect(dis->hDC, &btnRect, 6, fillColor, UI_LIGHT_BORDER, 1);

                const MenuTexts* texts = GetMenuTexts();
                const wchar_t* btnText = NULL;
                switch (dis->CtlID) {
                    case ID_ABOUT_BTN_OK: btnText = texts->ok; break;
                    case ID_ABOUT_BTN_FACTORY_RESET: btnText = texts->factoryReset; break;
                    case 1002: btnText = L"GitHub"; break;
                }
                if (btnText) {
                    HFONT oldFont = (HFONT)SelectObject(dis->hDC, hAboutFont);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, isOK ? RGB(255, 255, 255) : UI_LIGHT_TEXT_PRIMARY);
                    RECT textRect = btnRect;
                    DrawTextW(dis->hDC, btnText, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    SelectObject(dis->hDC, oldFont);
                }
                return TRUE;
            }
            return TRUE;
        }

        case WM_CLOSE:
            DestroyWindow(hDlg);
            return 0;

        case WM_DESTROY:
            if (hAboutFont) { DeleteObject(hAboutFont); hAboutFont = NULL; }
            g_hAboutHoverBtn = NULL;
            g_hAboutDialog = NULL;
            EnableWindow(g_timerState.hMainWnd, TRUE);
            SetForegroundWindow(g_timerState.hMainWnd);
            return 0;
    }

    return DefWindowProcW(hDlg, message, wParam, lParam);
}
