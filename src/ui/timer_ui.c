#include "timer_ui.h"
#include "timer_core.h"
#include "timer_config.h"
#include "timer_fonts.h"
#include "timer_window.h"
#include "timer_audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>  // for abs()

// 上一次渲染的时间字符串缓存
static char g_lastTimeStr[32] = {0};

// UI 渲染功能
void UpdateDisplay(HDC hdc) {
    char timeStr[32];
    FormatTimeCustom(g_timerState.seconds, timeStr);

    // 计算字体大小
    int charCount = strlen(timeStr);
    int fontSize = CalculateFontSize(g_timerState.windowWidth, g_timerState.windowHeight, charCount);

    // 使用字体缓存
    HFONT hFont = NULL;

    // 检查缓存是否有效：字体大小和字体名称都未变化时才使用缓存
    if (g_timerState.cachedFontValid &&
        fontSize == g_timerState.cachedFontHeight &&
        wcscmp(g_timerState.currentFontName, g_timerState.cachedFontName) == 0) {
        hFont = g_timerState.cachedHFont;
    }

    // 缓存无效或字体大小变化，需要重新获取
    if (!hFont || fontSize != g_timerState.cachedFontHeight) {
        // 获取新字体
        if (g_timerState.currentFontName[0] != L'\0') {
            hFont = GetTimerFont(g_timerState.currentFontName, fontSize, FALSE, FALSE);
        }

        if (!hFont) {
            hFont = GetMonospaceFont(fontSize);
        }

        if (!hFont) {
            hFont = GetDefaultTimerFont(fontSize);
        }

        // 更新缓存
        g_timerState.cachedHFont = hFont;
        g_timerState.cachedFontHeight = fontSize;
        wcscpy_s(g_timerState.cachedFontName, 64, g_timerState.currentFontName);
        g_timerState.cachedFontValid = (hFont != NULL);
    }

    // 选择字体
    HFONT oldFont = NULL;
    if (hFont) {
        oldFont = (HFONT)SelectObject(hdc, hFont);
    }

    // 设置字体颜色
    COLORREF textColor = g_timerState.fontColor;
    SetTextColor(hdc, textColor);

    // 设置背景模式和颜色
    if (g_timerState.transparentBackground) {
        SetBkMode(hdc, TRANSPARENT);
    } else if (g_timerState.isInOvertimeMode) {
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, g_timerState.backgroundColor);
    } else {
        SetBkMode(hdc, TRANSPARENT);
    }

    // 计算文本位置（居中）
    RECT rect = {0, 0, g_timerState.windowWidth, g_timerState.windowHeight};
    DrawTextA(hdc, timeStr, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 恢复原字体
    if (oldFont) {
        SelectObject(hdc, oldFont);
    }
}

/**
 * @brief 使用预渲染数字位图渲染时间（优化版）
 *
 * 通过预渲染的数字位图和脏矩形检测，
 * 只重绘变化的数字区域，大幅减少 GDI 调用。
 */
