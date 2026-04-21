/**
 * @file timer_font_manager.c
 * @brief Enhanced font management system implementation
 * 
 * Based on Catime's font loading mechanism with optimizations for timer display
 */

#include "timer_font_manager.h"
#include "timer_font_resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>

/** @brief Font name table structures for TTF/OTF font parsing */
#pragma pack(push, 1)
typedef struct {
    WORD platformID;
    WORD encodingID;
    WORD languageID;
    WORD nameID;
    WORD length;
    WORD offset;
} NameRecord;

typedef struct {
    WORD format;
    WORD count;
    WORD stringOffset;
} NameTableHeader;

typedef struct {
    DWORD tag;
    DWORD checksum;
    DWORD offset;
    DWORD length;
} TableRecord;

typedef struct {
    DWORD sfntVersion;
    WORD numTables;
    WORD searchRange;
    WORD entrySelector;
    WORD rangeShift;
} FontDirectoryHeader;
#pragma pack(pop)

/** @brief Available font resources from Catime */
CatimeFontInfo availableFonts[] = {
    {"DaddyTimeMono Essence.otf", "DaddyTime Mono", "SIL", true},
    {"DepartureMono Essence.otf", "Departure Mono", "SIL", true},
    {"Rec Mono Casual Essence.ttf", "Rec Mono Casual", "SIL", true},
    {"Terminess Nerd Font Essence.ttf", "Terminess Nerd Font", "SIL", true},
    {"ProFontWindows Essence.ttf", "ProFont Windows", "MIT", true},
    {"Jacquard 12 Essence.ttf", "Jacquard 12", "OFL", false},
    {"Jacquarda Bastarda 9 Essence.ttf", "Jacquarda Bastarda 9", "OFL", false},
    {"Pixelify Sans Essence.ttf", "Pixelify Sans", "OFL", false},
    {"Rubik Burned Essence.ttf", "Rubik Burned", "OFL", false},
    {"Rubik Glitch Essence.ttf", "Rubik Glitch", "OFL", false},
    {"Rubik Marker Hatch Essence.ttf", "Rubik Marker Hatch", "OFL", false},
    {"Rubik Puddles Essence.ttf", "Rubik Puddles", "OFL", false},
    {"Wallpoet Essence.ttf", "Wallpoet", "OFL", false}
};

/** @brief Count of available fonts */
const int AVAILABLE_FONTS_COUNT = sizeof(availableFonts) / sizeof(CatimeFontInfo);

/** @brief Current loaded font path */
char currentFontPath[MAX_PATH] = "";

/** @brief Current font family name */
char currentFontName[256] = "";

/** @brief Font loading state */
bool fontLoaded = false;

/** @brief Currently loaded font resource handle */
static HANDLE currentFontHandle = NULL;

/**
 * @brief Convert big-endian WORD to little-endian
 */
static WORD SwapWORD(WORD value) {
    return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
}

/**
 * @brief Convert big-endian DWORD to little-endian
 */
static DWORD SwapDWORD(DWORD value) {
    return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) | 
           ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

BOOL InitializeFontManager(void) {
    // Initialize font management system
    memset(currentFontPath, 0, sizeof(currentFontPath));
    memset(currentFontName, 0, sizeof(currentFontName));
    fontLoaded = false;
    currentFontHandle = NULL;
    
    return TRUE;
}

void CleanupFontManager(void) {
    UnloadCurrentFont();
}

