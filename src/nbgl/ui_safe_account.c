#include "cmd_safe_account.h"
#include "safe_descriptor.h"
#include "signer_descriptor.h"
#include "shared_context.h"
#include "apdu_constants.h"
#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "ui_utils.h"
#include "app_mem_utils.h"

#define MAX_PAIRS 2
#define TAG_SIZE  11

#ifdef SCREEN_SIZE_WALLET
// Signers per alias list: fits the SDK modal page limit at two signers per page, and keeps the
// groups list of the 100 signers maximum short enough for the single page infos modal
#define SIGNERS_PER_GROUP 50
#define GROUP_STR_SIZE    12
#endif

// Structure to manage dynamic string arrays
typedef struct {
    char **pointers;  // Array of string pointers
    char *buffer;     // Buffer containing all string data
} string_array_t;

// Structure to manage tag-value g_pairs
typedef struct {
    string_array_t tags;    // Tags/labels
    string_array_t values;  // Corresponding values
} tag_value_collection_t;

static tag_value_collection_t *signersInfo = NULL;
static nbgl_contentValueExt_t *extensions = NULL;
static nbgl_contentTagValue_t *tagValuePairs = NULL;
static nbgl_contentTagValueList_t *tagValueList = NULL;
static char *s_safe_addr_str = NULL;
static char *s_threshold_str = NULL;
static uint8_t s_nb_groups = 0;
#ifdef SCREEN_SIZE_WALLET
static nbgl_contentInfoList_t *s_groups_infos = NULL;
static const char **s_groups_types = NULL;
static const char **s_groups_values = NULL;
static char *s_groups_str = NULL;
#endif

/**
 * @brief Cleanup the memory buffers and reset the Safe Account state.
 *
 * @param[in] sw Status word to send back to the host.
 */
static void _cleanup(uint16_t sw) {
    ui_pairs_cleanup();
    APP_MEM_FREE_AND_NULL((void **) &extensions);
    APP_MEM_FREE_AND_NULL((void **) &tagValuePairs);
    APP_MEM_FREE_AND_NULL((void **) &tagValueList);
    APP_MEM_FREE_AND_NULL((void **) &s_safe_addr_str);
    APP_MEM_FREE_AND_NULL((void **) &s_threshold_str);
#ifdef SCREEN_SIZE_WALLET
    APP_MEM_FREE_AND_NULL((void **) &s_groups_infos);
    APP_MEM_FREE_AND_NULL((void **) &s_groups_types);
    APP_MEM_FREE_AND_NULL((void **) &s_groups_values);
    APP_MEM_FREE_AND_NULL((void **) &s_groups_str);
#endif
    if (signersInfo != NULL) {
        APP_MEM_FREE_AND_NULL((void **) &signersInfo->tags.buffer);
        APP_MEM_FREE_AND_NULL((void **) &signersInfo->tags.pointers);
        APP_MEM_FREE_AND_NULL((void **) &signersInfo->values.buffer);
        APP_MEM_FREE_AND_NULL((void **) &signersInfo->values.pointers);
        APP_MEM_FREE_AND_NULL((void **) &signersInfo);
    }
    clear_safe_account();
    io_seproxyhal_send_status(sw, 0, true, false);
}

/**
 * @brief Prepare the memory buffers for displaying the Safe Account information.
 *
 * @return whether it was successful
 */
