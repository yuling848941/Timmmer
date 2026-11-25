#!/usr/bin/env python3
"""Timmmer MCP 服务 —— 把桌面计时器暴露为 AI agent 原生工具。

零依赖：直接实现 MCP 的 stdio JSON-RPC 传输（newline-delimited JSON），
无需 `pip install mcp`。底层与 JSON CLI 共用命名管道 \\\\.\\pipe\\timmmer。

接入示例（MCP 客户端配置）:
    {
      "mcpServers": {
        "timmmer": {
          "command": "python",
          "args": ["E:/Timmmer/scripts/agent/timmmer_mcp.py"]
        }
      }
    }
"""

import json
import sys

import timmmer_ipc as ipc

SERVER_INFO = {"name": "timmmer", "version": "1.0.0"}
DEFAULT_PROTOCOL = "2024-11-05"

_STATE_NOTE = (
    "返回执行后的状态: running(是否运行), mode(running/paused/finished/overtime), "
    "remaining_seconds(倒计时剩余秒), overtime_seconds(超时正计时已走秒), "
    "display(当前显示文本), time_up, overtime。"
    "注意: overtime 模式下 seconds 是『已超时秒数』且在增加，remaining_seconds 为 0。"
)

TOOLS = [
    {
        "name": "timer_status",
        "description": "查询计时器当前状态（只读）。" + _STATE_NOTE,
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_start",
        "description": "开始/继续计时。" + _STATE_NOTE,
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_pause",
        "description": "暂停计时（保留剩余时间）。" + _STATE_NOTE,
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_toggle",
        "description": "在开始与暂停之间切换（等同主按钮）。" + _STATE_NOTE,
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_reset",
        "description": "停止并重置为上次设置的时间。" + _STATE_NOTE,
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_set",
        "description": "设置任意倒计时时间（会先停表）。seconds 范围 0..359999。" + _STATE_NOTE,
        "inputSchema": {
            "type": "object",
            "properties": {
                "seconds": {"type": "integer", "minimum": 0, "maximum": 359999,
                            "description": "倒计时总秒数，例如 25 分钟 = 1500"},
            },
            "required": ["seconds"],
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_presets",
        "description": "列出预设时间（单位: 分钟）及数量。响应含 presets 数组与 preset_count。",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_use_preset",
        "description": "使用第 index 个预设（0 起，会先停表并设为该预设时间）。" + _STATE_NOTE,
        "inputSchema": {
            "type": "object",
            "properties": {
                "index": {"type": "integer", "minimum": 0, "description": "预设索引，从 0 开始"},
            },
            "required": ["index"],
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_appearance",
        "description": "查询当前外观：字色、底色、不透明度、字体名、可用字体数量。",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_random_appearance",
        "description": "随机配色：字色、底色、不透明度一起随机（等同外观设置里的“随机”）。",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_fonts",
        "description": "列出可用字体（响应 appearance.fonts 数组，可直接用于 timer_set_font 的 index）。",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_set_font",
        "description": "切换到第 index 个字体（先用 timer_fonts 查看索引）。",
        "inputSchema": {
            "type": "object",
            "properties": {
                "index": {"type": "integer", "minimum": 0, "description": "字体索引，从 0 开始"},
            },
            "required": ["index"],
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_random_font",
        "description": "随机换一个与当前不同的字体。",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_set_overtime",
        "description": "超时正计时开关：开启后倒计时归零会继续正计时（mode=overtime），关闭则停在 finished。",
        "inputSchema": {
            "type": "object",
            "properties": {"enabled": {"type": "boolean"}},
            "required": ["enabled"],
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_set_colors",
        "description": "指定字色/底色（#RRGGBB）。两者至少提供一个，可只改一个。",
        "inputSchema": {
            "type": "object",
            "properties": {
                "font_color": {"type": "string", "description": "字色，如 #FFCC00"},
                "background_color": {"type": "string", "description": "底色，如 #1A1A2E"},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_set_transparent",
        "description": "透明背景模式开关。",
        "inputSchema": {
            "type": "object",
            "properties": {"enabled": {"type": "boolean"}},
            "required": ["enabled"],
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_reset_appearance",
        "description": "恢复默认外观：白字、192 灰底、不透明度 255。",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_audio",
        "description": "查询音频设置：是否启用、语言(zh/en)、是否自定义、自定义路径。",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "timer_set_audio",
        "description": "开启/关闭超时提示音。",
        "inputSchema": {
            "type": "object",
            "properties": {"enabled": {"type": "boolean"}},
            "required": ["enabled"],
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_set_audio_language",
        "description": "设置提示音语言：zh=中文，en=英文。",
        "inputSchema": {
            "type": "object",
            "properties": {"language": {"type": "string", "enum": ["zh", "en"]}},
            "required": ["language"],
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_set_custom_audio",
        "description": "设置自定义提示音文件路径，或 clear=true 取消自定义。文件必须存在。",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string", "description": "音频文件绝对路径"},
                "clear": {"type": "boolean", "description": "true=取消自定义提示音"},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_test_audio",
        "description": "试听提示音（同步播放，约 1-3 秒）。custom=true 试听自定义文件。",
        "inputSchema": {
            "type": "object",
            "properties": {"custom": {"type": "boolean"}},
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_window",
        "description": "显示或隐藏主窗口（计时继续运行）。",
        "inputSchema": {
            "type": "object",
            "properties": {"action": {"type": "string", "enum": ["show", "hide"]}},
            "required": ["action"],
            "additionalProperties": False,
        },
    },
    {
        "name": "timer_next_preset",
        "description": "循环到下一个预设时间（只设值，不自动开始）。",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
]


def tool_to_cmd(name: str, args: dict) -> dict:
    if name == "timer_set":
        return {"cmd": "set", "seconds": int(args["seconds"])}
    if name == "timer_use_preset":
        return {"cmd": "preset", "index": int(args["index"])}
    if name == "timer_set_font":
        return {"cmd": "set_font", "index": int(args["index"])}
    if name == "timer_set_overtime":
        return {"cmd": "set_overtime", "enabled": bool(args["enabled"])}
    if name == "timer_set_colors":
        cmd = {"cmd": "set_colors"}
        if "font_color" in args:
            cmd["font_color"] = args["font_color"]
        if "background_color" in args:
            cmd["background_color"] = args["background_color"]
        return cmd
    if name == "timer_set_transparent":
        return {"cmd": "set_transparent", "enabled": bool(args["enabled"])}
    if name == "timer_set_audio":
        return {"cmd": "set_audio", "enabled": bool(args["enabled"])}
    if name == "timer_set_audio_language":
        return {"cmd": "set_audio_language", "language": args["language"]}
    if name == "timer_set_custom_audio":
        cmd = {"cmd": "set_custom_audio"}
        if "path" in args:
            cmd["path"] = args["path"]
        if "clear" in args:
            cmd["clear"] = bool(args["clear"])
        return cmd
    if name == "timer_test_audio":
        cmd = {"cmd": "test_audio"}
        if "custom" in args:
            cmd["custom"] = bool(args["custom"])
        return cmd
    if name == "timer_window":
        return {"cmd": "window", "action": args["action"]}
    mapping = {
        "timer_status": "status",
        "timer_start": "start",
        "timer_pause": "pause",
        "timer_toggle": "toggle",
        "timer_reset": "reset",
        "timer_presets": "presets",
        "timer_appearance": "appearance",
        "timer_random_appearance": "random_appearance",
        "timer_fonts": "fonts",
        "timer_random_font": "random_font",
        "timer_reset_appearance": "reset_appearance",
        "timer_audio": "audio",
        "timer_next_preset": "next_preset",
    }
    if name in mapping:
        return {"cmd": mapping[name]}
    raise KeyError(name)


def handle_tools_call(req_id, params):
    name = params.get("name")
    args = params.get("arguments") or {}
    try:
        cmd = tool_to_cmd(name, args)
    except (KeyError, ValueError, TypeError) as e:
        return error(req_id, -32602, f"未知工具或参数错误: {name} ({e})")
    try:
        result = ipc.call(cmd)
    except ipc.TimerNotRunning as e:
        return result_text(req_id, {"ok": False, "error": str(e)}, is_error=True)
    return result_text(req_id, result, is_error=not result.get("ok", False))


def result_text(req_id, payload, is_error=False):
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "result": {
            "content": [{"type": "text", "text": json.dumps(payload, ensure_ascii=False)}],
            "isError": is_error,
        },
    }


def ok(req_id, result):
    return {"jsonrpc": "2.0", "id": req_id, "result": result}


def error(req_id, code, message):
    return {"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}}


def dispatch(msg):
    method = msg.get("method")
    req_id = msg.get("id")
    params = msg.get("params") or {}

    # 通知（无 id）不需要响应
    if req_id is None:
        return None

    if method == "initialize":
        return ok(req_id, {
            "protocolVersion": params.get("protocolVersion") or DEFAULT_PROTOCOL,
            "capabilities": {"tools": {"listChanged": False}},
            "serverInfo": SERVER_INFO,
        })
    if method == "tools/list":
        return ok(req_id, {"tools": TOOLS})
    if method == "tools/call":
        return handle_tools_call(req_id, params)
    if method == "ping":
        return ok(req_id, {})
    return error(req_id, -32601, f"Method not found: {method}")


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stdin.reconfigure(encoding="utf-8")
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            continue
        try:
            resp = dispatch(msg)
        except Exception as e:  # 兜底，避免服务崩溃
            resp = error(msg.get("id"), -32603, f"Internal error: {e}")
        if resp is not None:
            sys.stdout.write(json.dumps(resp, ensure_ascii=False) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()