BOOL GetFontFamilyName(const char* fontPath, char* fontName, size_t fontNameSize) {
    if (!fontPath || !fontName || fontNameSize == 0) return FALSE;
    
    // Convert path to wide character
    wchar_t wFontPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fontPath, -1, wFontPath, MAX_PATH);
    
    // Open font file
    HANDLE hFile = CreateFileW(wFontPath, GENERIC_READ, FILE_SHARE_READ, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    // Read font directory header
    FontDirectoryHeader fontHeader;
    DWORD bytesRead;
    if (!ReadFile(hFile, &fontHeader, sizeof(FontDirectoryHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(FontDirectoryHeader)) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    // Find name table
    DWORD nameTableOffset = 0;
    DWORD nameTableLength = 0;
    
    for (WORD i = 0; i < SwapWORD(fontHeader.numTables); i++) {
        TableRecord tableRecord;
        if (!ReadFile(hFile, &tableRecord, sizeof(TableRecord), &bytesRead, NULL) ||
            bytesRead != sizeof(TableRecord)) {
            CloseHandle(hFile);
            return FALSE;
        }
        
        if (SwapDWORD(tableRecord.tag) == 0x656D616E) { // 'name' in big-endian
            nameTableOffset = SwapDWORD(tableRecord.offset);
            nameTableLength = SwapDWORD(tableRecord.length);
            break;
        }
    }
    
    if (nameTableOffset == 0) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    // Read name table header
    SetFilePointer(hFile, nameTableOffset, NULL, FILE_BEGIN);
    NameTableHeader nameHeader;
    if (!ReadFile(hFile, &nameHeader, sizeof(NameTableHeader), &bytesRead, NULL) ||
        bytesRead != sizeof(NameTableHeader)) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    // Search for font family name (nameID = 1)
    WORD recordCount = SwapWORD(nameHeader.count);
    WORD stringOffset = SwapWORD(nameHeader.stringOffset);
    
    for (WORD i = 0; i < recordCount; i++) {
        NameRecord nameRecord;
        if (!ReadFile(hFile, &nameRecord, sizeof(NameRecord), &bytesRead, NULL) ||
            bytesRead != sizeof(NameRecord)) {
            continue;
        }
        
        if (SwapWORD(nameRecord.nameID) == 1) { // Font family name
            WORD platformID = SwapWORD(nameRecord.platformID);
            WORD encodingID = SwapWORD(nameRecord.encodingID);
            WORD length = SwapWORD(nameRecord.length);
            WORD offset = SwapWORD(nameRecord.offset);
            
            // Prefer Unicode platform (3) or Microsoft platform (3)
            if (platformID == 3 || (platformID == 1 && encodingID == 0)) {
                DWORD stringPos = nameTableOffset + stringOffset + offset;
                SetFilePointer(hFile, stringPos, NULL, FILE_BEGIN);
                
                if (platformID == 3) { // Unicode
                    wchar_t* unicodeBuffer = malloc(length + 2);
                    if (unicodeBuffer) {
                        memset(unicodeBuffer, 0, length + 2);
                        ReadFile(hFile, unicodeBuffer, length, &bytesRead, NULL);
                        
                        // Convert from big-endian Unicode
                        for (int j = 0; j < length / 2; j++) {
                            unicodeBuffer[j] = SwapWORD(unicodeBuffer[j]);
                        }
                        
                        WideCharToMultiByte(CP_UTF8, 0, unicodeBuffer, -1, 
                                          fontName, (int)fontNameSize, NULL, NULL);
                        free(unicodeBuffer);
                        CloseHandle(hFile);
                        return TRUE;
                    }
                } else { // ASCII
                    char* asciiBuffer = malloc(length + 1);
                    if (asciiBuffer) {
                        memset(asciiBuffer, 0, length + 1);
                        ReadFile(hFile, asciiBuffer, length, &bytesRead, NULL);
                        strncpy(fontName, asciiBuffer, fontNameSize - 1);
                        fontName[fontNameSize - 1] = '\0';
                        free(asciiBuffer);
                        CloseHandle(hFile);
                        return TRUE;
                    }
                }
            }
        }
    }
    
    CloseHandle(hFile);
    return FALSE;
}

BOOL LoadFontFromPath(const char* fontPath, FontLoadResult* result) {
    if (!fontPath || !result) return FALSE;
    
    // Initialize result
    result->success = false;
    memset(result->fontName, 0, sizeof(result->fontName));
    memset(result->errorMessage, 0, sizeof(result->errorMessage));
    
    // Unload current font first
    UnloadCurrentFont();
    
    // Convert to wide character path
    wchar_t wFontPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fontPath, -1, wFontPath, MAX_PATH);
    
    // Load font resource
    int fontsAdded = AddFontResourceExW(wFontPath, FR_PRIVATE, 0);
    if (fontsAdded == 0) {
        strcpy(result->errorMessage, "Failed to load font resource");
        return FALSE;
    }
    
    // Get font family name
    if (!GetFontFamilyName(fontPath, result->fontName, sizeof(result->fontName))) {
        // Fallback: use filename without extension
        const char* fileName = strrchr(fontPath, '\\');
        if (!fileName) fileName = strrchr(fontPath, '/');
        if (!fileName) fileName = fontPath;
        else fileName++;
        
        strncpy(result->fontName, fileName, sizeof(result->fontName) - 1);
        
        // Remove extension
        char* dot = strrchr(result->fontName, '.');
        if (dot) *dot = '\0';
    }
    
    // Update global state
    strncpy(currentFontPath, fontPath, sizeof(currentFontPath) - 1);
    strncpy(currentFontName, result->fontName, sizeof(currentFontName) - 1);
    fontLoaded = true;
    
    result->success = true;
    return TRUE;
}

BOOL FindFontFile(const char* fileName, char* foundPath, size_t foundPathSize) {
    if (!fileName || !foundPath || foundPathSize == 0) return FALSE;
    
    // Get current directory
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    
    // Search in fonts subdirectories
    const char* searchDirs[] = {
        "fonts\\SIL",
        "fonts\\MIT", 
        "fonts\\OFL",
        "fonts"
    };
    
    for (int i = 0; i < 4; i++) {
        char testPath[MAX_PATH];
        snprintf(testPath, MAX_PATH, "%s\\%s\\%s", currentDir, searchDirs[i], fileName);
        
        if (GetFileAttributesA(testPath) != INVALID_FILE_ATTRIBUTES) {
            strncpy(foundPath, testPath, foundPathSize - 1);
            foundPath[foundPathSize - 1] = '\0';
            return TRUE;
        }
    }
    
    return FALSE;
}

BOOL LoadFontByFileName(const char* fontFileName, FontLoadResult* result) {
    if (!fontFileName || !result) return FALSE;
    
    char fontPath[MAX_PATH];
    if (!FindFontFile(fontFileName, fontPath, sizeof(fontPath))) {
        result->success = false;
        snprintf(result->errorMessage, sizeof(result->errorMessage), 
                "Font file not found: %s", fontFileName);
        return FALSE;
    }
    
    return LoadFontFromPath(fontPath, result);
}

int ListAvailableFonts(CatimeFontInfo* fontList, int maxFonts) {
    if (!fontList || maxFonts <= 0) return 0;
    
    int count = 0;
    for (int i = 0; i < AVAILABLE_FONTS_COUNT && count < maxFonts; i++) {
        char fontPath[MAX_PATH];
        if (FindFontFile(availableFonts[i].fileName, fontPath, sizeof(fontPath))) {
            fontList[count] = availableFonts[i];
            count++;
        }
    }
    
    return count;
}

BOOL GetCurrentFontInfo(CatimeFontInfo* fontInfo) {
    if (!fontInfo || !fontLoaded) return FALSE;
    
    // Find current font in available fonts list
    for (int i = 0; i < AVAILABLE_FONTS_COUNT; i++) {
        char fontPath[MAX_PATH];
        if (FindFontFile(availableFonts[i].fileName, fontPath, sizeof(fontPath))) {
            if (strcmp(fontPath, currentFontPath) == 0) {
                *fontInfo = availableFonts[i];
                return TRUE;
            }
        }
    }
    
    // If not found in list, create basic info
    fontInfo->fileName = "";
    fontInfo->displayName = currentFontName;
    fontInfo->category = "Unknown";
    fontInfo->isMonospace = false;
    
    return TRUE;
}

BOOL UnloadCurrentFont(void) {
    if (!fontLoaded || strlen(currentFontPath) == 0) return TRUE;
    
    wchar_t wFontPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, currentFontPath, -1, wFontPath, MAX_PATH);
    
    BOOL result = RemoveFontResourceExW(wFontPath, FR_PRIVATE, 0);
    
    // Reset state
    memset(currentFontPath, 0, sizeof(currentFontPath));
    memset(currentFontName, 0, sizeof(currentFontName));
    fontLoaded = false;
    currentFontHandle = NULL;
    
    return result;
}

