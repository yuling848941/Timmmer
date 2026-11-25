/*
 * Timer Application - Named-pipe JSON IPC
 *
 * 为 AI agent / 脚本提供可查询、可闭环的控制接口。
 * 管道线程只负责收发 JSON；所有对 g_timerState 的读写都通过
 * WM_IPC_COMMAND 转交 UI 线程执行（计时状态与 SetTimer/KillTimer
 * 都绑定主窗口线程，不能跨线程直接改）。
 *
 * 协议：每行一个 JSON 请求，回一行 JSON 响应（含执行后的状态）。
 *   {"cmd":"status"}
 *   {"cmd":"start"} / {"cmd":"pause"} / {"cmd":"toggle"} / {"cmd":"reset"}
 *   {"cmd":"set","seconds":1500}
 *   {"cmd":"presets"}
 *   {"cmd":"preset","index":0}
 */

#include "timer_ipc.h"
#include "timer_core.h"
#include "timer_config.h"
#include "timer_types.h"
#include "timer_fonts.h"
#include "timer_theme.h"
#include "timer_buffer.h"
#include "timer_audio.h"
#include "timer_window_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define IPC_BUF_SIZE   4096
#define IPC_RESP_SIZE  8192
#define IPC_MAX_SECONDS 359999   /* 99:59:59 */
#define IPC_WAIT_MS    10000   /* 音频试听是同步播放，留足时间 */

#define JB(x) ((x) ? "true" : "false")

// 管道线程 → UI 线程的命令载体（堆分配，管道线程负责释放）
typedef struct IpcCommand {
    char   cmd[32];
    int    seconds;
    int    index;
    BOOL   hasSeconds;
    BOOL   hasIndex;

    // 外观
    char   fontColor[16];
    char   bgColor[16];
    BOOL   hasFontColor;
    BOOL   hasBgColor;

    // 开关类
    BOOL   enabled;
    BOOL   hasEnabled;
    BOOL   clearFlag;
    BOOL   hasClear;
    BOOL   customFlag;
    BOOL   hasCustom;

    // 音频
    char   language[8];
    BOOL   hasLanguage;
    char   path[1024];
    BOOL   hasPath;

    // 窗口
    char   action[16];
    BOOL   hasAction;

    HANDLE hEvent;                 // UI 线程执行完毕后置位
    char   response[IPC_RESP_SIZE];
} IpcCommand;

static HWND          g_ipcHwnd  = NULL;
static HANDLE        g_ipcThread = NULL;
static volatile LONG g_ipcStop  = 0;

// ===========================================
// 极简扁平 JSON 解析（请求只含 string / int 顶层字段）
// ===========================================

static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

// 跳过一个 JSON 字符串（*p 必须是 '"'），返回闭引号之后的位置；失败返回 NULL。
static const char* skip_string(const char* p) {
    if (*p != '"') return NULL;
    p++;
    while (*p) {
        if (*p == '\\') {
            p++;
            if (!*p) return NULL;
            if (*p == 'u') {           // \uXXXX
                for (int i = 0; i < 4; i++) {
                    p++;
                    if (!*p) return NULL;
                }
            }
            p++;
        } else if (*p == '"') {
            return p + 1;
        } else {
            p++;
        }
    }
    return NULL;
}

// 解析字符串到 bounded 缓冲区，做基本转义。返回闭引号之后的位置。
static const char* parse_string(const char* p, char* out, size_t cap) {
    if (*p != '"') return NULL;
    p++;
    size_t n = 0;
    while (*p && *p != '"') {
        char c = *p;
        if (c == '\\') {
            p++;
            switch (*p) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u':  c = '?';  // 命令均为 ASCII，不会出现；占位以保证安全
                           for (int i = 0; i < 4 && p[1]; i++) p++;
                           break;
                default:   return NULL;
            }
        }
        if (n + 1 < cap) out[n++] = c;
        p++;
    }
    if (*p != '"') return NULL;
    out[n] = '\0';
    return p + 1;
}

