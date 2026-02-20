/**
 * @file lang_de.h
 * @brief German (DE) language string table — binary size comparison test.
 *
 * Compiled only when HAVE_LANG_GERMAN is defined:
 *   make TARGET=flex ADD_GERMAN_LANG=1
 */

#pragma once

#ifdef HAVE_LANG_GERMAN

#include <stdint.h>

/** Number of translated strings in the German table (mirrors lang_fr count) */
#define LANG_DE_STRING_COUNT 78

/**
 * @brief Array of German UI strings (parallel to the English originals).
 */
extern const char *const LANG_DE_STRINGS[LANG_DE_STRING_COUNT];

#endif  // HAVE_LANG_GERMAN
