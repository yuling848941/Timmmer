/**
 * @file timer_font_manager.h
 * @brief Enhanced font management system for Timmmer
 * 
 * Based on Catime's font loading mechanism with optimizations for timer display
 */

#ifndef TIMER_FONT_MANAGER_H
#define TIMER_FONT_MANAGER_H

#include <windows.h>
#include <stdbool.h>

/**
 * @brief Font resource information structure
 */
typedef struct {
    const char* fileName;      /**< Font file name */
    const char* displayName;   /**< Display name for UI */
    const char* category;      /**< Font category (MIT/OFL/SIL) */
    bool isMonospace;          /**< Whether font is monospace */
} CatimeFontInfo;

/**
 * @brief Font loading result structure
 */
typedef struct {
    bool success;              /**< Loading success flag */
    char fontName[256];        /**< Loaded font family name */
    char errorMessage[512];    /**< Error message if failed */
} FontLoadResult;

/** @brief Available font resources */
extern CatimeFontInfo availableFonts[];

/** @brief Count of available fonts */
extern const int AVAILABLE_FONTS_COUNT;

/** @brief Current loaded font path */
extern char currentFontPath[MAX_PATH];

/** @brief Current font family name */
extern char currentFontName[256];

/** @brief Font loading state */
extern bool fontLoaded;

/**
 * @brief Initialize font management system
 * @return TRUE on success, FALSE on failure
 */
BOOL InitializeFontManager(void);

/**
 * @brief Cleanup font management system
 */
void CleanupFontManager(void);

/**
 * @brief Load font from file path
 * @param fontPath Full path to font file
 * @param result Pointer to FontLoadResult structure
 * @return TRUE on success, FALSE on failure
 */
BOOL LoadFontFromPath(const char* fontPath, FontLoadResult* result);

/**
 * @brief Load font by name from fonts directory
 * @param fontFileName Font file name (e.g., "DaddyTimeMono Essence.otf")
 * @param result Pointer to FontLoadResult structure
 * @return TRUE on success, FALSE on failure
 */
BOOL LoadFontByFileName(const char* fontFileName, FontLoadResult* result);

/**
 * @brief Get font family name from font file
 * @param fontPath Path to font file
 * @param fontName Buffer to store font family name
 * @param fontNameSize Size of fontName buffer
 * @return TRUE on success, FALSE on failure
 */
BOOL GetFontFamilyName(const char* fontPath, char* fontName, size_t fontNameSize);

/**
 * @brief Find font file in fonts directory
 * @param fileName Font file name to search
 * @param foundPath Buffer to store found path
 * @param foundPathSize Size of foundPath buffer
 * @return TRUE if found, FALSE otherwise
 */
BOOL FindFontFile(const char* fileName, char* foundPath, size_t foundPathSize);

/**
 * @brief List all available fonts
 * @param fontList Output array for font information
 * @param maxFonts Maximum number of fonts to return
 * @return Number of fonts found
 */
int ListAvailableFonts(CatimeFontInfo* fontList, int maxFonts);

/**
 * @brief Get current font information
 * @param fontInfo Pointer to CatimeFontInfo structure to fill
 * @return TRUE if font is loaded, FALSE otherwise
 */
BOOL GetCurrentFontInfo(CatimeFontInfo* fontInfo);

/**
 * @brief Unload current font
 * @return TRUE on success, FALSE on failure
 */
BOOL UnloadCurrentFont(void);

/**
 * @brief Check if font is suitable for timer display
 * @param fontPath Path to font file
 * @return TRUE if suitable, FALSE otherwise
 */
BOOL IsFontSuitableForTimer(const char* fontPath);

/**
 * @brief Get recommended fonts for timer display
 * @param recommendedFonts Output array for recommended fonts
 * @param maxFonts Maximum number of fonts to return
 * @return Number of recommended fonts
 */
int GetRecommendedTimerFonts(CatimeFontInfo* recommendedFonts, int maxFonts);

#endif // TIMER_FONT_MANAGER_H