// 解析整数（可带负号），返回结束位置。
static const char* parse_int(const char* p, int* out) {
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') { p++; }
    if (*p < '0' || *p > '9') return NULL;
    long v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        if (v > 1000000000L) return NULL;   // 防溢出
        p++;
    }
    *out = (int)(sign * v);
    return p;
}

// 解析布尔值（true/false/1/0）
static const char* parse_bool(const char* p, BOOL* out) {
    if (strncmp(p, "true", 4) == 0)  { *out = TRUE;  return p + 4; }
    if (strncmp(p, "false", 5) == 0) { *out = FALSE; return p + 5; }
    if (*p == '1') { *out = TRUE;  return p + 1; }
    if (*p == '0') { *out = FALSE; return p + 1; }
    return NULL;
}

// 跳过一个未知值（字符串 / 嵌套结构 / 标量）。
static const char* skip_value(const char* p) {
    p = skip_ws(p);
    if (*p == '"') return skip_string(p);
    if (*p == '{' || *p == '[') {
        char open = *p;
        char close = (open == '{') ? '}' : ']';
        int depth = 0;
        while (*p) {
            if (*p == '"') {
                p = skip_string(p);
                if (!p) return NULL;
                continue;
            }
            if (*p == open) depth++;
            else if (*p == close) {
                depth--;
                if (depth == 0) return p + 1;
            }
            p++;
        }
        return NULL;
    }
    // 标量：读到 , } ] 或空白为止
    while (*p && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
    return p;
}

static BOOL ParseRequest(const char* json, IpcCommand* out) {
    const char* p = skip_ws(json);
    if (*p != '{') return FALSE;
    p++;
    for (;;) {
        p = skip_ws(p);
        if (*p == '}') break;
        char key[32];
        p = parse_string(p, key, sizeof(key));
        if (!p) return FALSE;
        p = skip_ws(p);
        if (*p != ':') return FALSE;
        p = skip_ws(p + 1);

        if (strcmp(key, "cmd") == 0) {
            p = parse_string(p, out->cmd, sizeof(out->cmd));
            if (!p) return FALSE;
        } else if (strcmp(key, "seconds") == 0) {
            p = parse_int(p, &out->seconds);
            if (!p) return FALSE;
            out->hasSeconds = TRUE;
        } else if (strcmp(key, "index") == 0) {
            p = parse_int(p, &out->index);
            if (!p) return FALSE;
            out->hasIndex = TRUE;
        } else if (strcmp(key, "font_color") == 0) {
            p = parse_string(p, out->fontColor, sizeof(out->fontColor));
            if (!p) return FALSE;
            out->hasFontColor = TRUE;
        } else if (strcmp(key, "background_color") == 0) {
            p = parse_string(p, out->bgColor, sizeof(out->bgColor));
            if (!p) return FALSE;
            out->hasBgColor = TRUE;
        } else if (strcmp(key, "enabled") == 0) {
            p = parse_bool(p, &out->enabled);
            if (!p) return FALSE;
            out->hasEnabled = TRUE;
        } else if (strcmp(key, "clear") == 0) {
            p = parse_bool(p, &out->clearFlag);
            if (!p) return FALSE;
            out->hasClear = TRUE;
        } else if (strcmp(key, "custom") == 0) {
            p = parse_bool(p, &out->customFlag);
            if (!p) return FALSE;
            out->hasCustom = TRUE;
        } else if (strcmp(key, "language") == 0) {
            p = parse_string(p, out->language, sizeof(out->language));
            if (!p) return FALSE;
            out->hasLanguage = TRUE;
        } else if (strcmp(key, "path") == 0) {
            p = parse_string(p, out->path, sizeof(out->path));
            if (!p) return FALSE;
            out->hasPath = TRUE;
        } else if (strcmp(key, "action") == 0) {
            p = parse_string(p, out->action, sizeof(out->action));
            if (!p) return FALSE;
            out->hasAction = TRUE;
        } else {
            p = skip_value(p);
            if (!p) return FALSE;
        }

        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') break;
        return FALSE;
    }
    return out->cmd[0] != '\0';
}

// ===========================================
// 外观控制辅助
// ===========================================

static BOOL IsAppearanceCmd(const char* cmd) {
    return strcmp(cmd, "appearance") == 0 ||
           strcmp(cmd, "random_appearance") == 0 ||
           strcmp(cmd, "set_font") == 0 ||
           strcmp(cmd, "random_font") == 0 ||
           strcmp(cmd, "fonts") == 0 ||
           strcmp(cmd, "set_colors") == 0 ||
           strcmp(cmd, "set_transparent") == 0 ||
           strcmp(cmd, "reset_appearance") == 0;
}

static BOOL IsAudioCmd(const char* cmd) {
    return strcmp(cmd, "audio") == 0 ||
           strcmp(cmd, "set_audio") == 0 ||
           strcmp(cmd, "set_audio_language") == 0 ||
           strcmp(cmd, "set_custom_audio") == 0 ||
           strcmp(cmd, "test_audio") == 0;
}

// UTF-8 → 宽字符（用于自定义音频路径）
static void Utf8ToWide(const char* in, wchar_t* out, size_t cap) {
    out[0] = L'\0';
    if (!in || cap == 0) return;
    int n = MultiByteToWideChar(CP_UTF8, 0, in, -1, out, (int)cap);
    if (n <= 0) out[0] = L'\0';
}

static int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// 解析 "#RRGGBB" 或 "RRGGBB"
static BOOL ParseHexColor(const char* s, COLORREF* out) {
    if (!s) return FALSE;
    if (*s == '#') s++;
    for (int i = 0; i < 6; i++) {
        if (HexVal(s[i]) < 0) return FALSE;
    }
    if (s[6] != '\0') return FALSE;
    int r = HexVal(s[0]) * 16 + HexVal(s[1]);
    int g = HexVal(s[2]) * 16 + HexVal(s[3]);
    int b = HexVal(s[4]) * 16 + HexVal(s[5]);
    *out = RGB(r, g, b);
    return TRUE;
}

// 宽字符 → UTF-8（用于字体名回传）
static void WideToUtf8(const wchar_t* w, char* out, size_t cap) {
    out[0] = '\0';
    if (!w || cap == 0) return;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL);
    if (n <= 0) out[0] = '\0';
}