void UpdateDisplayOptimized(HDC hdc) {
    char timeStr[32];
    FormatTimeCustom(g_timerState.seconds, timeStr);

    // 如果时间字符串未变化，跳过渲染
    if (strcmp(timeStr, g_lastTimeStr) == 0) {
        return;
    }

    // 计算字体大小
    int charCount = strlen(timeStr);
    int fontSize = CalculateFontSize(g_timerState.windowWidth, g_timerState.windowHeight, charCount);

    // 确保数字缓存已构建
    BuildDigitCache(fontSize);

    // 计算文本总宽度以确定居中位置
    int totalWidth = 0;
    for (int i = 0; timeStr[i] != '\0'; i++) {
        if (timeStr[i] >= '0' && timeStr[i] <= '9') {
            totalWidth += 20; // 估算数字宽度
        } else if (timeStr[i] == ':') {
            totalWidth += 12;
        } else if (timeStr[i] == '.') {
            totalWidth += 8;
        }
    }

    // 起始 X 位置（居中）
    int startX = (g_timerState.windowWidth - totalWidth) / 2;
    int centerY = g_timerState.windowHeight / 2 + fontSize / 2;

    // 逐个绘制数字
    int xPos = startX;
    for (int i = 0; timeStr[i] != '\0'; i++) {
        HBITMAP hbmDigit = NULL;
        int digitWidth = 20;

        if (timeStr[i] >= '0' && timeStr[i] <= '9') {
            hbmDigit = g_timerState.hbmDigitCache[timeStr[i] - '0'];
        } else if (timeStr[i] == ':') {
            hbmDigit = g_timerState.hbmColonCache;
            digitWidth = 12;
        } else if (timeStr[i] == '.') {
            hbmDigit = g_timerState.hbmDotCache;
            digitWidth = 8;
        }

        if (hbmDigit) {
            // 创建内存 DC 并绘制位图
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, hbmDigit);

            // 获取位图尺寸
            BITMAP bmpInfo;
            GetObject(hbmDigit, sizeof(BITMAP), &bmpInfo);

            // 使用 AlphaBlend 绘制（需要 msimg32.lib）
            // 这里简化使用 BitBlt + 透明色键
            BLENDFUNCTION blend = {AC_SRC_OVER, 0, 128, AC_SRC_ALPHA};

            // 绘制到位图
            AlphaBlend(hdc, xPos, centerY - bmpInfo.bmHeight / 2,
                      bmpInfo.bmWidth, bmpInfo.bmHeight,
                      hdcMem, 0, 0, bmpInfo.bmWidth, bmpInfo.bmHeight, blend);

            SelectObject(hdcMem, oldBmp);
            DeleteDC(hdcMem);
        }

        xPos += digitWidth + 2; // 字符间距
    }

    // 更新缓存
    strcpy(g_lastTimeStr, timeStr);
}

// 字体管理 - 现在使用新的懒加载字体系统
// CreateCustomFont 函数已移除，使用 timer_fonts.h 中的函数

/**
 * @brief 计算适合窗口大小的字体大小
 *
 * 使用缓存机制避免重复计算。首次测量参考文本尺寸，
 * 之后通过线性公式计算目标字体大小。
 *
 * @param width 窗口宽度 (像素)
 * @param height 窗口高度 (像素)
 * @param charCount 要显示的字符数
 * @return 计算出的字体大小 (像素)
 *
 * @note 缓存命中条件：窗口尺寸和字符数均在容差范围内未变化
 * @note 字体大小限制范围：[16, 800]
 */
