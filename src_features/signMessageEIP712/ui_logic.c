#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "ui_logic.h"
#include "mem.h"
#include "mem_utils.h"
#include "os_io.h"
#include "shared_context.h"
#include "common_utils.h"  // uint256_to_decimal
#include "common_712.h"
#include "context_712.h"     // eip712_context_deinit
#include "uint256.h"         // tostring256 && tostring256_signed
#include "path.h"            // path_get_root_type
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"
#include "commands_712.h"
#include "common_ui.h"
#include "uint_common.h"

#define AMOUNT_JOIN_FLAG_TOKEN (1 << 0)
#define AMOUNT_JOIN_FLAG_VALUE (1 << 1)

typedef struct {
    // display name, not NULL-terminated
    char name[25];
    uint8_t name_length;
    uint8_t value[INT256_LENGTH];
    uint8_t value_length;
    // indicates the steps the token join has gone through
    uint8_t flags;
} s_amount_join;

typedef enum {
    AMOUNT_JOIN_STATE_TOKEN,
    AMOUNT_JOIN_STATE_VALUE,
} e_amount_join_state;

#define UI_712_FIELD_SHOWN         (1 << 0)
#define UI_712_FIELD_NAME_PROVIDED (1 << 1)
#define UI_712_AMOUNT_JOIN         (1 << 2)
#define UI_712_DATETIME            (1 << 3)

typedef struct {
    s_amount_join joins[MAX_ASSETS];
    uint8_t idx;
    e_amount_join_state state;
} s_amount_context;

typedef struct {
    bool shown;
    bool end_reached;
    uint8_t filtering_mode;
    uint8_t filters_to_process;
    uint8_t field_flags;
    uint8_t structs_to_review;
    s_amount_context amount;
} t_ui_context;

static t_ui_context *ui_ctx = NULL;

/**
 * Checks on the UI context to determine if the next EIP 712 field should be shown
 *
 * @return whether the next field should be shown
 */
static bool ui_712_field_shown(void) {
    bool ret = false;

    if (ui_ctx->filtering_mode == EIP712_FILTERING_BASIC) {
        if (N_storage.verbose_eip712 || (path_get_root_type() == ROOT_DOMAIN)) {
            ret = true;
        }
    } else {  // EIP712_FILTERING_FULL
        if (ui_ctx->field_flags & UI_712_FIELD_SHOWN) {
            ret = true;
        }
    }
    return ret;
}

/**
 * Set UI buffer
 *
 * @param[in] src source buffer
 * @param[in] src_length source buffer size
 * @param[in] dst destination buffer
 * @param[in] dst_length destination buffer length
 * @param[in] explicit_trunc if truncation should be explicitly shown
 */
static void ui_712_set_buf(const char *const src,
                           size_t src_length,
                           char *const dst,
                           size_t dst_length,
                           bool explicit_trunc) {
    uint8_t cpy_length;

    if (src_length < dst_length) {
        cpy_length = src_length;
    } else {
        cpy_length = dst_length - 1;
    }
    memcpy(dst, src, cpy_length);
    dst[cpy_length] = '\0';
    if (explicit_trunc && (cpy_length < src_length)) {
        memcpy(dst + cpy_length - 3, "...", 3);
    }
}

/**
 * Skip the field if needed and reset its UI flags
 */
void ui_712_finalize_field(void) {
    if (!ui_712_field_shown()) {
        ui_712_next_field();
    }
    ui_712_field_flags_reset();
}

/**
 * Set a new title for the EIP-712 generic UX_STEP
 *
 * @param[in] str the new title
 * @param[in] length its length
 */
void ui_712_set_title(const char *const str, uint8_t length) {
    ui_712_set_buf(str, length, strings.tmp.tmp2, sizeof(strings.tmp.tmp2), false);
}

/**
 * Set a new value for the EIP-712 generic UX_STEP
 *
 * @param[in] str the new value
 * @param[in] length its length
 */
void ui_712_set_value(const char *const str, uint8_t length) {
    ui_712_set_buf(str, length, strings.tmp.tmp, sizeof(strings.tmp.tmp), true);
}

/**
 * Redraw the dynamic UI step that shows EIP712 information
 */
