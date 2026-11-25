# Timmmer Agent 控制接口

让 AI agent / 脚本控制并查询桌面计时器。底层是命名管道 `\\.\pipe\timmmer`
（Win32 原生、本机、当前用户），所有命令都返回**执行后的状态**，便于闭环判断。

```
AI agent ──MCP 工具──> timmmer_mcp.py ──┐
                                        ├──命名管道(JSON)──> Timmmer GUI
AI agent ──shell─────> timmmerctl.py ───┘
```

C 侧实现见 `src/ipc/timer_ipc.c`：管道线程收发 JSON，命令经 `WM_IPC_COMMAND`
转交 UI 线程执行（计时状态只在 UI 线程读写）。已加单实例 mutex，保证管道对应唯一实例。

## 文件

| 文件 | 作用 |
|---|---|
| `timmmer_ipc.py` | 管道客户端（零依赖，ctypes），MCP 与 CLI 共用 |
| `timmmerctl.py` | JSON CLI，供 shell 型 agent |
| `timmmer_mcp.py` | 零依赖 MCP 服务（stdio JSON-RPC），供 MCP 型 agent |

## JSON CLI

```bash
python timmmerctl.py status            # 查询状态
python timmmerctl.py set 25m           # 设任意时间: 25m / 1h30m / 90s / 1500
python timmmerctl.py start             # 开始
python timmmerctl.py pause             # 暂停
python timmmerctl.py toggle            # 开始↔暂停
python timmmerctl.py reset             # 重置为上次时间
python timmmerctl.py presets           # 列出预设（分钟）
python timmmerctl.py preset 0          # 使用第 0 个预设
python timmmerctl.py appearance        # 查询当前配色/字体
python timmmerctl.py random-appearance # 随机配色（字色+底色+不透明度）
python timmmerctl.py fonts             # 列出可用字体
python timmmerctl.py font 3            # 切换到第 3 个字体
python timmmerctl.py random-font       # 随机换一个字体
python timmmerctl.py colors "#FFCC00" "#1A1A2E"  # 指定字色/底色
python timmmerctl.py transparent on    # 透明背景模式
python timmmerctl.py reset-appearance  # 恢复默认配色
python timmmerctl.py overtime on       # 超时正计时开关
python timmmerctl.py audio             # 查询音频设置
python timmmerctl.py audio on          # 提示音开关
python timmmerctl.py audio-language zh # 提示音语言
python timmmerctl.py audio-custom "D:/alert.wav"  # 自定义提示音（clear 取消）
python timmmerctl.py audio-test        # 试听
python timmmerctl.py window hide       # 隐藏/显示主窗口
python timmmerctl.py next-preset       # 循环到下一个预设
```

输出为 JSON，退出码 0=成功 / 1=失败。计时器未运行时返回 `{"ok":false,"error":...}`。

## MCP 服务

工具：`timer_status` `timer_start` `timer_pause` `timer_toggle` `timer_reset`
`timer_set(seconds)` `timer_presets` `timer_use_preset(index)`
`timer_appearance` `timer_random_appearance` `timer_fonts` `timer_set_font(index)` `timer_random_font`
`timer_set_colors` `timer_set_transparent` `timer_reset_appearance` `timer_set_overtime`
`timer_audio` `timer_set_audio` `timer_set_audio_language` `timer_set_custom_audio` `timer_test_audio`
`timer_window(action)` `timer_next_preset`。

本仓库已在 ZCode 全局配置（`~/.zcode/cli/config.json` 的 `mcp.servers.timmmer`）注册，
重载 ZCode 后即可原生调用。其它 MCP 客户端通用配置：

```json
{
  "mcpServers": {
    "timmmer": {
      "command": "python",
      "args": ["E:/Timmmer/scripts/agent/timmmer_mcp.py"]
    }
  }
}
```

## 状态字段语义

每条响应都含：

- `running` — 是否在计时
- `mode` — `running` / `paused` / `finished` / `overtime`
- `remaining_seconds` — 倒计时剩余秒（finished/overtime 时为 0）
- `overtime_seconds` — 超时正计时已走秒（仅 overtime 模式非 0）
- `seconds` — 原始值（倒计时=剩余；overtime=已超时秒数）
- `display` — 当前显示文本，如 `25:00`
- `time_up` / `overtime` — 布尔标志
- `visible` — 主窗口是否可见
- `overtime_enabled` — 超时正计时开关（开启后归零转入 overtime）
- `error` — 成功为 `null`，失败为原因

外观相关命令额外返回 `appearance` 对象：`font_color` / `background_color`（`#RRGGBB`）、
`opacity`、`font`、`font_count`；`fonts` 命令还含 `fonts` 数组。

音频相关命令额外返回 `audio` 对象：`enabled`、`language`（zh/en）、`custom`、`custom_path`。
`next_preset` 额外返回 `preset_index` / `preset_count`。

> ⚠️ **overtime 模式**：倒计时归零后若启用「超时正计时」，`seconds` 转为「已超时秒数」
> 且持续增加，`running` 仍为 true。判断是否计时结束请看 `mode` / `time_up`，不要只看 `running`。

## 管道协议（供二次开发）

每行一个 JSON 请求，回一行 JSON 响应：

```jsonc
{"cmd":"status"} | {"cmd":"start"} | {"cmd":"pause"} | {"cmd":"toggle"} | {"cmd":"reset"}
{"cmd":"set","seconds":1500}        // 0..359999
{"cmd":"presets"}
{"cmd":"preset","index":0}
{"cmd":"appearance"}                // 查询配色/字体
{"cmd":"random_appearance"}         // 随机字色+底色+不透明度
{"cmd":"fonts"}                     // 列出字体（索引即 set_font 的参数）
{"cmd":"set_font","index":3}
{"cmd":"random_font"}
{"cmd":"set_colors","font_color":"#FFCC00","background_color":"#1A1A2E"}
{"cmd":"set_transparent","enabled":true}
{"cmd":"reset_appearance"}
{"cmd":"set_overtime","enabled":true}
{"cmd":"audio"} | {"cmd":"set_audio","enabled":true} | {"cmd":"set_audio_language","language":"zh"}
{"cmd":"set_custom_audio","path":"D:\\alert.wav"} | {"cmd":"set_custom_audio","clear":true}
{"cmd":"test_audio"} | {"cmd":"test_audio","custom":true}
{"cmd":"window","action":"hide"}
{"cmd":"next_preset"}
```

## 构建

C 改动已并入 `build.bat`（新增 `-Isrc/ipc` 与 `src/ipc/timer_ipc.c`），正常 `build.bat` 即可。