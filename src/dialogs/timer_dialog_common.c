#include "timer_dialog_internal.h"

// ==========================================
// 现代 UI 样式辅助函数实现
// ==========================================

// 圆角矩形绘制辅助函数
void DrawRoundRect(HDC hdc, RECT* rect, int radius, COLORREF fillColor, COLORREF borderColor, int borderWidth) {
    HRGN hRgn = CreateRoundRectRgn(rect->left, rect->top, rect->right + 1, rect->bottom + 1, radius, radius);
    if (hRgn) {
        HBRUSH hBrush = CreateSolidBrush(fillColor);
        if (hBrush) { FillRgn(hdc, hRgn, hBrush); DeleteObject(hBrush); }
        HPEN hPen = CreatePen(PS_SOLID, borderWidth, borderColor);
        HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);
        RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom, radius, radius);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hPen);
        DeleteObject(hRgn);
    }
}

// 应用现代对话框样式（无边框、圆角、阴影）
void ApplyModernDialogStyle(HWND hDlg) {
    // 设置圆角
    DWORD cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hDlg, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    // 添加阴影
    MARGINS shadow = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hDlg, &shadow);
}

// No getters/setters here anymore. They are in their respective timer_dialog_*.c files.