int CalculateFontSize(int width, int height, int charCount) {
    // 缓存命中：参数在容差范围内未变化则直接返回
    if (abs(width - g_timerState.cachedCalcWidth) <= CACHE_TOLERANCE
     && abs(height - g_timerState.cachedCalcHeight) <= CACHE_TOLERANCE
     && charCount == g_timerState.cachedCharCount
     && g_timerState.cachedFontSize > 0) {
        return g_timerState.cachedFontSize;
    }

    // 计算可用空间（使用命名常量替代魔法数字）
    int availableWidth = width - UI_TEXT_MARGIN;
    int availableHeight = height - UI_TEXT_MARGIN;

    if (availableWidth <= 0) availableWidth = width - UI_SMALL_MARGIN;
    if (availableHeight <= 0) availableHeight = height - UI_SMALL_MARGIN;
    if (availableWidth <= 0) availableWidth = width - UI_NO_MARGIN;
    if (availableHeight <= 0) availableHeight = height - UI_NO_MARGIN;

    // 参考测量：仅在字符数变化时执行一次
    // 原理：TrueType 字体的文本尺寸与字号成严格线性关系
    // 在参考字号 (100pt) 下测量一次，之后用公式直接计算
    if (charCount != g_timerState.refMeasuredCharCount) {
        HDC hdc = GetDC(NULL);
        if (hdc) {
            char testStr[32];
            if (charCount <= 5) {
                strcpy(testStr, "05:00");
            } else if (charCount <= 8) {
                strcpy(testStr, "05:00:00");
            } else {
                strcpy(testStr, "05:00:00.00");
            }

            HFONT refFont = GetMonospaceFont(100);
            if (!refFont) {
                refFont = GetDefaultTimerFont(100);
            }

            if (refFont) {
                HFONT oldFont = (HFONT)SelectObject(hdc, refFont);
                SIZE textSize;
                GetTextExtentPointA(hdc, testStr, strlen(testStr), &textSize);
                SelectObject(hdc, oldFont);

                g_timerState.refTextWidth = textSize.cx;
                g_timerState.refTextHeight = textSize.cy;
                g_timerState.refMeasuredCharCount = charCount;
            }

            ReleaseDC(NULL, hdc);
        }
    }

    int fontSize = 16; // 默认值

    if (g_timerState.refTextWidth > 0 && g_timerState.refTextHeight > 0) {
        // 直接公式计算：fontSize = refSize * availableSpace / refMeasuredSpace
        int sizeByWidth = 100 * availableWidth / g_timerState.refTextWidth;
        int sizeByHeight = 100 * availableHeight / g_timerState.refTextHeight;
        fontSize = sizeByWidth < sizeByHeight ? sizeByWidth : sizeByHeight;
    } else {
        // 参考测量失败的降级：用粗略估算
        int maxWidthSize = (availableWidth * 10) / (charCount * 6);
        int maxHeightSize = (int)(availableHeight * 0.9);
        fontSize = maxWidthSize < maxHeightSize ? maxWidthSize : maxHeightSize;
        fontSize = (int)(fontSize * 1.1);
    }

    // 限制范围
    if (fontSize < 16) fontSize = 16;
    if (fontSize > 800) fontSize = 800;

    // 保存缓存
    g_timerState.cachedCalcWidth = width;
    g_timerState.cachedCalcHeight = height;
    g_timerState.cachedCharCount = charCount;
    g_timerState.cachedFontSize = fontSize;

    return fontSize;
}

// 调整大小相关功能
int GetResizeEdge(HWND hwnd, int x, int y) {
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    int edge = 0;

    if (x <= RESIZE_BORDER) edge |= RESIZE_LEFT;
    if (x >= clientRect.right - RESIZE_BORDER) edge |= RESIZE_RIGHT;
    if (y <= RESIZE_BORDER) edge |= RESIZE_TOP;
    if (y >= clientRect.bottom - RESIZE_BORDER) edge |= RESIZE_BOTTOM;

    return edge;
}

HCURSOR GetResizeCursor(int edge) {
    switch (edge) {
        case RESIZE_LEFT:
        case RESIZE_RIGHT:
            return LoadCursor(NULL, IDC_SIZEWE);
        case RESIZE_TOP:
        case RESIZE_BOTTOM:
            return LoadCursor(NULL, IDC_SIZENS);
        case RESIZE_LEFT | RESIZE_TOP:
        case RESIZE_RIGHT | RESIZE_BOTTOM:
            return LoadCursor(NULL, IDC_SIZENWSE);
        case RESIZE_RIGHT | RESIZE_TOP:
        case RESIZE_LEFT | RESIZE_BOTTOM:
            return LoadCursor(NULL, IDC_SIZENESW);
        default:
            return LoadCursor(NULL, IDC_ARROW);
    }
}

// 语言和文本相关功能
const wchar_t* GetStartPauseText(void) {
    const MenuTexts* texts = GetMenuTexts();

    if (g_timerState.isRunning) {
        return texts->pause;
    } else {
        return texts->start;
    }
}

/**
 * @brief 计时器心跳处理函数
 *
 * 处理倒计时、超时判断、超时正计时等逻辑。
 * 根据 showMilliseconds 标志决定更新频率。
 *
 * @param hwnd 主窗口句柄
 *
 * @note 该函数在 WM_TIMER 消息中被调用
 * @note 超时正计时模式会改变字体和背景颜色
 */