// 追加一个带转义的 JSON 字符串
static void AppendJsonString(char* buf, int* n, size_t cap, const char* s) {
    if (*n < 0 || (size_t)*n >= cap) return;
    int w = snprintf(buf + *n, cap - *n, "\"");
    if (w < 0) return;
    *n += w;
    for (; s && *s && (size_t)*n + 7 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            buf[(*n)++] = '\\';
            buf[(*n)++] = (char)c;
        } else if (c < 0x20) {
            *n += snprintf(buf + *n, cap - *n, "\\u%04x", c);
        } else {
            buf[(*n)++] = (char)c;
        }
    }
    if ((size_t)*n < cap) buf[(*n)++] = '"';
    if ((size_t)*n < cap) buf[*n] = '\0';
}

// 应用外观并立即刷新主窗口（与外观对话框的 ApplyPreview 一致）
static void RedrawMainWindow(void) {
    HWND h = g_timerState.hMainWnd;
    if (!h) return;
    if (g_timerState.transparentBackground) {
        UpdateLayeredWindow_Render();
    } else {
        SetLayeredWindowAttributes(h, 0, g_timerState.windowOpacity, LWA_ALPHA);
        InvalidateRect(h, NULL, TRUE);
        UpdateWindow(h);
    }
    Ui_RefreshAccent();
}

