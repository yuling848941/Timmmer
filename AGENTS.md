# Timer - Windows桌面计时器

## 编译命令

```cmd
# 使用 MinGW 编译 (运行项目自带的脚本即可)
build.bat
```

## 项目结构

- **入口**: `src/main_audio_optimized.c`
- **核心模块**: `src/core/timer_core.c/h` - 计时逻辑
- **UI**: `src/ui/timer_ui.c/h` - 渲染和显示
- **窗口**: `src/ui/timer_window.c/h` - 窗口管理
- **托盘**: `src/ui/timer_tray.c/h` + `src/ui/tray_panel.c/h` - 托盘图标与左键快速操控面板
- **渲染工具**: `src/ui/timer_render_utils.c/h` - 圆角/阴影/开关等像素级抗锯齿绘制（菜单、对话框、面板共用）
- **对话框**: `src/dialogs/` - 对话框模块 (已拆分为多个文件)
- **配置**: `src/core/timer_config.c/h` - 配置管理
- **音频**: `src/audio/timer_audio.c/h` - 音频播放
- **字体**: `src/fonts/` - 字体管理与资源
- **资源**: `res/` - 图标、音频和资源定义文件
- **输出**: `build/` - 编译生成的程序 (Timmmer.exe) 和临时文件

## 配置文件

程序在用户目录 `%USERPROFILE%/` 下创建统一的配置文件:
- `timer_config.ini` - 时间格式、外观、预设时间、音频、最后时间与窗口状态的统一配置（由 `src/core/timer_config.c` 管理）

## 运行时依赖

- Windows API (Win32)
- GDI+, DirectShow (winmm)
- 不需要额外运行时库（静态链接）

## 注意事项

1. 编译需要 MinGW-w64 环境
2. 资源文件 (`timer_resource.o`) 需要先编译 `.rc` 文件生成
3. 音频文件缺失时有智能回退机制