static bool _prepare_memory(void) {
    uint16_t totalSize = 0;
    // Init the g_pairs structure
    if (ui_pairs_init(MAX_PAIRS) == false) {
        return false;
    }

#ifdef SCREEN_SIZE_WALLET
    s_nb_groups = (SAFE_DESC->signers_count + SIGNERS_PER_GROUP - 1) / SIGNERS_PER_GROUP;
#else
    s_nb_groups = 1;
#endif
    // Threshold extension, followed by one extension per signers group when there are several
    if (APP_MEM_CALLOC((void **) &extensions,
                       sizeof(nbgl_contentValueExt_t) *
                           ((s_nb_groups == 1) ? 1 : (1 + s_nb_groups))) == false) {
        return false;
    }
    if (APP_MEM_CALLOC((void **) &tagValueList, sizeof(nbgl_contentTagValueList_t) * s_nb_groups) ==
        false) {
        return false;
    }
#ifdef SCREEN_SIZE_WALLET
    if (s_nb_groups > 1) {
        if ((APP_MEM_CALLOC((void **) &s_groups_infos, sizeof(*s_groups_infos)) == false) ||
            (APP_MEM_CALLOC((void **) &s_groups_types, sizeof(*s_groups_types) * s_nb_groups) ==
             false) ||
            (APP_MEM_CALLOC((void **) &s_groups_values, sizeof(*s_groups_values) * s_nb_groups) ==
             false) ||
            (APP_MEM_CALLOC((void **) &s_groups_str, GROUP_STR_SIZE * s_nb_groups) == false)) {
            return false;
        }
    }
#endif
    if (APP_MEM_CALLOC((void **) &tagValuePairs,
                       sizeof(nbgl_contentTagValue_t) * SAFE_DESC->signers_count) == false) {
        return false;
    }

    // Allocate pointers array for Signers tags/values
    if (APP_MEM_CALLOC((void **) &signersInfo, sizeof(tag_value_collection_t)) == false) {
        return false;
    }

    // Allocate pointers array for tags
    if (APP_MEM_CALLOC((void **) &signersInfo->tags.pointers,
                       sizeof(char *) * SAFE_DESC->signers_count) == false) {
        return false;
    }

    // Allocate buffer for actual tag strings
    totalSize = SAFE_DESC->signers_count * TAG_SIZE;
    if (APP_MEM_CALLOC((void **) &signersInfo->tags.buffer, totalSize) == false) {
        return false;
    }

    // Allocate pointers array for values
    if (APP_MEM_CALLOC((void **) &signersInfo->values.pointers,
                       sizeof(char *) * SAFE_DESC->signers_count) == false) {
        return false;
    }

    // Allocate buffer for actual value strings
    totalSize = SAFE_DESC->signers_count * ADDRESS_LENGTH_HEX_STR;
    if (APP_MEM_CALLOC((void **) &signersInfo->values.buffer, totalSize) == false) {
        return false;
    }
    if (APP_MEM_CALLOC((void **) &s_safe_addr_str, ADDRESS_LENGTH_HEX_STR) == false) {
        return false;
    }
    if (APP_MEM_CALLOC((void **) &s_threshold_str, 32) == false) {
        return false;
    }
    return true;
}

/**
 * @brief Prepare the strings for display in the Safe Account UI.
 *
 */
static void _prepare_strings(void) {
    uint16_t i = 0;
    char *ptr = NULL;
    // Prepare the strings for display
    array_bytes_string(s_safe_addr_str, ADDRESS_LENGTH_HEX_STR, SAFE_DESC->address, ADDRESS_LENGTH);
    snprintf(s_threshold_str, 32, "%d out of %d", SAFE_DESC->threshold, SAFE_DESC->signers_count);
#ifdef SCREEN_SIZE_WALLET
    if (s_nb_groups > 1) {
        for (i = 0; i < s_nb_groups; i++) {
            uint16_t first = (i * SIGNERS_PER_GROUP) + 1;
            uint16_t last = MIN(first + SIGNERS_PER_GROUP - 1, SAFE_DESC->signers_count);

            snprintf(s_groups_str + (i * GROUP_STR_SIZE), GROUP_STR_SIZE, "%d - %d", first, last);
        }
    }
#endif
    // Prepare the signers information
    // Populate tag strings
    ptr = signersInfo->tags.buffer;
    for (i = 0; i < SAFE_DESC->signers_count; i++) {
        snprintf(ptr, TAG_SIZE, "Signer %d", i + 1);
        signersInfo->tags.pointers[i] = ptr;
        ptr += TAG_SIZE;
    }

    // Populate value strings
    ptr = signersInfo->values.buffer;
    for (i = 0; i < SAFE_DESC->signers_count; i++) {
        array_bytes_string(ptr,
                           ADDRESS_LENGTH_HEX_STR,
                           SIGNER_DESC.data[i].address,
                           ADDRESS_LENGTH);
        signersInfo->values.pointers[i] = ptr;
        ptr += ADDRESS_LENGTH_HEX_STR;
    }
}