void TimerTick(HWND hwnd) {
    if (!g_timerState.isRunning) {
        return;
    }

    // 只有在非超时正计时模式下才执行倒计时逻辑
    if (g_timerState.seconds >= 0 && !g_timerState.isInOvertimeMode && !g_timerState.isTimeUp) {

        if (g_timerState.showMilliseconds) {
            // 毫秒模式：基于实际经过时间计算
            DWORD currentTime = (DWORD)GetTickCount64();
            DWORD elapsed = currentTime - g_timerState.startTime;
            int elapsedSeconds = elapsed / 1000;

            if (elapsedSeconds > 0) {
                g_timerState.seconds -= elapsedSeconds;
                g_timerState.startTime = currentTime - (elapsed % 1000); // 保持毫秒精度

                if (g_timerState.seconds < 0) {
                    g_timerState.seconds = 0;
                }
            }
        } else {
            // 普通模式：每秒减 1
            if (g_timerState.seconds > 0) {
                g_timerState.seconds--;
                // 如果减到 0，立即设置时间到达标志
                if (g_timerState.seconds == 0) {
                    g_timerState.isTimeUp = TRUE;
                }
            }
        }

        // 强制重绘窗口
        InvalidateRect(hwnd, NULL, FALSE);
    }

    // 如果时间到了，处理结束逻辑
    if (g_timerState.isTimeUp && !g_timerState.isInOvertimeMode) {
        // 如果启用了超时正计时功能
        if (g_timerState.enableOvertimeCount) {
            // 保存原始颜色（在进入超时模式之前保存）
            g_timerState.originalFontColor = g_timerState.fontColor;
            g_timerState.originalBackgroundColor = g_timerState.backgroundColor;

            // 进入超时正计时模式
            g_timerState.isInOvertimeMode = TRUE;
            g_timerState.isTimeUp = FALSE;
            g_timerState.isRunning = TRUE;
            g_timerState.seconds = 0; // 从 0 开始正计时

            // 切换到红底白字
            g_timerState.fontColor = RGB(255, 255, 255); // 白字
            g_timerState.backgroundColor = RGB(255, 0, 0); // 红底

            // 重新启动计时器继续正计时
            g_timerState.startTime = (DWORD)GetTickCount64();
            int interval = g_timerState.showMilliseconds ? 50 : 1000;
            g_timerState.timerId = SetTimer(hwnd, 1, interval, NULL);

            // 播放超时提示音（语音或系统提示音）
            if (g_timerState.enableAudioAlert && !PlayTimeoutAlert()) {
                // 如果音频启用但播放失败，播放系统提示音作为备用
                MessageBeep(MB_ICONEXCLAMATION);
            }

            // 强制重绘窗口
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            // 正常超时处理（不启用超时正计时）
            g_timerState.isRunning = FALSE;

            // 停止计时器
            if (g_timerState.timerId) {
                KillTimer(hwnd, g_timerState.timerId);
                g_timerState.timerId = 0;
            }

            // 强制重绘窗口
            InvalidateRect(hwnd, NULL, TRUE);

            // 播放超时提示音（语音或系统提示音）
            if (g_timerState.enableAudioAlert && !PlayTimeoutAlert()) {
                // 如果音频启用但播放失败，播放系统提示音作为备用
                MessageBeep(MB_ICONEXCLAMATION);
            }
        }
    }

    // 如果在超时正计时模式，继续正计时
    if (g_timerState.isInOvertimeMode && g_timerState.isRunning) {

        // 检查是否刚进入超时正计时模式（seconds=0 且 startTime 刚设置）
        DWORD currentTime = (DWORD)GetTickCount64();
        DWORD elapsed = currentTime - g_timerState.startTime;

        if (g_timerState.showMilliseconds) {
            // 毫秒模式：基于实际经过时间计算正计时
            int elapsedSeconds = elapsed / 1000;

            if (elapsedSeconds > 0) {
                g_timerState.seconds += elapsedSeconds;
                g_timerState.startTime = currentTime - (elapsed % 1000); // 保持毫秒精度
            }
        } else {
            // 普通模式：只有经过至少 1 秒后才开始递增
            if (elapsed >= 1000) {
                g_timerState.seconds++;
                g_timerState.startTime = currentTime; // 重置开始时间
            }
        }

        // 强制重绘窗口
        InvalidateRect(hwnd, NULL, FALSE);
    }
}
