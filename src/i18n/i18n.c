#include "i18n.h"
#include "os.h"
#include "globals.h"

// Import translation tables
extern const char *const g_strings_en[STR_COUNT];
extern const char *const g_strings_fr[STR_COUNT];

// Array of all language tables
static const char *const *const g_language_tables[LANG_COUNT] = {
    [LANG_ENGLISH] = g_strings_en,
    [LANG_FRENCH] = g_strings_fr,
};

language_e i18n_get_language(void) {
    // Read from NVM storage
    language_e lang = N_storage.language;

    // Validate and return default if invalid
    if (lang >= LANG_COUNT) {
        return LANG_ENGLISH;
    }
    return lang;
}

void i18n_set_language(language_e lang) {
    if (lang < LANG_COUNT) {
        nvm_write((void *) &N_storage.language, (void *) &lang, sizeof(lang));
    }
}

const char *i18n_get_string(string_id_e id) {
    language_e lang = i18n_get_language();

    // Bounds checking
    if (id >= STR_COUNT || lang >= LANG_COUNT) {
        return "???";
    }

    const char *str = g_language_tables[lang][id];
    return (str != NULL) ? str : "???";
}
