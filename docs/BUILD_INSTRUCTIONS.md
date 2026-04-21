# Timer 程序编译说明

## 系统要求

- Windows 10/11
- GCC 编译器 (MinGW 或 MSYS2)
- 必要的 Windows SDK 库

## 编译步骤

### 1. 安装 GCC

如果尚未安装 GCC，可以通过以下方式之一安装：

**选项 A: MSYS2 (推荐)**
1. 从 https://www.msys2.org/ 下载并安装 MSYS2
2. 打开 MSYS2 UCRT64 终端
3. 运行：`pacman -S mingw-w64-ucrt-x86_64-gcc`

**选项 B: MinGW-w64**
1. 从 https://www.mingw-w64.org/ 下载
2. 添加 MinGW 的 bin 目录到系统 PATH

### 2. 编译程序

在项目根目录中打开命令行（终端），运行：

```bash
build.bat
```

或者手动执行以下命令：

```bash
gcc -o timer_audio_optimized.exe ^
    main_audio_optimized.c ^
    timer_core.c ^
    timer_ui.c ^
    timer_window.c ^
    timer_dialogs.c ^
    timer_config.c ^
    timer_audio.c ^
    timer_embedded_audio.c ^
    timer_font_manager.c ^
    timer_embedded_fonts.c ^
    timer_fonts.c ^
    timer_font_resources_optimized.c ^
    timer_resource.o ^
    -luser32 ^
    -lgdi32 ^
    -lshell32 ^
    -lcomctl32 ^
    -lcomdlg32 ^
    -ldwmapi ^
    -lwinmm ^
    -luxtheme ^
    -lole32 ^
    -luuid ^
    -static-libgcc ^
    -mwindows
```

### 3. 编译资源文件（如需要）

如果需要重新生成资源文件：

```bash
windres timer_resource.rc -o timer_resource.o
```

## 运行程序

编译成功后，双击 `timer_audio_optimized.exe` 即可运行。

## 项目文件说明

| 文件 | 说明 |
|------|------|
| main_audio_optimized.c | 主程序入口 |
| timer_core.c/h | 核心定时器逻辑 |
| timer_ui.c/h | 用户界面代码 |
| timer_window.c/h | 窗口管理 |
| timer_dialogs.c/h | 对话框处理 |
| timer_config.c/h | 配置管理 |
| timer_audio.c/h | 音频处理 |
| timer_font_manager.c/h | 字体管理 |
| timer_embedded_*.c/h | 嵌入式资源 |
| timer_fonts.c/h | 字体渲染 |
| timer_resource.rc | Windows 资源定义 |
| timer_config.ini | 配置文件模板 |
| timmmer-modern.ico | 程序图标 |

## 资源文件

- `audio/` - 音频说明文件
- `sound/` - 音频文件（WAV 格式）
- `fonts/` - 字体文件（MIT/OFL/SIL 许可）

## 故障排除

### 编译错误：找不到 gcc
确保 GCC 已正确安装并添加到系统 PATH。

### 编译错误：找不到库文件
确保安装了完整的 MinGW/MSYS2 开发环境。

### 运行时错误：找不到配置文件
确保 `timer_config.ini` 与可执行文件在同一目录。

## 许可证

详见 LICENSE.txt