void ui_712_redraw_generic_step(void) {
    if (!ui_ctx->shown) {  // Initialize if it is not already
        ui_712_start();
        ui_ctx->shown = true;
    } else {
        ui_712_switch_to_message();
    }
}

/**
 * Called to fetch the next field if they have not all been processed yet
 *
 * Also handles the special "Review struct" screen of the verbose mode
 *
 * @return the next field state
 */
e_eip712_nfs ui_712_next_field(void) {
    e_eip712_nfs state = EIP712_NO_MORE_FIELD;

    if (ui_ctx == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    } else {
        if (ui_ctx->structs_to_review > 0) {
            ui_712_review_struct(path_get_nth_field_to_last(ui_ctx->structs_to_review));
            ui_ctx->structs_to_review -= 1;
            state = EIP712_FIELD_LATER;
        } else if (!ui_ctx->end_reached) {
            handle_eip712_return_code(true);
            state = EIP712_FIELD_INCOMING;
        }
    }
    return state;
}

/**
 * Used to notify of a new struct to review
 *
 * @param[in] struct_ptr pointer to the structure to be shown
 */
void ui_712_review_struct(const void *const struct_ptr) {
    const char *struct_name;
    uint8_t struct_name_length;
    const char *const title = "Review struct";

    if (ui_ctx == NULL) {
        return;
    }

    ui_712_set_title(title, strlen(title));
    if ((struct_name = get_struct_name(struct_ptr, &struct_name_length)) != NULL) {
        ui_712_set_value(struct_name, struct_name_length);
    }
    ui_712_redraw_generic_step();
}

/**
 * Show the hash of the message on the generic UI step
 */
void ui_712_message_hash(void) {
    const char *const title = "Message hash";

    ui_712_set_title(title, strlen(title));
    array_bytes_string(strings.tmp.tmp,
                       sizeof(strings.tmp.tmp),
                       tmpCtx.messageSigningContext712.messageHash,
                       KECCAK256_HASH_BYTESIZE);
    ui_712_redraw_generic_step();
}

/**
 * Format a given data as a string
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 */
static void ui_712_format_str(const uint8_t *const data, uint8_t length) {
    if (ui_712_field_shown()) {
        ui_712_set_value((char *) data, length);
    }
}

/**
 * Format a given data as a string representation of an address
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return if the formatting was successful
 */
static bool ui_712_format_addr(const uint8_t *const data, uint8_t length) {
    if (length != ADDRESS_LENGTH) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if (ui_712_field_shown()) {
        if (!getEthDisplayableAddress((uint8_t *) data,
                                      strings.tmp.tmp,
                                      sizeof(strings.tmp.tmp),
                                      chainConfig->chainId)) {
            THROW(APDU_RESPONSE_ERROR_NO_INFO);
        }
    }
    return true;
}

/**
 * Format given data as a string representation of a boolean
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return if the formatting was successful
 */
static bool ui_712_format_bool(const uint8_t *const data, uint8_t length) {
    const char *const true_str = "true";
    const char *const false_str = "false";
    const char *str;

    if (length != 1) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    str = *data ? true_str : false_str;
    if (ui_712_field_shown()) {
        ui_712_set_value(str, strlen(str));
    }
    return true;
}

/**
 * Format given data as a string representation of bytes
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 */
static void ui_712_format_bytes(const uint8_t *const data, uint8_t length) {
    if (ui_712_field_shown()) {
        array_bytes_string(strings.tmp.tmp, sizeof(strings.tmp.tmp), data, length);
        // +2 for the "0x"
        // x2 for each byte value is represented by 2 ASCII characters
        if ((2 + (length * 2)) > (sizeof(strings.tmp.tmp) - 1)) {
            strings.tmp.tmp[sizeof(strings.tmp.tmp) - 1 - 3] = '\0';
            strlcat(strings.tmp.tmp, "...", sizeof(strings.tmp.tmp));
        }
    }
}

/**
 * Format given data as a string representation of an integer
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return if the formatting was successful
 */
