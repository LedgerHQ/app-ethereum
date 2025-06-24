#include "ui_logic.h"
#include "mem.h"
#include "mem_utils.h"
#include "os_io.h"
#include "common_utils.h"  // uint256_to_decimal
#include "common_712.h"
#include "context_712.h"     // eip712_context_deinit
#include "path.h"            // path_get_root_type
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"
#include "commands_712.h"
#include "common_ui.h"
#include "filtering.h"
#include "network.h"
#include "time_format.h"
#include "list.h"

#define AMOUNT_JOIN_FLAG_TOKEN  (1 << 0)
#define AMOUNT_JOIN_FLAG_VALUE  (1 << 1)
#define AMOUNT_JOIN_NAME_LENGTH 25

typedef struct amount_join {
    s_flist_node _list;
    // display name, NULL-terminated
    char name[AMOUNT_JOIN_NAME_LENGTH + 1];
    // indicates the steps the token join has gone through
    uint8_t flags;
    uint8_t token_idx;
    uint8_t value_length;
    uint8_t value[INT256_LENGTH];
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
    s_amount_join *joins;
    uint8_t idx;
    e_amount_join_state state;
} s_amount_context;

typedef struct filter_crc {
    s_flist_node _list;
    uint32_t value;
} s_filter_crc;

typedef struct {
    bool shown;
    bool end_reached;
    e_eip712_filtering_mode filtering_mode;
    uint8_t filters_to_process;
    uint8_t field_flags;
    uint8_t structs_to_review;
    s_amount_context amount;
    s_filter_crc *filters_crc;
    char *discarded_path;
    uint8_t tn_type_count;
    uint8_t tn_source_count;
    e_name_type tn_types[TN_TYPE_COUNT];
    e_name_source tn_sources[TN_SOURCE_COUNT];
    s_ui_712_pair *ui_pairs;
} t_ui_context;

static t_ui_context *ui_ctx = NULL;

// to be used as a \ref f_list_node_del
static void delete_filter_crc(s_filter_crc *fcrc) {
    app_mem_free(fcrc);
}

// to be used as a \ref f_list_node_del
static void delete_ui_pair(s_ui_712_pair *pair) {
    if (pair->key != NULL) app_mem_free(pair->key);
    if (pair->value != NULL) app_mem_free(pair->value);
    app_mem_free(pair);
}

