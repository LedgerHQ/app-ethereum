#ifdef HAVE_SAFE_ACCOUNT
#include "cmd_safe_account.h"
#include "safe_descriptor.h"
#include "signer_descriptor.h"
#include "apdu_constants.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "ui_utils.h"
#include "mem_utils.h"

#define MAX_PAIRS          2
#define TAG_MAX_LEN        10
#define ADDRESS_LENGTH_STR ((ADDRESS_LENGTH * 2) + 3)  // "0x" + 2 * ADDRESS_LENGTH + '\0'

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

/**
 * @brief Cleanup the memory buffers and reset the Safe Account state.
 *
 * @param[in] sw Status word to send back to the host.
 */
static void _cleanup(uint16_t sw) {
    ui_pairs_cleanup();
    mem_buffer_cleanup((void **) &extensions);
    mem_buffer_cleanup((void **) &tagValuePairs);
    mem_buffer_cleanup((void **) &tagValueList);
    if (signersInfo != NULL) {
        mem_buffer_cleanup((void **) &signersInfo->tags.buffer);
        mem_buffer_cleanup((void **) &signersInfo->tags.pointers);
        mem_buffer_cleanup((void **) &signersInfo->values.buffer);
        mem_buffer_cleanup((void **) &signersInfo->values.pointers);
        mem_buffer_cleanup((void **) &signersInfo);
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

    // Allocate memory for extensions to display Safe Address QRCode and Signers Addressss
    if (mem_buffer_allocate((void **) &extensions, sizeof(nbgl_contentValueExt_t) * 2) == false) {
        return false;
    }
    if (mem_buffer_allocate((void **) &tagValueList, sizeof(nbgl_contentTagValueList_t)) == false) {
        return false;
    }
    if (mem_buffer_allocate((void **) &tagValuePairs,
                            sizeof(nbgl_contentTagValue_t) * SAFE_DESC->signers_count) == false) {
        return false;
    }

    // Allocate pointers array for Signers tags/values
    if (mem_buffer_allocate((void **) &signersInfo, sizeof(tag_value_collection_t)) == false) {
        return false;
    }

    // Allocate pointers array for tags
    if (mem_buffer_allocate((void **) &signersInfo->tags.pointers,
                            sizeof(char *) * SAFE_DESC->signers_count) == false) {
        return false;
    }

    // Allocate buffer for actual tag strings
    totalSize = SAFE_DESC->signers_count * (TAG_MAX_LEN + 1);
    if (mem_buffer_allocate((void **) &signersInfo->tags.buffer, totalSize) == false) {
        return false;
    }

    // Allocate pointers array for values
    if (mem_buffer_allocate((void **) &signersInfo->values.pointers,
                            sizeof(char *) * SAFE_DESC->signers_count) == false) {
        return false;
    }

    // Allocate buffer for actual value strings
    totalSize = SAFE_DESC->signers_count * (ADDRESS_LENGTH_STR + 1);
    if (mem_buffer_allocate((void **) &signersInfo->values.buffer, totalSize) == false) {
        return false;
    }
    explicit_bzero((void *) &strings, sizeof(strings_t));
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
    array_bytes_string(strings.tmp.tmp, ADDRESS_LENGTH_STR, SAFE_DESC->address, ADDRESS_LENGTH);
    snprintf(strings.tmp.tmp + ADDRESS_LENGTH_STR,
             sizeof(strings.tmp.tmp) - ADDRESS_LENGTH_STR,
             "%d out of %d",
             SAFE_DESC->threshold,
             SAFE_DESC->signers_count);
    // Prepare the signers information
    // Populate tag strings
    ptr = signersInfo->tags.buffer;
    for (i = 0; i < SAFE_DESC->signers_count; i++) {
        snprintf(ptr, TAG_MAX_LEN, "Signer %d", i + 1);
        signersInfo->tags.pointers[i] = ptr;
        ptr += TAG_MAX_LEN + 1;
    }

    // Populate value strings
    ptr = signersInfo->values.buffer;
    for (i = 0; i < SAFE_DESC->signers_count; i++) {
        array_bytes_string(ptr, ADDRESS_LENGTH_STR, SIGNER_DESC.data[i].address, ADDRESS_LENGTH);
        signersInfo->values.pointers[i] = ptr;
        ptr += ADDRESS_LENGTH_STR + 1;
    }
}

/**
 * @brief Set the tag value g_pairs for the Safe Account information.
 *
 */
static void setTagValuePairs(void) {
    uint8_t nbPairs = 0;
    uint16_t i = 0;

    g_pairs[nbPairs].item = "Your role";
    g_pairs[nbPairs].value = ROLE_STR(SAFE_DESC->role);

    nbPairs++;
    g_pairs[nbPairs].item = "Threshold";
    g_pairs[nbPairs].value = strings.tmp.tmp + ADDRESS_LENGTH_STR;
    g_pairs[nbPairs].aliasValue = 1;
    g_pairs[nbPairs].extension = &extensions[1];
    extensions[1].aliasType = TAG_VALUE_LIST_ALIAS;
    extensions[1].title = "";
    extensions[1].tagValuelist = tagValueList;
    tagValueList->pairs = tagValuePairs;
    tagValueList->nbPairs = SAFE_DESC->signers_count;
    for (i = 0; i < SAFE_DESC->signers_count; i++) {
        tagValuePairs[i].item = signersInfo->tags.pointers[i];
        tagValuePairs[i].value = signersInfo->values.pointers[i];
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
        _cleanup(APDU_RESPONSE_OK);
        nbgl_useCaseStatus("Safe Address validated", true, ui_idle);
        return;
    } else {
        // User rejected the Safe Account
        _cleanup(APDU_RESPONSE_CONDITION_NOT_SATISFIED);
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
        _cleanup(APDU_RESPONSE_INSUFFICIENT_MEMORY);
        return;
    }
    // Prepare strings for display
    _prepare_strings();

    // Setup data to display
    setTagValuePairs();

    nbgl_useCaseAddressReview(strings.tmp.tmp,
                              g_pairsList,
                              &C_multisig,
                              "Verify Safe address",
                              NULL,
                              review_cb);
}

#endif  // HAVE_SAFE_ACCOUNT
