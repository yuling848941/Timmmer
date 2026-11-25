#ifndef TIMER_IPC_H
#define TIMER_IPC_H

#include <windows.h>

// 命名管道名（本机、当前用户）。MCP 服务与 JSON CLI 共用此管道。
#define TIMMER_PIPE_NAME L"\\\\.\\pipe\\timmmer"

// 管道线程把命令转交 UI 线程执行时使用的窗口消息。
// 用 WM_APP 段，避免与控件占用的 WM_USER 段冲突。
#define WM_IPC_COMMAND (WM_APP + 0x100)

// 启动 IPC 服务线程（须在创建主窗口、g_timerState.hMainWnd 就绪后调用）。
void Ipc_Start(HWND hMainWnd);

// 停止 IPC 服务线程并释放资源（在 WM_DESTROY 中调用）。
void Ipc_Shutdown(void);

// 在 UI 线程的 WindowProc 中处理 WM_IPC_COMMAND：
// lParam 指向 IpcCommand，执行命令、填充响应并 SetEvent。
void Ipc_HandleCommand(HWND hwnd, LPARAM lParam);

#endif // TIMER_IPC_H