BOOL IsFontSuitableForTimer(const char* fontPath) {
    if (!fontPath) return FALSE;
    
    // Check if font is in our available fonts list
    const char* fileName = strrchr(fontPath, '\\');
    if (!fileName) fileName = strrchr(fontPath, '/');
    if (!fileName) fileName = fontPath;
    else fileName++;
    
    for (int i = 0; i < AVAILABLE_FONTS_COUNT; i++) {
        if (strcmp(availableFonts[i].fileName, fileName) == 0) {
            // Prefer monospace fonts for timer display
            return availableFonts[i].isMonospace;
        }
    }
    
    return FALSE;
}

int GetRecommendedTimerFonts(CatimeFontInfo* recommendedFonts, int maxFonts) {
    if (!recommendedFonts || maxFonts <= 0) return 0;
    
    int count = 0;
    
    // Prioritize monospace fonts
    for (int i = 0; i < AVAILABLE_FONTS_COUNT && count < maxFonts; i++) {
        if (availableFonts[i].isMonospace) {
            char fontPath[MAX_PATH];
            if (FindFontFile(availableFonts[i].fileName, fontPath, sizeof(fontPath))) {
                recommendedFonts[count] = availableFonts[i];
                count++;
            }
        }
    }
    
    // Add non-monospace fonts if space available
    for (int i = 0; i < AVAILABLE_FONTS_COUNT && count < maxFonts; i++) {
        if (!availableFonts[i].isMonospace) {
            char fontPath[MAX_PATH];
            if (FindFontFile(availableFonts[i].fileName, fontPath, sizeof(fontPath))) {
                recommendedFonts[count] = availableFonts[i];
                count++;
            }
        }
    }
    
    return count;
}