static bool ui_712_format_int(const uint8_t *const data,
                              uint8_t length,
                              const void *const field_ptr) {
    uint256_t value256;
    uint128_t value128;
    int32_t value32;
    int16_t value16;

    switch (get_struct_field_typesize(field_ptr) * 8) {
        case 256:
            convertUint256BE(data, length, &value256);
            if (ui_712_field_shown()) {
                tostring256_signed(&value256, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
            }
            break;
        case 128:
            convertUint128BE(data, length, &value128);
            if (ui_712_field_shown()) {
                tostring128_signed(&value128, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
            }
            break;
        case 64:
            convertUint64BEto128(data, length, &value128);
            if (ui_712_field_shown()) {
                tostring128_signed(&value128, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
            }
            break;
        case 32:
            value32 = 0;
            for (int i = 0; i < length; ++i) {
                ((uint8_t *) &value32)[length - 1 - i] = data[i];
            }
            if (ui_712_field_shown()) {
                snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "%d", value32);
            }
            break;
        case 16:
            value16 = 0;
            for (int i = 0; i < length; ++i) {
                ((uint8_t *) &value16)[length - 1 - i] = data[i];
            }
            if (ui_712_field_shown()) {
                snprintf(strings.tmp.tmp,
                         sizeof(strings.tmp.tmp),
                         "%d",
                         value16);  // expanded to 32 bits
            }
            break;
        case 8:
            if (ui_712_field_shown()) {
                snprintf(strings.tmp.tmp,
                         sizeof(strings.tmp.tmp),
                         "%d",
                         ((int8_t *) data)[0]);  // expanded to 32 bits
            }
            break;
        default:
            PRINTF("Unhandled field typesize\n");
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return false;
    }
    return true;
}

/**
 * Format given data as a string representation of an unsigned integer
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 */
static void ui_712_format_uint(const uint8_t *const data, uint8_t length) {
    uint256_t value256;

    convertUint256BE(data, length, &value256);
    if (ui_712_field_shown()) {
        tostring256(&value256, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
    }
}

/**
 * Format given data as an amount with its ticker and value with correct decimals
 *
 * @return whether it was successful or not
 */
static bool ui_712_format_amount_join(void) {
    const tokenDefinition_t *token;
    token = &tmpCtx.transactionContext.extraInfo[ui_ctx->amount.idx].token;

    if (!amountToString(ui_ctx->amount.joins[ui_ctx->amount.idx].value,
                        ui_ctx->amount.joins[ui_ctx->amount.idx].value_length,
                        token->decimals,
                        token->ticker,
                        strings.tmp.tmp,
                        sizeof(strings.tmp.tmp))) {
        return false;
    }
    ui_ctx->field_flags |= UI_712_FIELD_SHOWN;
    ui_712_set_title(ui_ctx->amount.joins[ui_ctx->amount.idx].name,
                     ui_ctx->amount.joins[ui_ctx->amount.idx].name_length);
    explicit_bzero(&ui_ctx->amount.joins[ui_ctx->amount.idx],
                   sizeof(ui_ctx->amount.joins[ui_ctx->amount.idx]));
    return true;
}

/**
 * Update the state of the amount-join
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return whether it was successful or not
 */
static bool update_amount_join(const uint8_t *data, uint8_t length) {
    tokenDefinition_t *token;

    token = &tmpCtx.transactionContext.extraInfo[ui_ctx->amount.idx].token;
    switch (ui_ctx->amount.state) {
        case AMOUNT_JOIN_STATE_TOKEN:
            if (memcmp(data, token->address, ADDRESS_LENGTH) != 0) {
                return false;
            }
            ui_ctx->amount.joins[ui_ctx->amount.idx].flags |= AMOUNT_JOIN_FLAG_TOKEN;
            break;

        case AMOUNT_JOIN_STATE_VALUE:
            memcpy(ui_ctx->amount.joins[ui_ctx->amount.idx].value, data, length);
            ui_ctx->amount.joins[ui_ctx->amount.idx].value_length = length;
            ui_ctx->amount.joins[ui_ctx->amount.idx].flags |= AMOUNT_JOIN_FLAG_VALUE;
            break;

        default:
            return false;
    }
    return true;
}

/**
 * Format given data as a human-readable date/time representation
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return whether it was successful or not
 */
static bool ui_712_format_datetime(const uint8_t *data, uint8_t length) {
    struct tm tstruct;
    int shown_hour;
    time_t timestamp = u64_from_BE(data, length);

    if (gmtime_r(&timestamp, &tstruct) == NULL) {
        return false;
    }
    if (tstruct.tm_hour == 0) {
        shown_hour = 12;
    } else {
        shown_hour = tstruct.tm_hour;
        if (shown_hour > 12) {
            shown_hour -= 12;
        }
    }
    snprintf(strings.tmp.tmp,
             sizeof(strings.tmp.tmp),
             "%04d-%02d-%02d\n%02d:%02d:%02d %s UTC",
             tstruct.tm_year + 1900,
             tstruct.tm_mon + 1,
             tstruct.tm_mday,
             shown_hour,
             tstruct.tm_min,
             tstruct.tm_sec,
             (tstruct.tm_hour < 12) ? "AM" : "PM");
    return true;
}

/**
 * Used to notify of a new field to review in the current struct (key + value)
 *
 * @param[in] field_ptr pointer to the new struct field
 * @param[in] data pointer to the field's raw value
 * @param[in] length field's raw value byte-length
 */
bool ui_712_new_field(const void *const field_ptr, const uint8_t *const data, uint8_t length) {
    const char *key;
    uint8_t key_len;

    if (ui_ctx == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    // Key
    if ((key = get_struct_field_keyname(field_ptr, &key_len)) == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    if (ui_712_field_shown() && !(ui_ctx->field_flags & UI_712_FIELD_NAME_PROVIDED)) {
        ui_712_set_title(key, key_len);
    }

    // Value
    switch (struct_field_type(field_ptr)) {
        case TYPE_SOL_STRING:
            ui_712_format_str(data, length);
            break;
        case TYPE_SOL_ADDRESS:
            if (ui_712_format_addr(data, length) == false) {
                return false;
            }
            break;
        case TYPE_SOL_BOOL:
            if (ui_712_format_bool(data, length) == false) {
                return false;
            }
            break;
        case TYPE_SOL_BYTES_FIX:
        case TYPE_SOL_BYTES_DYN:
            ui_712_format_bytes(data, length);
            break;
        case TYPE_SOL_INT:
            if (ui_712_format_int(data, length, field_ptr) == false) {
                return false;
            }
            break;
        case TYPE_SOL_UINT:
            ui_712_format_uint(data, length);
            break;
        default:
            PRINTF("Unhandled type\n");
            return false;
    }

    if (ui_ctx->field_flags & UI_712_AMOUNT_JOIN) {
        if (!update_amount_join(data, length)) {
            return false;
        }

        if (ui_ctx->amount.joins[ui_ctx->amount.idx].flags ==
            (AMOUNT_JOIN_FLAG_TOKEN | AMOUNT_JOIN_FLAG_VALUE)) {
            if (!ui_712_format_amount_join()) {
                return false;
            }
        }
    }

    if (ui_ctx->field_flags & UI_712_DATETIME) {
        if (!ui_712_format_datetime(data, length)) {
            return false;
        }
    }

    // Check if this field is supposed to be displayed
    if (ui_712_field_shown()) {
        ui_712_redraw_generic_step();
    }
    return true;
}

/**
 * Used to signal that we are done with reviewing the structs and we can now have
 * the option to approve or reject the signature
 */
void ui_712_end_sign(void) {
    if (ui_ctx == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return;
    }
    ui_ctx->end_reached = true;

    if (N_storage.verbose_eip712 || (ui_ctx->filtering_mode == EIP712_FILTERING_FULL)) {
        ui_712_switch_to_sign();
    }
}

/**
 * Initializes the UI context structure in memory
 */
bool ui_712_init(void) {
    if ((ui_ctx = MEM_ALLOC_AND_ALIGN_TYPE(*ui_ctx))) {
        ui_ctx->shown = false;
        ui_ctx->end_reached = false;
        ui_ctx->filtering_mode = EIP712_FILTERING_BASIC;
        explicit_bzero(&ui_ctx->amount, sizeof(ui_ctx->amount));
    } else {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
    }
    return ui_ctx != NULL;
}

/**
 * Deinit function that simply unsets the struct pointer to NULL
 */
void ui_712_deinit(void) {
    ui_ctx = NULL;
}

/**
 * Approve button handling, calls the common handler function then
 * deinitializes the EIP712 context altogether.
 * @param[in] e unused here, just needed to match the UI function signature
 * @return unused here, just needed to match the UI function signature
 */
unsigned int ui_712_approve() {
    ui_712_approve_cb();
    eip712_context_deinit();
    return 0;
}

/**
 * Reject button handling, calls the common handler function then
 * deinitializes the EIP712 context altogether.

 * @param[in] e unused here, just needed to match the UI function signature
 * @return unused here, just needed to match the UI function signature
 */
unsigned int ui_712_reject() {
    ui_712_reject_cb();
    eip712_context_deinit();
    return 0;
}

/**
 * Set a structure field's UI flags
 *
 * @param[in] show if this field should be shown on the device
 * @param[in] name_provided if a substitution name has been provided
 * @param[in] token_join if this field is part of a token join
 * @param[in] datetime if this field should be shown and formatted as a date/time
 */
void ui_712_flag_field(bool show, bool name_provided, bool token_join, bool datetime) {
    if (show) {
        ui_ctx->field_flags |= UI_712_FIELD_SHOWN;
    }
    if (name_provided) {
        ui_ctx->field_flags |= UI_712_FIELD_NAME_PROVIDED;
    }
    if (token_join) {
        ui_ctx->field_flags |= UI_712_AMOUNT_JOIN;
    }
    if (datetime) {
        ui_ctx->field_flags |= UI_712_DATETIME;
    }
}

/**
 * Set the UI filtering mode
 *
 * @param[in] the new filtering mode
 */
void ui_712_set_filtering_mode(e_eip712_filtering_mode mode) {
    ui_ctx->filtering_mode = mode;
}

/**
 * Get the UI filtering mode
 *
 * @return current filtering mode
 */
e_eip712_filtering_mode ui_712_get_filtering_mode(void) {
    return ui_ctx->filtering_mode;
}

/**
 * Set the number of filters this message should process
 *
 * @param[in] count number of filters
 */
void ui_712_set_filters_count(uint8_t count) {
    ui_ctx->filters_to_process = count;
}

/**
 * Get the number of filters left to process
 *
 * @return number of filters
 */
uint8_t ui_712_remaining_filters(void) {
    return ui_ctx->filters_to_process;
}

/**
 * Reset all the UI struct field flags
 */
void ui_712_field_flags_reset(void) {
    ui_ctx->field_flags = 0;
}

/**
 * Add a struct to the UI review queue
 *
 * Makes it so the user will have to go through a "Review struct" screen
 */
void ui_712_queue_struct_to_review(void) {
    if (N_storage.verbose_eip712) {
        ui_ctx->structs_to_review += 1;
    }
}

/**
 * Notify of a filter change from a path advance
 *
 * This function figures out by itself if there is anything to do
 */
void ui_712_notify_filter_change(void) {
    if (path_get_root_type() == ROOT_MESSAGE) {
        if (ui_ctx->filtering_mode == EIP712_FILTERING_FULL) {
            if (ui_ctx->filters_to_process > 0) {
                if (ui_ctx->field_flags & UI_712_FIELD_SHOWN) {
                    ui_ctx->filters_to_process -= 1;
                }
            }
        }
    }
}

void ui_712_token_join_prepare_addr_check(uint8_t index) {
    ui_ctx->amount.idx = index;
    ui_ctx->amount.state = AMOUNT_JOIN_STATE_TOKEN;
}

void ui_712_token_join_prepare_amount(uint8_t index, const char *name, uint8_t name_length) {
    uint8_t cpy_len = MIN(sizeof(ui_ctx->amount.joins[index].name), name_length);

    ui_ctx->amount.idx = index;
    ui_ctx->amount.state = AMOUNT_JOIN_STATE_VALUE;
    memcpy(ui_ctx->amount.joins[index].name, name, cpy_len);
    ui_ctx->amount.joins[index].name_length = cpy_len;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
