# Timmmer 编译环境配置指南

## 系统要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Windows 10 / 11（64 位） |
| 架构 | x86_64 |
| 磁盘空间 | ≥ 2 GB（含 MinGW 工具链 + 源码 + 字体资源） |

## 编译工具链

### MinGW-w64（必须）

项目使用 GCC 编译，需要安装 MinGW-w64。

**方法一：MSYS2（推荐）**

1. 下载 MSYS2 安装器：https://www.msys2.org/
2. 运行安装器，默认安装到 `C:\msys64`
3. 打开「MSYS2 MSYS」终端，执行：

```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils
```

4. 将以下路径添加到系统环境变量 `PATH`（优先级放前面）：

```
C:\msys64\mingw64\bin
```

**方法二：MinGW-w64 独立版**

1. 访问 https://www.mingw-w64.org/downloads/ 或 https://github.com/niXman/mingw-builds-binaries/releases
2. 下载 `x86_64-*-release-posix-seh-rt_v6-rev0.7z`
3. 解压到例如 `C:\mingw64`
4. 将 `C:\mingw64\bin` 添加到系统环境变量 `PATH`

### 验证安装

打开 CMD 或 PowerShell，执行：

```
gcc --version
windres --version
```

应输出 GCC 版本信息（≥ 8.0 即可）。若提示找不到命令，说明 PATH 配置有误。

## 项目目录结构

```
Timmmer/
├── build.bat                          # 一键编译脚本
├── main_audio_optimized.c             # 程序入口
├── timer_core.c / .h                  # 计时逻辑
├── timer_ui.c / .h                    # 界面渲染
├── timer_window.c / .h                # 窗口管理
├── timer_dialogs.c / .h               # 对话框
├── timer_config.c / .h                # 配置读写
├── timer_audio.c / .h                 # 音频播放
├── timer_embedded_audio.c / .h        # 嵌入式音频数据
├── timer_fonts.c / .h                 # 字体渲染
├── timer_font_manager.c / .h          # 字体管理器
├── timer_embedded_fonts.c / .h        # 嵌入式字体数据
├── timer_font_resources_optimized.c   # 字体资源
├── timer_font_resources.h             # 字体资源声明
├── timer_types.h                      # 类型定义
├── timer_resource.rc                  # Windows 资源脚本
├── timer_resource.o                   # 已编译的资源文件
├── timer_font_manager.o               # 已编译的字体管理器
├── timmmer-modern.ico                 # 应用图标
├── fonts/                             # 运行时加载的字体文件
│   ├── SIL/                           # SIL 协议字体
│   ├── OFL/                           # OFL 协议字体
│   └── MIT/                           # MIT 协议字体
└── sound/                             # 音频文件
    ├── TimeOver.wav
    └── 您已超时.wav
```

## 编译步骤

### 1. 完整编译（推荐）

双击 `build.bat` 或在项目根目录下打开终端执行：

```
build.bat
```

编译成功后生成 `timer_audio_optimized.exe`。

### 2. 手动编译

```
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
    -luser32 -lgdi32 -lshell32 -lcomctl32 ^
    -lcomdlg32 -ldwmapi -lwinmm -luxtheme ^
    -lole32 -luuid ^
    -static-libgcc -mwindows
```

### 3. 资源文件重新编译（仅修改 .rc/.ico 时需要）

如果修改了 `timer_resource.rc` 或图标、音频文件，需要重新编译资源：

```
windres timer_resource.rc -O coff -o timer_resource.o
```

## 链接的 Windows 系统库说明

| 库 | 用途 |
|----|------|
| user32 | 窗口管理、消息机制 |
| gdi32 | 图形绘制、字体渲染 |
| shell32 | 系统路径（SHGetFolderPath） |
| comctl32 | 通用控件 |
| comdlg32 | 颜色/字体选择对话框 |
| dwmapi | 窗口圆角、阴影效果 |
| winmm | 音频播放（PlaySound） |
| uxtheme | 主题相关渲染 |
| ole32 | COM 初始化 |
| uuid | UUID 支持 |

`-static-libgcc` 确保 GCC 运行时静态链接，生成的 exe 不依赖 `libgcc_s_seh-1.dll`。

`-mwindows` 指定生成 Windows GUI 程序（不显示控制台窗口）。

## 运行时依赖

编译生成的 exe 为独立可执行文件，**不需要额外运行时库**。

但首次运行时程序会在 `%USERPROFILE%` 下创建配置文件：

- `timer_config.ini` — 所有配置（时间、外观、音频、窗口位置等）

程序运行时会从自身 `fonts/` 和 `sound/` 目录加载资源。如果这些目录缺失，程序仍可运行（使用系统默认字体和系统提示音回退）。

## 分发部署

将以下文件一起打包即可分发：

```
timer_audio_optimized.exe
fonts/           （整个目录）
sound/           （整个目录）
```

用户双击 exe 即可运行，无需安装。
