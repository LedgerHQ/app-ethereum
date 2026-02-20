/**
 * @file lang_es.h
 * @brief Spanish (ES) language string table — binary size comparison test.
 *
 * Compiled only when HAVE_LANG_SPANISH is defined:
 *   make TARGET=flex ADD_SPANISH_LANG=1
 */

#pragma once

#ifdef HAVE_LANG_SPANISH

#include <stdint.h>

/** Number of translated strings in the Spanish table (mirrors lang_fr count) */
#define LANG_ES_STRING_COUNT 78

/**
 * @brief Array of Spanish UI strings (parallel to the English originals).
 */
extern const char *const LANG_ES_STRINGS[LANG_ES_STRING_COUNT];

#endif  // HAVE_LANG_SPANISH