// 获取混合字体列表（系统字体 + 内置字体）
int GetMixedFontList(wchar_t fontNames[][256], int maxFonts) {
    if (!fontNames || maxFonts <= 0) return 0;
    
    // 获取字体资源管理器
    FontResourceManager* manager = GetFontResourceManager();
    if (!manager) return 0;
    
    int count = 0;
    
    // 遍历混合字体配置
    for (int i = 0; i < manager->mixedFontCount && count < maxFonts; i++) {
        ExtendedFontInfo* fontInfo = &manager->mixedFonts[i];
        
        // 检查字体是否可用
        if (fontInfo->type == FONT_RESOURCE_SYSTEM) {
            // 对于系统字体，检查是否已安装
            if (!fontInfo->resource.system.isInstalled) {
                // 尝试检测系统字体是否存在
                HDC hdc = GetDC(NULL);
                if (hdc) {
                    LOGFONTW lf = {0};
                    wcscpy_s(lf.lfFaceName, LF_FACESIZE, fontInfo->displayName);
                    lf.lfCharSet = DEFAULT_CHARSET;
                    
                    HFONT hFont = CreateFontIndirectW(&lf);
                    if (hFont) {
                        HFONT hOldFont = SelectObject(hdc, hFont);
                        wchar_t actualName[LF_FACESIZE];
                        GetTextFaceW(hdc, LF_FACESIZE, actualName);
                        
                        // 检查是否成功创建了指定字体
                        if (wcscmp(actualName, fontInfo->displayName) == 0) {
                            fontInfo->resource.system.isInstalled = TRUE;
                        }
                        
                        SelectObject(hdc, hOldFont);
                        DeleteObject(hFont);
                    }
                    ReleaseDC(NULL, hdc);
                }
            }
            
            // 如果系统字体可用，添加到列表
            if (fontInfo->resource.system.isInstalled) {
                wcscpy_s(fontNames[count], 256, fontInfo->displayName);
                count++;
            }
        } else if (fontInfo->type == FONT_RESOURCE_EMBEDDED) {
            // 内置字体始终可用
            wcscpy_s(fontNames[count], 256, fontInfo->displayName);
            count++;
        }
    }
    
    return count;
}