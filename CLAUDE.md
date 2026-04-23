# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Timmmer** is a native Windows desktop timer application written in pure C using the Win32 API. It features countdown/stopwatch modes, customizable appearance, system tray support, embedded fonts/audio, and ships as a portable ~550KB executable with no external runtime dependencies.

## Build Commands

```bash
# Build the project (requires MinGW-w64 with gcc and windres on PATH)
build.bat

# Output: build\Timmmer.exe
```

The build is a single `build.bat` script with two steps:
1. `windres` compiles `res\timer_resource.rc` → `build\timer_resource.o`
2. `gcc` compiles all `src/**/*.c` files with `-mwindows -municode -static-libgcc`, linking against: `user32`, `gdi32`, `shell32`, `comctl32`, `comdlg32`, `dwmapi`, `winmm`, `uxtheme`, `ole32`, `uuid`, `msimg32`

## Code Architecture

### Entry Point

`src/main_audio_optimized.c` — `wWinMain` initializes global state, loads configs, creates the timer window, and runs the standard Win32 message loop with `IsDialogMessage` dispatch for active dialogs.

### Module Structure

| Directory | Purpose |
|-----------|---------|
| `src/core/` | Core timing logic, INI configuration management, shared type definitions (`timer_types.h`) |
| `src/ui/` | Window creation, GDI+ rendering, double-buffering, system tray, window utilities |
| `src/dialogs/` | All UI dialogs (format, set time, appearance, audio, preset, about, integrated). Uses `timer_dialog_internal.h` and `timer_dialog_common.c` for shared logic |
| `src/audio/` | Audio playback via winmm/MCI, embedded audio data |
| `src/fonts/` | Font loading/management, embedded font data (620KB) |
| `src/menu/` | Context menus and right-click menu logic (custom-drawn, not native TrackPopupMenu) |
| `res/` | Icons, WAV audio files, embedded fonts (MIT/OFL/SIL licensed), resource scripts |

### Key Design Patterns

- **Double-buffered GDI rendering**: All UI painted to off-screen bitmap, then BitBlt'd to screen
- **Embedded resources**: Fonts (620KB) and audio data compiled directly into the executable via resource files
- **INI-based configuration**: Settings stored in `%USERPROFILE%/timer_config.ini` with sections: `[Format]`, `[General]`, `[Appearance]`, `[Audio]`, `[Presets]`, `[Timer]`, `[Window]`
- **Custom context menu**: Self-drawn menu implementation in `src/menu/` (not Windows native `TrackPopupMenu`) — uses `WM_NCHITTEST`, `SetCapture`, and manual mouse event handling for popup behavior
- **Modular dialogs**: v1.1.0 refactored a single 3200+ line dialog file into 8 focused modules

## Configuration

Runtime config file: `%USERPROFILE%/timer_config.ini`
Sample config: `timer_config.ini` in project root

## Important Notes

- **No automated testing**: Testing is manual only; no test framework exists
- **No Makefile/CMake**: Build is entirely driven by `build.bat`
- **UTF-8 encoding**: All source files and output should use UTF-8
- **Chinese language**: Primary documentation and commit messages are in Chinese
- **Custom menu implementation**: The right-click menu in `src/menu/ios_menu.c` and `src/menu/context_menu.c` is a fully custom-drawn implementation, not using Windows native menu APIs. This affects how menu dismissal, focus, and mouse interaction behave compared to native Windows menus.
中文回答我的问题