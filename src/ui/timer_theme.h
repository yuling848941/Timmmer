#ifndef TIMER_THEME_H
#define TIMER_THEME_H

#include "timer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 当前主题（首次调用前自动初始化为暗色）。返回的指针在主题切换后仍有效，但内容会变。
const UiTheme* Ui_GetTheme(void);

// 当前是否为暗色主题
BOOL Ui_IsDark(void);

// 切换暗色 / 亮色：立即生效，并向本线程所有顶层窗口发送 WM_UI_THEME_CHANGED。
// 调用方负责把选择写入配置（SaveAppearanceConfig）。
void Ui_SetDark(BOOL dark);

// 计时器的字色变化后调用：重新推导强调色（菜单与面板的强调色跟随计时器字色），
// 并通知所有界面重绘。
void Ui_RefreshAccent(void);

#ifdef __cplusplus
}
#endif

#endif // TIMER_THEME_H
