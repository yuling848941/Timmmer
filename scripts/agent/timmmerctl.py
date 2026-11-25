#!/usr/bin/env python3
"""Timmmer JSON CLI —— 供 shell 型 agent 使用，输出结构化 JSON。

用法:
    python timmmerctl.py status
    python timmmerctl.py start | pause | toggle | reset
    python timmmerctl.py set 25m | 1h30m | 90s | 1500
    python timmmerctl.py presets
    python timmmerctl.py preset 0

每条命令都返回执行后的状态（running / mode / remaining_seconds / display ...）。
退出码: 0=成功, 1=失败。
"""

import json
import re
import sys

import timmmer_ipc as ipc


def parse_duration(text: str) -> int:
    """把 '1500' / '25m' / '1h30m' / '90s' 解析为秒。"""
    text = text.strip().lower()
    if re.fullmatch(r"\d+", text):
        return int(text)
    parts = re.findall(r"(\d+)\s*([hms])", text)
    if not parts or re.sub(r"[\d\s hms]", "", text):
        raise ValueError(f"无法解析时长: {text!r}（示例: 25m / 1h30m / 90s / 1500）")
    unit = {"h": 3600, "m": 60, "s": 1}
    return sum(int(n) * unit[u] for n, u in parts)


def parse_onoff(text: str) -> bool:
    t = text.strip().lower()
    if t in ("on", "true", "1", "yes"):
        return True
    if t in ("off", "false", "0", "no"):
        return False
    raise ValueError(f"需要 on/off: {text!r}")


def build_command(argv) -> dict:
    if not argv:
        raise ValueError("缺少命令。运行 `timmmerctl.py help` 查看用法。")
    op = argv[0].lower()
    if op in ("status", "start", "pause", "toggle", "reset", "presets"):
        return {"cmd": op}
    if op in ("appearance", "fonts"):
        return {"cmd": op}
    if op == "random-appearance":
        return {"cmd": "random_appearance"}
    if op == "random-font":
        return {"cmd": "random_font"}
    if op == "font":
        if len(argv) < 2:
            raise ValueError("font 需要索引，例如: font 0（先用 fonts 查看）")
        return {"cmd": "set_font", "index": int(argv[1])}
    if op == "set":
        if len(argv) < 2:
            raise ValueError("set 需要时长，例如: set 25m")
        return {"cmd": "set", "seconds": parse_duration(argv[1])}
    if op == "preset":
        if len(argv) < 2:
            raise ValueError("preset 需要索引，例如: preset 0")
        return {"cmd": "preset", "index": int(argv[1])}
    if op == "overtime":
        if len(argv) < 2:
            raise ValueError("overtime 需要 on/off")
        return {"cmd": "set_overtime", "enabled": parse_onoff(argv[1])}
    if op == "colors":
        if len(argv) < 2:
            raise ValueError("colors 需要颜色，例如: colors #FFCC00 [#1A1A2E]")
        cmd = {"cmd": "set_colors", "font_color": argv[1]}
        if len(argv) >= 3:
            cmd["background_color"] = argv[2]
        return cmd
    if op == "transparent":
        if len(argv) < 2:
            raise ValueError("transparent 需要 on/off")
        return {"cmd": "set_transparent", "enabled": parse_onoff(argv[1])}
    if op == "reset-appearance":
        return {"cmd": "reset_appearance"}
    if op == "audio":
        if len(argv) >= 2:
            return {"cmd": "set_audio", "enabled": parse_onoff(argv[1])}
        return {"cmd": "audio"}
    if op == "audio-language":
        if len(argv) < 2:
            raise ValueError("audio-language 需要 zh 或 en")
        return {"cmd": "set_audio_language", "language": argv[1].lower()}
    if op == "audio-custom":
        if len(argv) < 2:
            raise ValueError("audio-custom 需要文件路径，或 clear")
        if argv[1].lower() == "clear":
            return {"cmd": "set_custom_audio", "clear": True}
        return {"cmd": "set_custom_audio", "path": argv[1]}
    if op == "audio-test":
        cmd = {"cmd": "test_audio"}
        if len(argv) >= 2:
            arg = argv[1].lower()
            if arg == "custom":
                cmd["custom"] = True
            elif arg == "builtin":
                cmd["custom"] = False
            else:
                cmd["custom"] = parse_onoff(arg)
        return cmd
    if op == "window":
        if len(argv) < 2:
            raise ValueError("window 需要 show/hide")
        return {"cmd": "window", "action": argv[1].lower()}
    if op == "next-preset":
        return {"cmd": "next_preset"}
    if op in ("help", "--help", "-h"):
        return {"cmd": "__help__"}
    raise ValueError(f"未知命令: {op}")


HELP = """Timmmer JSON CLI
  status              查询状态
  start / pause       开始 / 暂停
  toggle              开始↔暂停切换
  reset               重置为上次设置的时间
  set <duration>      设任意时间 (25m / 1h30m / 90s / 1500)
  presets             列出预设（分钟）
  preset <index>      使用第 index 个预设（0 起）
  appearance          查询当前配色/字体
  random-appearance   随机配色（字色+底色+不透明度）
  fonts               列出可用字体
  font <index>        切换到第 index 个字体
  random-font         随机换一个字体
  colors <hex> [bg]   指定字色（可选底色），如 colors #FFCC00 #1A1A2E
  transparent on|off  透明背景模式
  reset-appearance    恢复默认配色（白字/灰底/不透明）
  overtime on|off     超时正计时开关
  audio [on|off]      查询音频设置 / 开关提示音
  audio-language zh|en 提示音语言
  audio-custom <path>|clear  自定义提示音
  audio-test [custom] 试听（custom=试听自定义文件）
  window show|hide    显示/隐藏主窗口
  next-preset         循环到下一个预设
所有命令输出执行后的 JSON 状态。"""


def main() -> int:
    try:
        cmd = build_command(sys.argv[1:])
    except ValueError as e:
        print(json.dumps({"ok": False, "error": str(e)}, ensure_ascii=False))
        return 1

    if cmd.get("cmd") == "__help__":
        print(HELP)
        return 0

    try:
        result = ipc.call(cmd)
    except ipc.TimerNotRunning as e:
        print(json.dumps({"ok": False, "error": str(e)}, ensure_ascii=False))
        return 1

    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())