/**
 * @brief Set the tag value g_pairs for the Safe Account information.
 *
 */
static void setTagValuePairs(void) {
    uint8_t nbPairs = 0;
    uint16_t i = 0;
    uint16_t start = 0;

    g_pairs[nbPairs].item = "Your role";
    g_pairs[nbPairs].value = ROLE_STR(SAFE_DESC->role);

    nbPairs++;
    g_pairs[nbPairs].item = "Threshold";
    g_pairs[nbPairs].value = s_threshold_str;
    g_pairs[nbPairs].aliasValue = true;
    g_pairs[nbPairs].extension = &extensions[0];
    if (s_nb_groups == 1) {
        extensions[0].aliasType = TAG_VALUE_LIST_ALIAS;
        extensions[0].title = "";
        extensions[0].tagValuelist = &tagValueList[0];
    }
#ifdef SCREEN_SIZE_WALLET
    else {
        // The threshold opens the list of signers groups, each group opening its signers
        extensions[0].aliasType = INFO_LIST_ALIAS;
        extensions[0].infolist = s_groups_infos;
        s_groups_infos->infoTypes = s_groups_types;
        s_groups_infos->infoContents = s_groups_values;
        s_groups_infos->infoExtensions = &extensions[1];
        s_groups_infos->nbInfos = s_nb_groups;
        s_groups_infos->withExtensions = true;
        for (i = 0; i < s_nb_groups; i++) {
            s_groups_types[i] = "Signers";
            s_groups_values[i] = s_groups_str + (i * GROUP_STR_SIZE);
            // The infos list only shows the alias of an extension with a full value
            extensions[1 + i].fullValue = s_groups_values[i];
            extensions[1 + i].aliasType = TAG_VALUE_LIST_ALIAS;
            extensions[1 + i].title = "";
            extensions[1 + i].tagValuelist = &tagValueList[i];
        }
    }
#endif

    for (i = 0; i < SAFE_DESC->signers_count; i++) {
        tagValuePairs[i].item = signersInfo->tags.pointers[i];
        tagValuePairs[i].value = signersInfo->values.pointers[i];
    }
    for (i = 0; i < s_nb_groups; i++) {
        tagValueList[i].pairs = &tagValuePairs[start];
        tagValueList[i].nbPairs = SAFE_DESC->signers_count - start;
#ifdef SCREEN_SIZE_WALLET
        tagValueList[i].nbPairs = MIN(tagValueList[i].nbPairs, SIGNERS_PER_GROUP);
#endif
        start += tagValueList[i].nbPairs;
    }
}

/**
 * @brief Callback for the Safe Account review.
 *
 * @param[in] confirm Flag indicating whether the user confirmed the Safe Account
 */
static void review_cb(bool confirm) {
    if (confirm) {
        // User confirmed the Safe Account
        _cleanup(SWO_SUCCESS);
        nbgl_useCaseStatus("Safe Address validated", true, ui_idle);
        return;
    } else {
        // User rejected the Safe Account
        _cleanup(SWO_CONDITIONS_NOT_SATISFIED);
        nbgl_useCaseStatus("Safe Address rejected", false, ui_idle);
    }
}

/**
 * @brief Display the Safe Account information in a review screen.
 *
 */
void ui_display_safe_account(void) {
    // Memory allocations
    if (_prepare_memory() == false) {
        // Memory allocation failed in _prepare_memory
        _cleanup(SWO_INSUFFICIENT_MEMORY);
        return;
    }
    // Prepare strings for display
    _prepare_strings();

    // Setup data to display
    setTagValuePairs();

    nbgl_useCaseAddressReview(s_safe_addr_str,
                              g_pairsList,
                              &LARGE_MULTISIG_ICON,
                              "Verify Safe address",
                              NULL,
                              review_cb);
}