static void ApplyFontByName(const wchar_t* name) {
    wcsncpy_s(g_timerState.currentFontName,
              sizeof(g_timerState.currentFontName) / sizeof(wchar_t), name, _TRUNCATE);
    ClearFontCache();
    g_timerState.cachedFontValid = FALSE;
    g_timerState.cachedHFont = NULL;
    CleanupDigitCache();          // 数字位图缓存按字号缓存，换字体必须失效
    SaveAppearanceConfig();
}

// ===========================================
// 响应构建（在 UI 线程读取执行后的状态）
// ===========================================

static void BuildResponse(IpcCommand* c, BOOL ok, const char* error) {
    BOOL running  = g_timerState.isRunning;
    BOOL timeUp   = g_timerState.isTimeUp;
    BOOL overtime = g_timerState.isInOvertimeMode;
    int  secs     = g_timerState.seconds;
    int  remaining = overtime ? 0 : secs;
    int  overtimeSecs = overtime ? secs : 0;
    const char* mode = overtime ? "overtime"
                     : timeUp   ? "finished"
                     : running  ? "running"
                                : "paused";

    char display[32];
    FormatTimeCustom(secs, display);

    int n = snprintf(c->response, IPC_RESP_SIZE,
        "{\"ok\":%s,\"cmd\":\"%s\",\"running\":%s,\"time_up\":%s,\"overtime\":%s,"
        "\"remaining_seconds\":%d,\"overtime_seconds\":%d,\"seconds\":%d,"
        "\"display\":\"%s\",\"mode\":\"%s\","
        "\"visible\":%s,\"overtime_enabled\":%s",
        JB(ok), c->cmd, JB(running), JB(timeUp), JB(overtime),
        remaining, overtimeSecs, secs, display, mode,
        JB(IsWindowVisible(g_timerState.hMainWnd)),
        JB(g_timerState.enableOvertimeCount));

    // 预设列表（仅 presets 命令附带）
    if (n > 0 && n < IPC_RESP_SIZE && strcmp(c->cmd, "presets") == 0) {
        int count = GetPresetCount();
        n += snprintf(c->response + n, IPC_RESP_SIZE - n, ",\"preset_count\":%d,\"presets\":[", count);
        for (int i = 0; i < count && n < IPC_RESP_SIZE; i++) {
            n += snprintf(c->response + n, IPC_RESP_SIZE - n, "%s%d",
                          i ? "," : "", GetPresetTime(i));
        }
        if (n < IPC_RESP_SIZE) n += snprintf(c->response + n, IPC_RESP_SIZE - n, "]");
    }

    // 外观信息（仅外观相关命令附带；fonts 命令额外返回字体列表）
    if (n > 0 && n < IPC_RESP_SIZE && IsAppearanceCmd(c->cmd)) {
        wchar_t names[32][256];
        int fontCount = GetMixedFontList(names, 32);
        COLORREF fc = g_timerState.fontColor;
        COLORREF bc = g_timerState.backgroundColor;
        n += snprintf(c->response + n, IPC_RESP_SIZE - n,
            ",\"appearance\":{\"font_color\":\"#%02X%02X%02X\","
            "\"background_color\":\"#%02X%02X%02X\",\"opacity\":%d,"
            "\"transparent_background\":%s,\"font\":",
            GetRValue(fc), GetGValue(fc), GetBValue(fc),
            GetRValue(bc), GetGValue(bc), GetBValue(bc),
            g_timerState.windowOpacity, JB(g_timerState.transparentBackground));

        char u8[512];
        WideToUtf8(g_timerState.currentFontName, u8, sizeof(u8));
        AppendJsonString(c->response, &n, IPC_RESP_SIZE, u8);

        n += snprintf(c->response + n, IPC_RESP_SIZE - n, ",\"font_count\":%d", fontCount);
        if (strcmp(c->cmd, "set_font") == 0 || strcmp(c->cmd, "random_font") == 0) {
            n += snprintf(c->response + n, IPC_RESP_SIZE - n, ",\"font_index\":%d", c->index);
        }
        if (strcmp(c->cmd, "fonts") == 0) {
            n += snprintf(c->response + n, IPC_RESP_SIZE - n, ",\"fonts\":[");
            for (int i = 0; i < fontCount && n < IPC_RESP_SIZE; i++) {
                if (i) n += snprintf(c->response + n, IPC_RESP_SIZE - n, ",");
                WideToUtf8(names[i], u8, sizeof(u8));
                AppendJsonString(c->response, &n, IPC_RESP_SIZE, u8);
            }
            n += snprintf(c->response + n, IPC_RESP_SIZE - n, "]");
        }
        n += snprintf(c->response + n, IPC_RESP_SIZE - n, "}");
    }

    // 音频信息（仅音频相关命令附带）
    if (n > 0 && n < IPC_RESP_SIZE && IsAudioCmd(c->cmd)) {
        n += snprintf(c->response + n, IPC_RESP_SIZE - n,
            ",\"audio\":{\"enabled\":%s,\"language\":\"%s\",\"custom\":%s,\"custom_path\":",
            JB(g_timerState.enableAudioAlert),
            g_timerState.useChineseAudio ? "zh" : "en",
            JB(g_timerState.useCustomAudio));
        char p8[2048];
        WideToUtf8(g_timerState.customAudioPath, p8, sizeof(p8));
        AppendJsonString(c->response, &n, IPC_RESP_SIZE, p8);
        n += snprintf(c->response + n, IPC_RESP_SIZE - n, "}");
    }

    // 预设循环结果
    if (n > 0 && n < IPC_RESP_SIZE && strcmp(c->cmd, "next_preset") == 0) {
        n += snprintf(c->response + n, IPC_RESP_SIZE - n,
                      ",\"preset_index\":%d,\"preset_count\":%d",
                      g_timerState.currentPresetIndex, GetPresetCount());
    }

    // 错误信息
    if (n > 0 && n < IPC_RESP_SIZE) {
        if (ok) {
            snprintf(c->response + n, IPC_RESP_SIZE - n, ",\"error\":null}");
        } else {
            snprintf(c->response + n, IPC_RESP_SIZE - n, ",\"error\":\"%s\"}",
                     error ? error : "unknown error");
        }
    }
}

