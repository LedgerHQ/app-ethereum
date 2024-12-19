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
#include "filtering.h"
#include "trusted_name.h"
#include "network.h"
#include "time_format.h"

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
#define UI_712_TRUSTED_NAME        (1 << 4)

typedef struct {
    s_amount_join joins[MAX_ASSETS];
    uint8_t idx;
    e_amount_join_state state;
} s_amount_context;

typedef struct {
    bool shown;
    bool end_reached;
    e_eip712_filtering_mode filtering_mode;
    uint8_t filters_to_process;
    uint8_t field_flags;
    uint8_t structs_to_review;
    s_amount_context amount;
    uint8_t filters_received;
    uint32_t filters_crc[MAX_FILTERS];
    uint8_t discarded_path_length;
    char discarded_path[255];
#ifdef HAVE_TRUSTED_NAME
    uint8_t tn_type_count;
    uint8_t tn_source_count;
    e_name_type tn_types[TN_TYPE_COUNT];
    e_name_source tn_sources[TN_SOURCE_COUNT];
#endif
#ifdef SCREEN_SIZE_WALLET
    char ui_pairs_buffer[(SHARED_CTX_FIELD_1_SIZE + SHARED_CTX_FIELD_2_SIZE) * 2];
#endif
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
#ifdef SCREEN_SIZE_WALLET
        if (true) {
#else
        if (N_storage.verbose_eip712 || (path_get_root_type() == ROOT_DOMAIN)) {
#endif
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
static void ui_712_set_buf(const char *src,
                           size_t src_length,
                           char *dst,
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
void ui_712_set_title(const char *str, size_t length) {
    ui_712_set_buf(str, length, strings.tmp.tmp2, sizeof(strings.tmp.tmp2), false);
}

/**
 * Set a new value for the EIP-712 generic UX_STEP
 *
 * @param[in] str the new value
 * @param[in] length its length
 */
void ui_712_set_value(const char *str, size_t length) {
    ui_712_set_buf(str, length, strings.tmp.tmp, sizeof(strings.tmp.tmp), true);
}

/**
 * Redraw the dynamic UI step that shows EIP712 information
 *
 * @return whether it was successful or not
 */
bool ui_712_redraw_generic_step(void) {
    if (!ui_ctx->shown) {  // Initialize if it is not already
        if ((ui_ctx->filtering_mode == EIP712_FILTERING_BASIC) && !N_storage.dataAllowed &&
            !N_storage.verbose_eip712) {
            // Both settings not enabled => Error
            ui_error_blind_signing();
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            eip712_context->go_home_on_failure = false;
            return false;
        }
        if (ui_ctx->filtering_mode == EIP712_FILTERING_BASIC) {
            ui_712_start_unfiltered();
        } else {
            ui_712_start();
        }
        ui_ctx->shown = true;
    } else {
        ui_712_switch_to_message();
    }
    return true;
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
            // So that later when we append to them, we start from an empty string
            explicit_bzero(&strings, sizeof(strings));
        }
    }
    return state;
}

/**
 * Used to notify of a new struct to review
 *
 * @param[in] struct_ptr pointer to the structure to be shown
 * @return whether it was successful or not
 */
bool ui_712_review_struct(const void *struct_ptr) {
    const char *struct_name;
    uint8_t struct_name_length;
    const char *title = "Review struct";

    if (ui_ctx == NULL) {
        return false;
    }

    ui_712_set_title(title, strlen(title));
    if ((struct_name = get_struct_name(struct_ptr, &struct_name_length)) != NULL) {
        ui_712_set_value(struct_name, struct_name_length);
    }
    return ui_712_redraw_generic_step();
}

/**
 * Show the hash of the message on the generic UI step
 */
void ui_712_message_hash(void) {
    const char *title = "Message hash";

    ui_712_set_title(title, strlen(title));
    array_bytes_string(strings.tmp.tmp,
                       sizeof(strings.tmp.tmp),
                       tmpCtx.messageSigningContext712.messageHash,
                       KECCAK256_HASH_BYTESIZE);
    ui_ctx->end_reached = true;
    ui_712_redraw_generic_step();
}

/**
 * Format a given data as a string
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @param[in] last if this is the last chunk
 */
static void ui_712_format_str(const uint8_t *data, uint8_t length, bool last) {
    size_t max_len = sizeof(strings.tmp.tmp) - 1;
    size_t cur_len = strlen(strings.tmp.tmp);

    memcpy(strings.tmp.tmp + cur_len, data, MIN(max_len - cur_len, length));
    // truncated
    if (last && ((max_len - cur_len) < length)) {
        memcpy(strings.tmp.tmp + max_len - 3, "...", 3);
    }
}

/**
 * Format a given data as a string representation of an address
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @param[in] first if this is the first chunk
 * @return if the formatting was successful
 */
static bool ui_712_format_addr(const uint8_t *data, uint8_t length, bool first) {
    // no reason for an address to be received over multiple chunks
    if (!first) {
        return false;
    }
    if (length != ADDRESS_LENGTH) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if (!getEthDisplayableAddress((uint8_t *) data,
                                  strings.tmp.tmp,
                                  sizeof(strings.tmp.tmp),
                                  chainConfig->chainId)) {
        apdu_response_code = APDU_RESPONSE_ERROR_NO_INFO;
        return false;
    }
    return true;
}

/**
 * Format given data as a string representation of a boolean
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @param[in] first if this is the first chunk
 * @return if the formatting was successful
 */
static bool ui_712_format_bool(const uint8_t *data, uint8_t length, bool first) {
    size_t max_len = sizeof(strings.tmp.tmp) - 1;
    const char *true_str = "true";
    const char *false_str = "false";
    const char *str;

    // no reason for a boolean to be received over multiple chunks
    if (!first) {
        return false;
    }
    if (length != 1) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    str = *data ? true_str : false_str;
    memcpy(strings.tmp.tmp, str, MIN(max_len, strlen(str)));
    return true;
}

/**
 * Format given data as a string representation of bytes
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @param[in] first if this is the first chunk
 * @param[in] last if this is the last chunk
 * @return if the formatting was successful
 */
static bool ui_712_format_bytes(const uint8_t *data, uint8_t length, bool first, bool last) {
    size_t max_len = sizeof(strings.tmp.tmp) - 1;
    size_t cur_len = strlen(strings.tmp.tmp);

    if (first) {
        memcpy(strings.tmp.tmp, "0x", MIN(max_len, 2));
        cur_len += 2;
    }
    if (format_hex(data,
                   MIN((max_len - cur_len) / 2, length),
                   strings.tmp.tmp + cur_len,
                   max_len + 1 - cur_len) < 0) {
        return false;
    }
    // truncated
    if (last && (((max_len - cur_len) / 2) < length)) {
        memcpy(strings.tmp.tmp + max_len - 3, "...", 3);
    }
    return true;
}

/**
 * Format given data as a string representation of an integer
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @param[in] first if this is the first chunk
 * @param[in] field_ptr pointer to the EIP-712 field
 * @return if the formatting was successful
 */
static bool ui_712_format_int(const uint8_t *data,
                              uint8_t length,
                              bool first,
                              const void *field_ptr) {
    uint256_t value256;
    uint128_t value128;
    int32_t value32;
    int16_t value16;

    // no reason for an integer to be received over multiple chunks
    if (!first) {
        return false;
    }
    switch (get_struct_field_typesize(field_ptr) * 8) {
        case 256:
            convertUint256BE(data, length, &value256);
            tostring256_signed(&value256, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
            break;
        case 128:
            convertUint128BE(data, length, &value128);
            tostring128_signed(&value128, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
            break;
        case 64:
            convertUint64BEto128(data, length, &value128);
            tostring128_signed(&value128, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
            break;
        case 32:
            value32 = 0;
            for (int i = 0; i < length; ++i) {
                ((uint8_t *) &value32)[length - 1 - i] = data[i];
            }
            snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "%d", value32);
            break;
        case 16:
            value16 = 0;
            for (int i = 0; i < length; ++i) {
                ((uint8_t *) &value16)[length - 1 - i] = data[i];
            }
            snprintf(strings.tmp.tmp,
                     sizeof(strings.tmp.tmp),
                     "%d",
                     value16);  // expanded to 32 bits
            break;
        case 8:
            snprintf(strings.tmp.tmp,
                     sizeof(strings.tmp.tmp),
                     "%d",
                     ((int8_t *) data)[0]);  // expanded to 32 bits
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
 * @param[in] first if this is the first chunk
 * @return if the formatting was successful
 */
static bool ui_712_format_uint(const uint8_t *data, uint8_t length, bool first) {
    uint256_t value256;

    // no reason for an integer to be received over multiple chunks
    if (!first) {
        return false;
    }
    convertUint256BE(data, length, &value256);
    tostring256(&value256, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
    return true;
}

/**
 * Format given data as an amount with its ticker and value with correct decimals
 *
 * @return whether it was successful or not
 */
static bool ui_712_format_amount_join(void) {
    const tokenDefinition_t *token = NULL;

    if (tmpCtx.transactionContext.assetSet[ui_ctx->amount.idx]) {
        token = &tmpCtx.transactionContext.extraInfo[ui_ctx->amount.idx].token;
    }
    if ((ui_ctx->amount.joins[ui_ctx->amount.idx].value_length == INT256_LENGTH) &&
        ismaxint(ui_ctx->amount.joins[ui_ctx->amount.idx].value,
                 ui_ctx->amount.joins[ui_ctx->amount.idx].value_length)) {
        strlcpy(strings.tmp.tmp, "Unlimited ", sizeof(strings.tmp.tmp));
        strlcat(strings.tmp.tmp,
                (token != NULL) ? token->ticker : g_unknown_ticker,
                sizeof(strings.tmp.tmp));
    } else {
        if (!amountToString(ui_ctx->amount.joins[ui_ctx->amount.idx].value,
                            ui_ctx->amount.joins[ui_ctx->amount.idx].value_length,
                            (token != NULL) ? token->decimals : 0,
                            (token != NULL) ? token->ticker : g_unknown_ticker,
                            strings.tmp.tmp,
                            sizeof(strings.tmp.tmp))) {
            return false;
        }
    }
    ui_ctx->field_flags |= UI_712_FIELD_SHOWN;
    ui_712_set_title(ui_ctx->amount.joins[ui_ctx->amount.idx].name,
                     ui_ctx->amount.joins[ui_ctx->amount.idx].name_length);
    explicit_bzero(&ui_ctx->amount.joins[ui_ctx->amount.idx],
                   sizeof(ui_ctx->amount.joins[ui_ctx->amount.idx]));
    return true;
}

/**
 * Simply mark the current amount-join's token address as received
 */
void amount_join_set_token_received(void) {
    ui_ctx->amount.joins[ui_ctx->amount.idx].flags |= AMOUNT_JOIN_FLAG_TOKEN;
}

/**
 * Update the state of the amount-join
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return whether it was successful or not
 */
static bool update_amount_join(const uint8_t *data, uint8_t length) {
    const tokenDefinition_t *token = NULL;

    if (tmpCtx.transactionContext.assetSet[ui_ctx->amount.idx]) {
        token = &tmpCtx.transactionContext.extraInfo[ui_ctx->amount.idx].token;
    } else {
        if (tmpCtx.transactionContext.currentAssetIndex == ui_ctx->amount.idx) {
            // So that the following amount-join find their tokens in the expected indices
            tmpCtx.transactionContext.currentAssetIndex =
                (tmpCtx.transactionContext.currentAssetIndex + 1) % MAX_ASSETS;
        }
    }
    switch (ui_ctx->amount.state) {
        case AMOUNT_JOIN_STATE_TOKEN:
            if (token != NULL) {
                if (memcmp(data, token->address, ADDRESS_LENGTH) != 0) {
                    return false;
                }
            }
            amount_join_set_token_received();
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

#ifdef HAVE_TRUSTED_NAME
/**
 * Try to substitute given address by a matching contract name
 *
 * Fallback on showing the address if no match is found.
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return whether it was successful or not
 */
static bool ui_712_format_trusted_name(const uint8_t *data, uint8_t length) {
    if (length != ADDRESS_LENGTH) {
        return false;
    }
    if (get_trusted_name(ui_ctx->tn_type_count,
                         ui_ctx->tn_types,
                         ui_ctx->tn_source_count,
                         ui_ctx->tn_sources,
                         &eip712_context->chain_id,
                         data) != NULL) {
        strlcpy(strings.tmp.tmp, g_trusted_name, sizeof(strings.tmp.tmp));
    }
    return true;
}
#endif

/**
 * Format given data as a human-readable date/time representation
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return whether it was successful or not
 */
static bool ui_712_format_datetime(const uint8_t *data, uint8_t length) {
    time_t timestamp = u64_from_BE(data, length);

    return time_format_to_utc(&timestamp, strings.tmp.tmp, sizeof(strings.tmp.tmp));
}

/**
 * Formats and feeds the given input data to the display buffers
 *
 * @param[in] field_ptr pointer to the new struct field
 * @param[in] data pointer to the field's raw value
 * @param[in] length field's raw value byte-length
 * @param[in] first if this is the first chunk
 * @param[in] last if this is the last chunk
 */
bool ui_712_feed_to_display(const void *field_ptr,
                            const uint8_t *data,
                            uint8_t length,
                            bool first,
                            bool last) {
    if (ui_ctx == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    if (first && (strlen(strings.tmp.tmp) > 0)) {
        return false;
    }
    // Value
    if (ui_712_field_shown()) {
        switch (struct_field_type(field_ptr)) {
            case TYPE_SOL_STRING:
                ui_712_format_str(data, length, last);
                break;
            case TYPE_SOL_ADDRESS:
                if (ui_712_format_addr(data, length, first) == false) {
                    return false;
                }
                break;
            case TYPE_SOL_BOOL:
                if (ui_712_format_bool(data, length, first) == false) {
                    return false;
                }
                break;
            case TYPE_SOL_BYTES_FIX:
            case TYPE_SOL_BYTES_DYN:
                if (ui_712_format_bytes(data, length, first, last) == false) {
                    return false;
                }
                break;
            case TYPE_SOL_INT:
                if (ui_712_format_int(data, length, first, field_ptr) == false) {
                    return false;
                }
                break;
            case TYPE_SOL_UINT:
                if (ui_712_format_uint(data, length, first) == false) {
                    return false;
                }
                break;
            default:
                PRINTF("Unhandled type\n");
                return false;
        }
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

#ifdef HAVE_TRUSTED_NAME
    if (ui_ctx->field_flags & UI_712_TRUSTED_NAME) {
        if (!ui_712_format_trusted_name(data, length)) {
            return false;
        }
    }
#endif

    // Check if this field is supposed to be displayed
    if (last && ui_712_field_shown()) {
        if (!ui_712_redraw_generic_step()) return false;
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

#ifdef SCREEN_SIZE_WALLET
    if (true) {
#else
    if (N_storage.verbose_eip712 || (ui_ctx->filtering_mode == EIP712_FILTERING_FULL)) {
#endif
        ui_ctx->end_reached = true;
        ui_712_switch_to_sign();
    }
}

/**
 * Initializes the UI context structure in memory
 */
bool ui_712_init(void) {
    if ((ui_ctx = MEM_ALLOC_AND_ALIGN_TYPE(*ui_ctx))) {
        explicit_bzero(ui_ctx, sizeof(*ui_ctx));
        ui_ctx->filtering_mode = EIP712_FILTERING_BASIC;
        explicit_bzero(&strings, sizeof(strings));
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
 * @param[in] trusted_name if this field should be shown as a trusted contract name
 */
void ui_712_flag_field(bool show,
                       bool name_provided,
                       bool token_join,
                       bool datetime,
                       bool trusted_name) {
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
    if (trusted_name) {
        ui_ctx->field_flags |= UI_712_TRUSTED_NAME;
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
    return ui_ctx->filters_to_process - ui_ctx->filters_received;
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
#ifdef SCREEN_SIZE_WALLET
    if (true) {
#else
    if (N_storage.verbose_eip712) {
#endif
        ui_ctx->structs_to_review += 1;
    }
}

/**
 * Increment the filters counter
 *
 * @return if the counter could be incremented
 */
bool ui_712_filters_counter_incr(void) {
    if (ui_ctx->filters_received > ui_ctx->filters_to_process) {
        return false;
    }
    ui_ctx->filters_received += 1;
    return true;
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

/**
 * Set UI pair key to the raw JSON key
 *
 * @param[in] field_ptr pointer to the field
 * @return whether it was successful or not
 */
bool ui_712_show_raw_key(const void *field_ptr) {
    const char *key;
    uint8_t key_len;

    if ((key = get_struct_field_keyname(field_ptr, &key_len)) == NULL) {
        return false;
    }

    if (ui_712_field_shown() && !(ui_ctx->field_flags & UI_712_FIELD_NAME_PROVIDED)) {
        ui_712_set_title(key, key_len);
    }
    return true;
}

/**
 * Push a new filter path
 *
 * @param[in] path_crc CRC of the filter path
 * @return if the path was pushed or not (in case it was already present)
 */
bool ui_712_push_new_filter_path(uint32_t path_crc) {
    // check if already present
    for (int i = 0; i < ui_ctx->filters_received; ++i) {
        if (ui_ctx->filters_crc[i] == path_crc) {
            PRINTF("EIP-712 path CRC (%x) already found at index %u!\n", path_crc, i);
            return false;
        }
    }
    PRINTF("Pushing new EIP-712 path CRC (%x) at index %u\n", path_crc, ui_ctx->filters_received);
    ui_ctx->filters_crc[ui_ctx->filters_received] = path_crc;
    return true;
}

/**
 * Set a discarded filter path
 *
 * @param[in] path the given filter path
 * @param[in] length the path length
 */
void ui_712_set_discarded_path(const char *path, uint8_t length) {
    memcpy(ui_ctx->discarded_path, path, length);
    ui_ctx->discarded_path_length = length;
}

/**
 * Get the discarded filter path
 *
 * @param[out] length the path length
 * @return filter path
 */
const char *ui_712_get_discarded_path(uint8_t *length) {
    *length = ui_ctx->discarded_path_length;
    return ui_ctx->discarded_path;
}

#ifdef HAVE_TRUSTED_NAME
void ui_712_set_trusted_name_requirements(uint8_t type_count,
                                          const e_name_type *types,
                                          uint8_t source_count,
                                          const e_name_source *sources) {
    ui_ctx->tn_type_count = type_count;
    memcpy(ui_ctx->tn_types, types, type_count);
    ui_ctx->tn_source_count = source_count;
    memcpy(ui_ctx->tn_sources, sources, source_count);
}
#endif

#ifdef SCREEN_SIZE_WALLET
/*
 * Get UI pairs buffer
 *
 * @param[out] size buffer size
 * @return pointer to the buffer
 */
char *get_ui_pairs_buffer(size_t *size) {
    *size = sizeof(ui_ctx->ui_pairs_buffer);
    return ui_ctx->ui_pairs_buffer;
}
#endif

#endif  // HAVE_EIP712_FULL_SUPPORT
