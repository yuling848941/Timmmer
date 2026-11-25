"""Timmmer 命名管道客户端（零依赖，仅用标准库 ctypes）。

MCP 服务与 JSON CLI 共用此模块。协议：向 \\\\.\\pipe\\timmmer 写一行 JSON 请求，
读一行 JSON 响应（含执行后的状态）。详见 src/ipc/timer_ipc.c。
"""

import ctypes
import json
import time
from ctypes import wintypes

PIPE_NAME = r"\\.\pipe\timmmer"

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
ERROR_FILE_NOT_FOUND = 2
ERROR_PIPE_BUSY = 231

_k32 = ctypes.WinDLL("kernel32", use_last_error=True)

_k32.CreateFileW.argtypes = [
    wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
    ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE,
]
_k32.CreateFileW.restype = wintypes.HANDLE
_k32.WaitNamedPipeW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
_k32.WaitNamedPipeW.restype = wintypes.BOOL
_k32.ReadFile.argtypes = [
    wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p,
]
_k32.ReadFile.restype = wintypes.BOOL
_k32.WriteFile.argtypes = [
    wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p,
]
_k32.WriteFile.restype = wintypes.BOOL
_k32.CloseHandle.argtypes = [wintypes.HANDLE]
_k32.CloseHandle.restype = wintypes.BOOL


class TimerNotRunning(ConnectionError):
    """计时器进程未运行，或管道不可用。"""


def call(cmd: dict, retries: int = 20, retry_delay: float = 0.05) -> dict:
    """发送一条命令并返回解析后的 JSON 响应。

    管道忙 / 尚未就绪时自动重试；始终连不上则抛 TimerNotRunning。
    """
    payload = (json.dumps(cmd, ensure_ascii=False) + "\n").encode("utf-8")
    last_err = None
    for _ in range(retries):
        h = _k32.CreateFileW(
            PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, None,
            OPEN_EXISTING, 0, None,
        )
        if h and h != INVALID_HANDLE_VALUE:
            try:
                return _transact(h, payload)
            finally:
                _k32.CloseHandle(h)
        err = ctypes.get_last_error()
        last_err = err
        if err in (ERROR_PIPE_BUSY, ERROR_FILE_NOT_FOUND):
            _k32.WaitNamedPipeW(PIPE_NAME, 200)
            time.sleep(retry_delay)
            continue
        raise TimerNotRunning(f"无法连接管道 (WinError {err})，计时器是否在运行？")
    raise TimerNotRunning(f"无法连接管道 (WinError {last_err})，计时器是否在运行？")


def _transact(h, payload: bytes) -> dict:
    written = wintypes.DWORD(0)
    if not _k32.WriteFile(h, payload, len(payload), ctypes.byref(written), None):
        raise TimerNotRunning(f"写入失败 (WinError {ctypes.get_last_error()})")
    buf = ctypes.create_string_buffer(65536)
    got = wintypes.DWORD(0)
    if not _k32.ReadFile(h, buf, len(buf) - 1, ctypes.byref(got), None):
        raise TimerNotRunning(f"读取失败 (WinError {ctypes.get_last_error()})")
    text = buf.raw[: got.value].decode("utf-8", "replace").strip()
    return json.loads(text)


def status() -> dict:
    return call({"cmd": "status"})