// ===========================================
// UI 线程：执行命令
// ===========================================

void Ipc_HandleCommand(HWND hwnd, LPARAM lParam) {
    IpcCommand* c = (IpcCommand*)lParam;
    if (!c) return;

    BOOL ok = TRUE;
    const char* error = NULL;

    if (strcmp(c->cmd, "status") == 0 || strcmp(c->cmd, "presets") == 0) {
        // 只读，无副作用
    } else if (strcmp(c->cmd, "start") == 0) {
        StartTimer();
    } else if (strcmp(c->cmd, "pause") == 0) {
        StopTimer();
    } else if (strcmp(c->cmd, "toggle") == 0) {
        if (g_timerState.isRunning) StopTimer(); else StartTimer();
    } else if (strcmp(c->cmd, "reset") == 0) {
        ResetTimer();
    } else if (strcmp(c->cmd, "set") == 0) {
        if (!c->hasSeconds) {
            ok = FALSE; error = "set requires 'seconds'";
        } else if (c->seconds < 0 || c->seconds > IPC_MAX_SECONDS) {
            ok = FALSE; error = "seconds out of range (0..359999)";
        } else {
            SetTimerSeconds(c->seconds);
        }
    } else if (strcmp(c->cmd, "preset") == 0) {
        int count = GetPresetCount();
        if (!c->hasIndex) {
            ok = FALSE; error = "preset requires 'index'";
        } else if (c->index < 0 || c->index >= count) {
            ok = FALSE; error = "preset index out of range";
        } else {
            SetTimerSeconds(GetPresetTime(c->index) * 60);
            g_timerState.currentPresetIndex = c->index;
        }
    } else if (strcmp(c->cmd, "appearance") == 0 || strcmp(c->cmd, "fonts") == 0) {
        // 只读，无副作用
    } else if (strcmp(c->cmd, "random_appearance") == 0) {
        // 与外观对话框「随机」按钮一致：字色、底色、不透明度都随机
        srand((unsigned)GetTickCount64());
        g_timerState.fontColor       = RGB(rand() % 256, rand() % 256, rand() % 256);
        g_timerState.backgroundColor = RGB(rand() % 256, rand() % 256, rand() % 256);
        g_timerState.windowOpacity   = (BYTE)(150 + rand() % 106);
        SaveAppearanceConfig();
        RedrawMainWindow();
    } else if (strcmp(c->cmd, "set_font") == 0) {
        wchar_t names[32][256];
        int count = GetMixedFontList(names, 32);
        if (!c->hasIndex) {
            ok = FALSE; error = "set_font requires 'index'";
        } else if (c->index < 0 || c->index >= count) {
            ok = FALSE; error = "font index out of range";
        } else {
            ApplyFontByName(names[c->index]);
            RedrawMainWindow();
        }
    } else if (strcmp(c->cmd, "random_font") == 0) {
        wchar_t names[32][256];
        int count = GetMixedFontList(names, 32);
        if (count <= 0) {
            ok = FALSE; error = "no fonts available";
        } else {
            srand((unsigned)GetTickCount64() ^ 0x9E3779B9u);
            int idx = rand() % count;
            for (int tries = 0; tries < count && wcscmp(names[idx], g_timerState.currentFontName) == 0; tries++) {
                idx = rand() % count;   // 尽量换一个与当前不同的字体
            }
            c->index = idx;
            ApplyFontByName(names[idx]);
            RedrawMainWindow();
        }
    } else if (strcmp(c->cmd, "set_overtime") == 0) {
        if (!c->hasEnabled) {
            ok = FALSE; error = "set_overtime requires 'enabled'";
        } else {
            g_timerState.enableOvertimeCount = c->enabled;
            // 关闭功能且当前正在超时正计时：与菜单开关一致，退出超时模式并恢复配色
            if (!c->enabled && g_timerState.isInOvertimeMode) {
                g_timerState.isInOvertimeMode = FALSE;
                g_timerState.fontColor = g_timerState.originalFontColor;
                g_timerState.backgroundColor = g_timerState.originalBackgroundColor;
            }
            SaveAppearanceConfig();
            RedrawMainWindow();
        }
    } else if (strcmp(c->cmd, "set_colors") == 0) {
        COLORREF fc = g_timerState.fontColor;
        COLORREF bc = g_timerState.backgroundColor;
        if (!c->hasFontColor && !c->hasBgColor) {
            ok = FALSE; error = "set_colors requires 'font_color' or 'background_color'";
        } else if (c->hasFontColor && !ParseHexColor(c->fontColor, &fc)) {
            ok = FALSE; error = "invalid font_color (use #RRGGBB)";
        } else if (c->hasBgColor && !ParseHexColor(c->bgColor, &bc)) {
            ok = FALSE; error = "invalid background_color (use #RRGGBB)";
        } else {
            g_timerState.fontColor = fc;
            g_timerState.backgroundColor = bc;
            SaveAppearanceConfig();
            RedrawMainWindow();
        }
    } else if (strcmp(c->cmd, "set_transparent") == 0) {
        if (!c->hasEnabled) {
            ok = FALSE; error = "set_transparent requires 'enabled'";
        } else if (c->enabled != g_timerState.transparentBackground) {
            g_timerState.transparentBackground = c->enabled;
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
            if (c->enabled) {
                SetWindowRoundedCorners(hwnd, 0);
                SetWindowShadow(hwnd, FALSE);
                UpdateLayeredWindow_Render();
            } else {
                SetWindowRoundedCorners(hwnd, 12);
                SetWindowShadow(hwnd, TRUE);
                SetLayeredWindowAttributes(hwnd, 0, g_timerState.windowOpacity, LWA_ALPHA);
                InvalidateRect(hwnd, NULL, TRUE);
                UpdateWindow(hwnd);
            }
            SaveAppearanceConfig();
            Ui_RefreshAccent();
        }
    } else if (strcmp(c->cmd, "reset_appearance") == 0) {
        // 与外观对话框「重置」一致：白字 / 192 灰底 / 不透明
        g_timerState.fontColor       = RGB(255, 255, 255);
        g_timerState.backgroundColor = RGB(192, 192, 192);
        g_timerState.windowOpacity   = 255;
        SaveAppearanceConfig();
        RedrawMainWindow();
    } else if (strcmp(c->cmd, "audio") == 0) {
        // 只读
    } else if (strcmp(c->cmd, "set_audio") == 0) {
        if (!c->hasEnabled) {
            ok = FALSE; error = "set_audio requires 'enabled'";
        } else {
            SetAudioEnabled(c->enabled);
            SaveAudioConfig();
        }
    } else if (strcmp(c->cmd, "set_audio_language") == 0) {
        if (!c->hasLanguage) {
            ok = FALSE; error = "set_audio_language requires 'language'";
        } else if (strcmp(c->language, "zh") == 0) {
            SetAudioLanguage(TRUE); SaveAudioConfig();
        } else if (strcmp(c->language, "en") == 0) {
            SetAudioLanguage(FALSE); SaveAudioConfig();
        } else {
            ok = FALSE; error = "language must be 'zh' or 'en'";
        }
    } else if (strcmp(c->cmd, "set_custom_audio") == 0) {
        if (c->hasClear && c->clearFlag) {
            SetCustomAudio(FALSE, NULL);
            SaveAudioConfig();
        } else if (c->hasPath && c->path[0]) {
            wchar_t wpath[MAX_PATH];
            Utf8ToWide(c->path, wpath, MAX_PATH);
            DWORD attr = GetFileAttributesW(wpath);
            if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                ok = FALSE; error = "audio file not found";
            } else {
                SetCustomAudio(TRUE, wpath);
                SaveAudioConfig();
            }
        } else {
            ok = FALSE; error = "set_custom_audio requires 'path' or clear=true";
        }
    } else if (strcmp(c->cmd, "test_audio") == 0) {
        BOOL useCustom = c->hasCustom ? c->customFlag : g_timerState.useCustomAudio;
        BOOL played = useCustom
            ? TestCustomAudioPlayback(g_timerState.customAudioPath)
            : TestAudioPlayback(g_timerState.useChineseAudio);
        if (!played) {
            ok = FALSE; error = "audio test failed (file missing?)";
        }
    } else if (strcmp(c->cmd, "window") == 0) {
        if (!c->hasAction) {
            ok = FALSE; error = "window requires 'action'";
        } else if (strcmp(c->action, "show") == 0) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        } else if (strcmp(c->action, "hide") == 0) {
            ShowWindow(hwnd, SW_HIDE);
        } else {
            ok = FALSE; error = "action must be 'show' or 'hide'";
        }
    } else if (strcmp(c->cmd, "next_preset") == 0) {
        if (GetPresetCount() <= 0) {
            ok = FALSE; error = "no presets";
        } else {
            CycleToNextPresetTime();
        }
    } else {
        ok = FALSE; error = "unknown command";
    }

    BuildResponse(c, ok, error);

    if (ok) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
    SetEvent(c->hEvent);
}

