#ifndef TIMER_UI_H
#define TIMER_UI_H

#include "timer_types.h"

// UI渲染功能
void UpdateDisplay(HDC hdc);
void TimerTick(HWND hwnd);

// 字体管理 - 现在使用timer_fonts.h中的函数
int CalculateFontSize(int width, int height, int charCount);
void CleanupFont(void);

// 语言和文本
const wchar_t* GetStartPauseText(void);
const MenuTexts* GetCurrentMenuTexts(void);

#endif // TIMER_UI_H