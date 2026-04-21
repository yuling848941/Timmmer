#ifndef TIMER_DIALOG_INTERNAL_H
#define TIMER_DIALOG_INTERNAL_H

// 内部共享头文件 - 仅供 timer_dialog_*.c 文件使用

#include "timer_dialogs.h"
#include "timer_core.h"
#include "timer_config.h"
#include "timer_fonts.h"
#include "timer_font_resources.h"
#include "timer_audio.h"
#include "timer_window.h"
#include "timer_ui.h"
#include <stdio.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <windowsx.h>
#include <dwmapi.h>

// 步进按钮ID定义
#define ID_BTN_HRS_MINUS    3001
#define ID_BTN_HRS_PLUS     3002
#define ID_BTN_MIN_MINUS    3003
#define ID_BTN_MIN_PLUS     3004
#define ID_BTN_SEC_MINUS    3005
#define ID_BTN_SEC_PLUS     3006

// 兼容性ID定义
#define ID_CHECK_HOURS      1001
#define ID_CHECK_MINUTES    1002
#define ID_CHECK_SECONDS    1003
#define ID_CHECK_MILLISECONDS 1004
#define ID_EDIT_HOURS       1005
#define ID_EDIT_MINUTES     1006
#define ID_EDIT_SECONDS     1007
#define ID_BUTTON_OK        1008
#define ID_BUTTON_CANCEL    1009

// 公共辅助函数声明
void DrawRoundRect(HDC hdc, RECT* rect, int radius, COLORREF fillColor, COLORREF borderColor, int borderWidth);
void ApplyModernDialogStyle(HWND hDlg);

#endif // TIMER_DIALOG_INTERNAL_H