// ===========================================
// 管道线程：收发 JSON，把命令转交 UI 线程
// ===========================================

static void ServeClient(HANDLE hPipe) {
    char buf[IPC_BUF_SIZE];
    DWORD got = 0;
    if (!ReadFile(hPipe, buf, IPC_BUF_SIZE - 1, &got, NULL) || got == 0) {
        return;
    }
    buf[got] = '\0';

    IpcCommand* c = (IpcCommand*)calloc(1, sizeof(IpcCommand));
    if (!c) return;

    if (!ParseRequest(buf, c)) {
        snprintf(c->response, IPC_RESP_SIZE,
                 "{\"ok\":false,\"cmd\":\"\",\"error\":\"invalid JSON request\"}");
    } else {
        c->hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!c->hEvent) {
            snprintf(c->response, IPC_RESP_SIZE,
                     "{\"ok\":false,\"cmd\":\"%s\",\"error\":\"internal error\"}", c->cmd);
        } else if (!PostMessageW(g_ipcHwnd, WM_IPC_COMMAND, 0, (LPARAM)c)) {
            // UI 线程从未收到命令，可安全释放
            snprintf(c->response, IPC_RESP_SIZE,
                     "{\"ok\":false,\"cmd\":\"%s\",\"error\":\"window unavailable\"}", c->cmd);
            CloseHandle(c->hEvent);
            free(c);
            return;
        } else {
            DWORD w = WaitForSingleObject(c->hEvent, IPC_WAIT_MS);
            if (w != WAIT_OBJECT_0) {
                // 超时：UI 线程可能稍后仍会处理该命令并访问 c / hEvent。
                // 此处故意不释放（异常路径少量泄漏），避免悬空指针 / 无效句柄。
                snprintf(c->response, IPC_RESP_SIZE,
                         "{\"ok\":false,\"cmd\":\"%s\",\"error\":\"UI thread timeout\"}", c->cmd);
                size_t elen = strlen(c->response);
                char eline[IPC_RESP_SIZE + 2];
                memcpy(eline, c->response, elen);
                eline[elen] = '\n';
                DWORD ew = 0;
                WriteFile(hPipe, eline, (DWORD)(elen + 1), &ew, NULL);
                FlushFileBuffers(hPipe);
                return;
            }
            CloseHandle(c->hEvent);
        }
    }

    // 回写一行 JSON
    size_t len = strlen(c->response);
    char line[IPC_RESP_SIZE + 2];
    memcpy(line, c->response, len);
    line[len] = '\n';
    DWORD written = 0;
    WriteFile(hPipe, line, (DWORD)(len + 1), &written, NULL);
    FlushFileBuffers(hPipe);
    free(c);
}

