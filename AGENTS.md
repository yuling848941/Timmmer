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
- **对话框**: `src/dialogs/` - 对话框模块 (已拆分为多个文件)
- **配置**: `src/core/timer_config.c/h` - 配置管理
- **音频**: `src/audio/timer_audio.c/h` - 音频播放
- **字体**: `src/fonts/` - 字体管理与资源
- **资源**: `res/` - 图标、音频和资源定义文件
- **输出**: `build/` - 编译生成的程序和临时文件

## 配置文件

程序在用户目录 `%USERPROFILE%/` 下创建:
- `timer_format_config.ini` - 时间格式
- `timer_appearance_config.ini` - 外观设置
- `timer_preset_config.ini` - 预设时间
- `timer_audio_config.ini` - 音频设置
- `timer_last_time.ini` - 最后时间
- `timer_window_config.ini` - 窗口位置

## 运行时依赖

- Windows API (Win32)
- GDI+, DirectShow (winmm)
- 不需要额外运行时库（静态链接）

## 注意事项

1. 编译需要 MinGW-w64 环境
2. 资源文件 (`timer_resource.o`) 需要先编译 `.rc` 文件生成
3. 音频文件缺失时有智能回退机制