#ifndef TIMER_BUFFER_H
#define TIMER_BUFFER_H

// 双缓冲位图管理
void InitBackBuffer(void);
void CleanupBackBuffer(void);
void EnsureBackBufferSize(int width, int height);

// 数字预渲染缓存
void BuildDigitCache(int fontSize);
void CleanupDigitCache(void);

// 分层窗口缓冲
void CleanupLayeredBuffer(void);
void UpdateLayeredWindow_Render(void);

#endif // TIMER_BUFFER_H
