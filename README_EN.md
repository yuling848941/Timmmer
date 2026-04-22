# 🕐 Timer - Modern Desktop Timer

English | [简体中文](README.md)


A feature-rich, visually appealing Windows desktop timer application with support for countdown, stopwatch, multi-language interface, customizable appearance, and audio alerts.

![Timer Screenshot](https://img.shields.io/badge/Platform-Windows-blue)
![Language](https://img.shields.io/badge/Language-C-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

## ✨ Key Features

### 🎯 Core Functionality
- **High Precision**: Supports hours, minutes, seconds, and milliseconds precision.
- **Countdown Mode**: Automatic countdown after setting the time.
- **Stopwatch Mode**: Continues counting up after the timer hits zero.
- **Preset Times**: Quickly select common time intervals.
- **System Tray**: Minimizes to the system tray with a real-time preview of time and status on the icon.
- **Window Scaling**: Supports real-time resizing using the mouse wheel.
- **Fluent 2 Style**: Modern visual style with rounded corners and drop shadows.

### 🌍 Multi-language Support
- **Chinese Interface**: Full Chinese localization.
- **English Interface**: Internationalized English interface.
- **Instant Switch**: Real-time language switching without restarting the app.

### 🎨 Appearance Customization
- **Font Selection**: Supports system fonts and custom embedded fonts.
- **Color Themes**: Freely choose text and background colors.
- **Transparent Background**: Supports a fully transparent background mode.
- **Modern UI**: Rounded windows and shadow effects for a premium feel.
- **Real-time Preview**: See changes instantly while configuring settings.

### 🔊 Audio Alerts
- **Bilingual Voice**: Chinese and English voice prompts.
- **Custom Audio**: Support for importing your own audio files.
- **Smart Fallback**: Automatically uses alternative sounds if audio files are missing.
- **Audio Test**: Preview audio effects directly in the settings dialog.

## 🚀 Quick Start

### System Requirements
- **OS**: Windows 10/11 (Recommended) or Windows 7/8.
- **Memory**: Minimum 50MB free RAM.
- **Storage**: Approx. 10MB disk space.

### Installation

#### Method 1: Pre-built Binary (Recommended)
1. Download `Timmmer.exe`.
2. Double-click to run.
3. Configuration files will be created automatically on the first run.

#### Method 2: Build from Source
```bash
# Clone the repository
git clone https://github.com/your-repo/timer.git
cd timer

# Use the included build script
build.bat

# Run the program
./build/Timmmer.exe
```

## 📖 User Guide

### Basic Operations

#### 1. Setting the Time
- **Context Menu**: Right-click on the timer → "Set Time".
- **Shortcuts**: Double-click the timer window.
- **Presets**: Right-click menu → "Preset Times" → Select a common time.

#### 2. Start/Control Timing
- **Start/Pause**: Right-click menu → "Start/Pause".
- **Reset**: Right-click menu → "Reset".

#### 3. Quick Interactions
- **Cycle Presets**: **Double-click** the main window to cycle through your preset times.
- **Scale Window**: Use the **Mouse Wheel** on the main window to zoom in or out proportionally.
- **Move Window**: Click and drag anywhere on the main window to move it.

#### 4. Customize Appearance
1. Right-click menu → "Appearance Settings".
2. Select font, colors, and opacity.
3. Preview effects in real-time.
4. Click "OK" to save.

#### 5. Audio Settings
1. Right-click menu → "Audio Settings".
2. Enable audio alerts.
3. Choose the voice language or a custom file.
4. Click "Test Audio" to preview.

### Advanced Features

#### Preset Management
- **Add Preset**: Enter minutes in the preset dialog and click "Add".
- **Delete Preset**: Select a preset and click "Delete".
- **Modify Preset**: Select a preset, enter a new value, and click "Update".

#### Time Display Format
- **Custom Display**: Choose whether to show hours, minutes, seconds, or milliseconds.
- **Flexible Combinations**: Combine different time units according to your needs.

#### Transparent Background Mode
- **Full Transparency**: Enable this mode to show only the time text.
- **Rendering Optimization**: Antialiasing is automatically adjusted in transparent mode to ensure crisp text.

## 🛠️ Technical Architecture

### Core Modules
```
Timer/
├── src/                        # Source code
│   ├── main_audio_optimized.c  # Entry point
│   ├── core/                   # Core timing logic & configuration
│   ├── ui/                     # UI rendering & window management
│   ├── dialogs/                # Dialog modules (modularized)
│   ├── audio/                  # Audio processing logic
│   ├── fonts/                  # Font management & rendering
│   └── menu/                   # Menu logic
├── res/                        # Resources (icons, audio, rc files)
├── build/                      # Build output
├── docs/                       # Documentation
├── scripts/                    # Helper scripts
└── build.bat                   # One-click build script
```

### Technical Highlights
- **Pure C**: Built using standard C and the Windows API (Win32).
- **Modular Design**: Clean separation of functional modules.
- **Resource Embedding**: Fonts and audio resources are embedded directly into the executable.
- **Persistent Config**: User settings are automatically saved.
- **Memory Optimized**: Efficient resource management and caching mechanisms.

## 🔧 Configuration

The program creates a unified configuration file in your user directory:

```
%USERPROFILE%/
└── timer_config.ini     # Unified config for features, appearance, presets, and window state
```

#### Configuration Example (timer_config.ini)

```ini
[Format]
ShowHours=1                    # Show hours (0/1)
ShowMinutes=1                  # Show minutes (0/1)
ShowSeconds=1                  # Show seconds (0/1)
ShowMilliseconds=0             # Show milliseconds (0/1)

[General]
Language=0                     # Interface language (0:CN, 1:EN)

[Appearance]
FontColor=16777215             # Font color (COLORREF)
BackgroundColor=0               # Background color (COLORREF)
WindowOpacity=255              # Window opacity (0-255)
FontName=Arial                 # Selected font name
TransparentBackground=0        # Transparent mode (0/1)

[Audio]
EnableAudioAlert=1             # Enable audio (0/1)
UseChineseAudio=1              # Use Chinese voice (0/1)
UseCustomAudio=0               # Use custom audio (0/1)
CustomAudioPath=               # Custom audio path

[Presets]
Count=3                        # Number of presets
Preset0=10                     # Preset 1 (minutes)
Preset1=15                     # Preset 2 (minutes)
Preset2=20                     # Preset 3 (minutes)

[Timer]
EnableOvertimeCount=0          # Enable count-up after timeout (0/1)
LastUsedTime=600               # Last used time (seconds)

[Window]
X=100                          # Window X position
Y=100                          # Window Y position
Width=300                      # Window width
Height=150                     # Window height
```

## 🎨 Theme Examples

### Classic Dark
```ini
FontColor=255,255,255
BackgroundColor=0,0,0
Opacity=200
TransparentBackground=0
```

### Pure Transparent
```ini
FontColor=255,255,255
TransparentBackground=1
```

### Eye-Care Green
```ini
FontColor=0,255,0
BackgroundColor=0,0,0
Opacity=180
```

## 🔍 Troubleshooting

### Common Issues

#### Q: The program won't start.
**A**: Ensure your system meets the requirements and you have enough free memory.

#### Q: Audio is not playing.
**A**:
1. Check if audio is enabled in settings.
2. Verify your system audio output is working.
3. Try switching between language options.

#### Q: Font display issues.
**A**:
1. Try resetting font settings.
2. Check if system fonts are corrupted.
3. Revert to default font settings.

#### Q: Settings are lost.
**A**:
1. Check user directory permissions.
2. Manually delete the config file to regenerate it.
3. Use the "Reset to Defaults" feature.

### Performance Tips
1. **Reduce Redraws**: Disabling milliseconds improves performance.
2. **Font Caching**: Avoid changing fonts too frequently.
3. **Transparency**: Transparent mode may impact performance on older hardware.

## 🤝 Contributing

### Dev Environment Setup
```bash
# Install MinGW-w64
# Clone project
git clone https://github.com/your-repo/timer.git

# Build
make build

# Run tests
make test
```

### Code Style
- Use CamelCase.
- Function names start with verbs.
- Descriptive variable names.
- Include appropriate comments.

### Commit Messages
- feat: New feature
- fix: Bug fix
- docs: Documentation changes
- style: Formatting
- refactor: Code restructuring
- test: Testing related

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments
- Windows API Documentation & Community.
- MinGW Compiler Project.
- All contributors and users for their feedback.

## 📞 Contact
- **Issues**: [GitHub Issues](https://github.com/your-repo/timer/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-repo/timer/discussions)
- **Email**: your-email@example.com

---

**Enjoy using Timer!** 🎉

If this project helps you, please give us a ⭐ Star!
