@echo off
echo Compiling timer program...
echo.

if not exist build mkdir build

REM Step 1: Compile resource file
echo Compiling resources...
if exist res\timer_resource.rc (
    windres -I res res\timer_resource.rc -o build\timer_resource.o
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to compile resources!
        pause
        exit /b 1
    )
) else (
    echo Warning: res\timer_resource.rc not found, skipping resource compilation.
)

echo.

REM Step 2: Compile timer program
gcc -o build\Timmmer.exe ^
    -Isrc\core -Isrc\ui -Isrc\dialogs -Isrc\audio -Isrc\fonts -Isrc\menu ^
    src\main_audio_optimized.c ^
    src\core\timer_core.c ^
    src\ui\timer_ui.c ^
    src\ui\timer_window.c ^
    src\ui\timer_render_utils.c ^
    src\dialogs\timer_dialog_common.c ^
    src\dialogs\timer_dialog_format.c ^
    src\dialogs\timer_dialog_settime.c ^
    src\dialogs\timer_dialog_preset.c ^
    src\dialogs\timer_dialog_about.c ^
    src\dialogs\timer_dialog_audio.c ^
    src\dialogs\timer_dialog_appearance.c ^
    src\dialogs\timer_dialog_integrated.c ^
    src\core\timer_config.c ^
    src\audio\timer_audio.c ^
    src\audio\timer_embedded_audio.c ^
    src\fonts\timer_font_manager.c ^
    src\fonts\timer_embedded_fonts.c ^
    src\fonts\timer_fonts.c ^
    src\fonts\timer_font_resources_optimized.c ^
    src\menu\ios_menu.c ^
    build\timer_resource.o ^
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
    -lmsimg32 ^
    -static-libgcc ^
    -municode -lmsimg32 -mwindows

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilation successful! Generated file: build\Timmmer.exe
    echo.
) else (
    echo.
    echo Compilation failed! Please check error messages.
    echo.
    pause
    exit /b 1
)

echo Compilation completed.
pause