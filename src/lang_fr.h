/**
 * @file lang_fr.h
 * @brief French (FR) language string table for binary size comparison testing.
 *
 * This file is only included when HAVE_LANG_FRENCH is defined.
 * It declares a table of French translations for all UI strings used in the app.
 *
 * NOTE: This is a size-impact test file. The app UI still uses the hardcoded
 * English strings — this module simply forces the French string data to be
 * present in the binary so we can measure the ROM cost of multi-language support.
 */

#pragma once

#ifdef HAVE_LANG_FRENCH

#include <stdint.h>

/** Number of translated strings in the French table */
#define LANG_FR_STRING_COUNT 78

/**
 * @brief Array of French UI strings (parallel to the English originals).
 *
 * The table is declared as a const array stored in flash (ROM), so its size
 * directly reflects the ROM overhead of embedding a second language.
 */
extern const char *const LANG_FR_STRINGS[LANG_FR_STRING_COUNT];

#endif  // HAVE_LANG_FRENCH
