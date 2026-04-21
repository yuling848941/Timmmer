#ifndef TIMER_DIALOGS_H
#define TIMER_DIALOGS_H

#include "timer_types.h"

// 自定义消息
#define WM_SET_FOCUS_TO_MINUTES (WM_USER + 100)

// 格式设置对话框
void ShowFormatDialog(void);
void CreateFormatDialog(void);
LRESULT CALLBACK FormatDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 时间设置对话框
void ShowSetTimeDialog(void);
void CreateSetTimeDialog(void);
LRESULT CALLBACK SetTimeDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// 整合设置对话框
void ShowIntegratedDialog(void);
void CreateIntegratedDialog(void);
LRESULT CALLBACK IntegratedDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// 外观设置对话框
void ShowAppearanceDialog(void);
void CreateAppearanceDialog(void);
LRESULT CALLBACK AppearanceDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// 音频设置对话框
void ShowAudioDialog(void);
LRESULT CALLBACK AudioDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// 关于对话框
void ShowAboutDialog(void);
LRESULT CALLBACK AboutDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// 预设时间编辑对话框
void ShowPresetEditDialog(void);
void CreatePresetEditDialog(void);
LRESULT CALLBACK PresetEditDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// 对话框辅助功能
void UpdateCheckboxDependencies(HWND hDlg);
void UpdateTimeInputStates(HWND hwnd);

// 获取对话框句柄的函数
HWND GetFormatDialog(void);
HWND GetSetTimeDialog(void);
HWND GetAppearanceDialog(void);
HWND GetIntegratedDialog(void);

#endif // TIMER_DIALOGS_H