// to be used as a \ref f_list_node_del
static void delete_amount_join(s_amount_join *join) {
    app_mem_free(join);
}

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
bool ui_712_review_struct(const s_struct_712 *struct_ptr) {
    const char *struct_name;
    const char *title = "Review struct";

    if (ui_ctx == NULL) {
        return false;
    }

    ui_712_set_title(title, strlen(title));
    if ((struct_name = struct_ptr->name) != NULL) {
        ui_712_set_value(struct_name, strlen(struct_name));
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
                              const s_struct_712_field *field_ptr) {
    uint256_t value256;
    uint128_t value128;
    int32_t value32;
    int16_t value16;

    // no reason for an integer to be received over multiple chunks
    if (!first) {
        return false;
    }
    switch (field_ptr->type_size * 8) {
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

static s_amount_join *get_amount_join(uint8_t token_idx) {
    s_amount_join *tmp;
    s_amount_join *new;

    for (tmp = ui_ctx->amount.joins; tmp != NULL;
         tmp = (s_amount_join *) ((s_flist_node *) tmp)->next) {
        if (tmp->token_idx == token_idx) break;
    }
    if (tmp != NULL) return tmp;

    // does not exist, create it
    if ((new = app_mem_alloc(sizeof(*new))) == NULL) {
        return NULL;
    }
    explicit_bzero(new, sizeof(*new));
    new->token_idx = token_idx;

    flist_push_back((s_flist_node **) &ui_ctx->amount.joins, (s_flist_node *) new);
    return new;
}

/**
 * Format given data as an amount with its ticker and value with correct decimals
 *
 * @return whether it was successful or not
 */
static bool ui_712_format_amount_join(void) {
    const tokenDefinition_t *token = NULL;
    s_amount_join *amount_join;

    if (tmpCtx.transactionContext.assetSet[ui_ctx->amount.idx]) {
        token = &tmpCtx.transactionContext.extraInfo[ui_ctx->amount.idx].token;
    }
    if ((amount_join = get_amount_join(ui_ctx->amount.idx)) == NULL) {
        return false;
    }
    if ((amount_join->value_length == INT256_LENGTH) &&
        ismaxint(amount_join->value, amount_join->value_length)) {
        strlcpy(strings.tmp.tmp, "Unlimited ", sizeof(strings.tmp.tmp));
        strlcat(strings.tmp.tmp,
                (token != NULL) ? token->ticker : g_unknown_ticker,
                sizeof(strings.tmp.tmp));
    } else {
        if (!amountToString(amount_join->value,
                            amount_join->value_length,
                            (token != NULL) ? token->decimals : 0,
                            (token != NULL) ? token->ticker : g_unknown_ticker,
                            strings.tmp.tmp,
                            sizeof(strings.tmp.tmp))) {
            return false;
        }
    }
    ui_ctx->field_flags |= UI_712_FIELD_SHOWN;
    ui_712_set_title(amount_join->name, strlen(amount_join->name));
    flist_remove((s_flist_node **) &ui_ctx->amount.joins,
                 (s_flist_node *) amount_join,
                 (f_list_node_del) delete_amount_join);
    return true;
}

/**
 * Simply mark the current amount-join's token address as received
 */
bool amount_join_set_token_received(void) {
    s_amount_join *amount_join = get_amount_join(ui_ctx->amount.idx);

    if (amount_join == NULL) return false;
    amount_join->flags |= AMOUNT_JOIN_FLAG_TOKEN;
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
    const tokenDefinition_t *token = NULL;
    s_amount_join *amount_join;

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
            if (!amount_join_set_token_received()) {
                return false;
            }
            break;

        case AMOUNT_JOIN_STATE_VALUE:
            if ((amount_join = get_amount_join(ui_ctx->amount.idx)) == NULL) {
                return false;
            }
            memcpy(amount_join->value, data, length);
            amount_join->value_length = length;
            amount_join->flags |= AMOUNT_JOIN_FLAG_VALUE;
            break;

        default:
            return false;
    }
    return true;
}

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

/**
 * Format given data as a human-readable date/time representation
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @param[in] field_ptr pointer to the new struct field
 * @return whether it was successful or not
 */
static bool ui_712_format_datetime(const uint8_t *data,
                                   uint8_t length,
                                   const s_struct_712_field *field_ptr) {
    time_t timestamp;

    if ((length >= field_ptr->type_size) && ismaxint((uint8_t *) data, length)) {
        snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "Unlimited");
        return true;
    }
    timestamp = u64_from_BE(data, length);
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
bool ui_712_feed_to_display(const s_struct_712_field *field_ptr,
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
        switch (field_ptr->type) {
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

        s_amount_join *amount_join = get_amount_join(ui_ctx->amount.idx);
        if (amount_join == NULL) {
            return false;
        }
        if (amount_join->flags == (AMOUNT_JOIN_FLAG_TOKEN | AMOUNT_JOIN_FLAG_VALUE)) {
            if (!ui_712_format_amount_join()) {
                return false;
            }
        }
    }

    if (ui_ctx->field_flags & UI_712_DATETIME) {
        if (!ui_712_format_datetime(data, length, field_ptr)) {
            return false;
        }
    }

    if (ui_ctx->field_flags & UI_712_TRUSTED_NAME) {
        if (!ui_712_format_trusted_name(data, length)) {
            return false;
        }
    }

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
    if (ui_ctx != NULL) {
        ui_712_deinit();
        return false;
    }

    if ((ui_ctx = app_mem_alloc(sizeof(*ui_ctx)))) {
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
    if (ui_ctx != NULL) {
        app_mem_free(ui_ctx);
        if (ui_ctx->filters_crc != NULL)
            flist_clear((s_flist_node **) &ui_ctx->filters_crc,
                        (f_list_node_del) &delete_filter_crc);
        if (ui_ctx->ui_pairs != NULL)
            flist_clear((s_flist_node **) &ui_ctx->ui_pairs, (f_list_node_del) &delete_ui_pair);
        if (ui_ctx->amount.joins != NULL)
            flist_clear((s_flist_node **) &ui_ctx->amount.joins,
                        (f_list_node_del) &delete_amount_join);
        ui_ctx = NULL;
    }
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
    uint8_t filter_count = 0;

    for (const s_filter_crc *tmp = ui_ctx->filters_crc; tmp != NULL;
         tmp = (s_filter_crc *) ((s_flist_node *) tmp)->next)
        filter_count += 1;
    return ui_ctx->filters_to_process - filter_count;
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

void ui_712_token_join_prepare_addr_check(uint8_t index) {
    ui_ctx->amount.idx = index;
    ui_ctx->amount.state = AMOUNT_JOIN_STATE_TOKEN;
}

bool ui_712_token_join_prepare_amount(uint8_t index, const char *name, uint8_t name_length) {
    s_amount_join *amount_join = get_amount_join(index);
    uint8_t cpy_len;

    if (amount_join == NULL) {
        return false;
    }
    cpy_len = MIN(sizeof(amount_join->name) - 1, name_length);
    ui_ctx->amount.idx = index;
    ui_ctx->amount.state = AMOUNT_JOIN_STATE_VALUE;
    memcpy(amount_join->name, name, cpy_len);
    amount_join->name[cpy_len] = '\0';
    return true;
}

/**
 * Set UI pair key to the raw JSON key
 *
 * @param[in] field_ptr pointer to the field
 * @return whether it was successful or not
 */
bool ui_712_show_raw_key(const s_struct_712_field *field_ptr) {
    const char *key;

    if ((key = field_ptr->key_name) == NULL) {
        return false;
    }

    if (ui_712_field_shown() && !(ui_ctx->field_flags & UI_712_FIELD_NAME_PROVIDED)) {
        ui_712_set_title(key, strlen(key));
    }
    return true;
}

/**
 * Push a new filter path
 *
 * @param[in] path_crc CRC of the filter path
 * @return whether it was successful or not
 */
bool ui_712_push_new_filter_path(uint32_t path_crc) {
    s_filter_crc *tmp;
    s_filter_crc *new_crc;
    uint8_t filter_count = 0;

    // check if already present
    for (tmp = ui_ctx->filters_crc; tmp != NULL;
         tmp = (s_filter_crc *) ((s_flist_node *) tmp)->next) {
        if (tmp->value == path_crc) {
            PRINTF("EIP-712 path CRC (%x) already found!\n", path_crc);
            return true;
        }
        filter_count += 1;
    }

    if (filter_count >= ui_ctx->filters_to_process) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    // allocate it
    if ((new_crc = app_mem_alloc(sizeof(*new_crc))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    explicit_bzero(new_crc, sizeof(*new_crc));
    new_crc->value = path_crc;

    PRINTF("Pushing new EIP-712 path CRC (%x)\n", path_crc);
    flist_push_back((s_flist_node **) &ui_ctx->filters_crc, (s_flist_node *) new_crc);
    return true;
}

/**
 * Set a discarded filter path
 *
 * @param[in] path the given filter path
 * @param[in] length the path length
 * @return whether it was successful or not
 */
bool ui_712_set_discarded_path(const char *path, uint8_t length) {
    if (ui_ctx->discarded_path != NULL) {
        return false;
    }
    if ((ui_ctx->discarded_path = app_mem_alloc(length + 1)) == NULL) {
        return false;
    }
    memcpy(ui_ctx->discarded_path, path, length);
    ui_ctx->discarded_path[length] = '\0';
    return true;
}

/**
 * Get the discarded filter path
 *
 * @return filter path
 */
const char *ui_712_get_discarded_path(void) {
    return ui_ctx->discarded_path;
}

void ui_712_clear_discarded_path(void) {
    if (ui_ctx->discarded_path != NULL) {
        app_mem_free(ui_ctx->discarded_path);
        ui_ctx->discarded_path = NULL;
    }
}

void ui_712_set_trusted_name_requirements(uint8_t type_count,
                                          const e_name_type *types,
                                          uint8_t source_count,
                                          const e_name_source *sources) {
    ui_ctx->tn_type_count = type_count;
    memcpy(ui_ctx->tn_types, types, type_count);
    ui_ctx->tn_source_count = source_count;
    memcpy(ui_ctx->tn_sources, sources, source_count);
}

const s_ui_712_pair *ui_712_get_pairs(void) {
    return ui_ctx->ui_pairs;
}

bool ui_712_push_new_pair(const char *key, const char *value) {
    s_ui_712_pair *new_pair;

    // allocate pair
    if ((new_pair = app_mem_alloc(sizeof(*new_pair))) == NULL) {
        return false;
    }
    explicit_bzero(new_pair, sizeof(*new_pair));

    flist_push_back((s_flist_node **) &ui_ctx->ui_pairs, (s_flist_node *) new_pair);

    if ((new_pair->key = app_mem_strdup(key)) == NULL) {
        return false;
    }

    if ((new_pair->value = app_mem_strdup(value)) == NULL) {
        return false;
    }
    return true;
}

void ui_712_delete_pairs(size_t keep) {
    size_t size;

    size = flist_size((s_flist_node **) &ui_ctx->ui_pairs);
    if (size > 0) {
        while (size > keep) {
            flist_pop_front((s_flist_node **) &ui_ctx->ui_pairs, (f_list_node_del) &delete_ui_pair);
            size -= 1;
        }
    }
}