static DWORD WINAPI IpcThreadProc(LPVOID param) {
    (void)param;
    while (!g_ipcStop) {
        HANDLE hPipe = CreateNamedPipeW(
            TIMMER_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1, IPC_BUF_SIZE, IPC_RESP_SIZE, 0, NULL);
        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(200);
            continue;
        }

        BOOL connected = ConnectNamedPipe(hPipe, NULL)
                         ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (g_ipcStop) {
            CloseHandle(hPipe);
            break;
        }
        if (connected) {
            ServeClient(hPipe);
        }
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
    return 0;
}

void Ipc_Start(HWND hMainWnd) {
    g_ipcHwnd = hMainWnd;
    g_ipcStop = 0;
    g_ipcThread = CreateThread(NULL, 0, IpcThreadProc, NULL, 0, NULL);
}

void Ipc_Shutdown(void) {
    if (!g_ipcThread) return;
    InterlockedExchange(&g_ipcStop, 1);

    // 用一个 dummy 客户端唤醒阻塞中的 ConnectNamedPipe / ReadFile
    HANDLE h = CreateFileW(TIMMER_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, "\n", 1, &written, NULL);
        CloseHandle(h);
    }

    WaitForSingleObject(g_ipcThread, 2000);
    CloseHandle(g_ipcThread);
    g_ipcThread = NULL;
}