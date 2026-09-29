#include "eip712_v1_ui_logic.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "os_io.h"
#include "format.h"
#include "common_utils.h"  // uint256_to_decimal
#include "common_712.h"
#include "eip712_v1_context.h"  // eip712_v1_context
#include "typed_data.h"
#include "common_ui.h"
#include "eip712_v1_filtering.h"
#include "eip712_v1_parse.h"
#include "network.h"
#include "time_format.h"
#include "lists.h"
#include "ui_utils.h"
#include "utils.h"
#include "tx_ctx.h"  // g_parked_calldata
#include "read.h"    // read_u64_be
#include "token_info.h"
#ifdef HAVE_ADDRESS_BOOK
#include "handle_contacts.h"
#endif  // HAVE_ADDRESS_BOOK

#define N_OF_M_LENGTH 10  // enough to hold "nn of mm"

#define AMOUNT_JOIN_FLAG_TOKEN  (1 << 0)
#define AMOUNT_JOIN_FLAG_VALUE  (1 << 1)
#define AMOUNT_JOIN_NAME_LENGTH 25

typedef struct amount_join {
    flist_node_t _list;
    // display name, NULL-terminated
    char name[AMOUNT_JOIN_NAME_LENGTH + 1];
    // indicates the steps the token join has gone through
    uint8_t flags;
    uint8_t id;
    uint8_t address[ADDRESS_LENGTH];
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
#define UI_712_CALLDATA            (1 << 5)

typedef struct {
    s_amount_join *joins;
    uint8_t current_id;
    e_amount_join_state state;
} s_amount_context;

typedef struct filter_crc {
    flist_node_t _list;
    uint32_t value;
} s_filter_crc;

typedef struct {
    bool end_reached;
    e_eip712_filtering_mode filtering_mode;
    uint8_t filters_to_process;
    bool message_info_received;
    uint8_t field_flags;
    s_amount_context amount;
    s_filter_crc *filters_crc;
    // paths of fields completed without any filter, in full filtering mode
    s_filter_crc *unfiltered_crc;
    char *discarded_path;
    uint8_t tn_type_count;
    uint8_t tn_source_count;
    e_name_type tn_types[TN_TYPE_COUNT];
    e_name_source tn_sources[TN_SOURCE_COUNT];
    s_ui_712_pair *ui_pairs;
    s_eip712_calldata_info *calldata_info;
    uint8_t calldata_index;
    bool filtering_broken;
} t_ui_context;

static t_ui_context *ui_ctx = NULL;

// to be used as a \ref f_list_node_del
static void delete_filter_crc(s_filter_crc *fcrc) {
    APP_MEM_FREE(fcrc);
}

// to be used as a \ref f_list_node_del
static void delete_ui_pair(s_ui_712_pair *pair) {
    APP_MEM_FREE(pair->key);
    APP_MEM_FREE(pair->value);
    APP_MEM_FREE(pair);
}

// to be used as a \ref f_list_node_del
static void delete_amount_join(s_amount_join *join) {
    APP_MEM_FREE(join);
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
        ret = true;
#else
        if (N_storage.verbose_eip712 || (v1_get_root_type() == ROOT_DOMAIN)) {
            ret = true;
        }
#endif
    } else {  // EIP712_FILTERING_FULL
        if (ui_ctx->field_flags & UI_712_FIELD_SHOWN) {
            ret = true;
        }
    }
    return ret;
}

/**
 * Set a new intent for the EIP-712 SafeBatch transaction
 *
 * @return whether it was successful or not
 */
bool ui_712_set_intent(void) {
    s_ui_712_pair *new_pair = NULL;
    const char *title = "Review transaction";
    size_t title_length = strlen(title);

    // Allocate memory for the new pair
    if (APP_MEM_CALLOC((void **) &new_pair, sizeof(*new_pair)) == false) {
        return false;
    }
    // Add it to the chained list
    flist_push_back((flist_node_t **) &ui_ctx->ui_pairs, (flist_node_t *) new_pair);

    // Allocate and copy the title
    if (APP_MEM_CALLOC((void **) &new_pair->key, title_length + 1) == false) {
        return false;
    }
    memcpy(new_pair->key, title, title_length);

    // Allocate and clear the intent buffer
    if (APP_MEM_CALLOC((void **) &new_pair->value, N_OF_M_LENGTH) == false) {
        return false;
    }

    // Mark it as an intent
    new_pair->start_intent = true;
    return true;
}

/**
 * Set a new title for the EIP-712 generic UX_STEP
 *
 * @param[in] str the new title
 * @param[in] length its length
 * @return whether it was successful or not
 */
bool ui_712_set_title(const char *str, size_t length) {
    s_ui_712_pair *new_pair = NULL;

    if (APP_MEM_CALLOC((void **) &new_pair, sizeof(*new_pair)) == false) {
        return false;
    }
    flist_push_back((flist_node_t **) &ui_ctx->ui_pairs, (flist_node_t *) new_pair);
    if ((new_pair->key = APP_MEM_ALLOC(length + 1)) == NULL) {
        return false;
    }
    memcpy(new_pair->key, str, length);
    new_pair->key[length] = '\0';
    return true;
}

/**
 * Set a new value for the EIP-712 generic UX_STEP
 *
 * @param[in] str the new value
 * @param[in] length its length
 * @return whether it was successful or not
 */
bool ui_712_set_value(const char *str, size_t length) {
    s_ui_712_pair *tmp = ui_ctx->ui_pairs;

    if (str == NULL) {
        return false;
    }
    if (tmp == NULL) {
        // No pairs created yet
        return false;
    }
    while (((flist_node_t *) tmp)->next != NULL) {
        tmp = (s_ui_712_pair *) ((flist_node_t *) tmp)->next;
    }
    if (tmp->value != NULL) {
        PRINTF("Value already exist for tag %s: %s\n", tmp->key, tmp->value);
        return false;
    }
    if ((tmp->value = APP_MEM_ALLOC(length + 1)) == NULL) {
        return false;
    }
    memcpy(tmp->value, str, length);
    tmp->value[length] = '\0';
    tmp->end_intent = validate_instruction_hash();
    if (tmp->end_intent) {
        PRINTF("[Intent] End\n");
    }
    return true;
}

/**
 * Send APDU reply continuing the filtering flow, or trigger final review if end reached.
 * Called after a filtering APDU has added its pairs to the list.
 *
 * @return whether it was successful or not
 */
bool ui_712_continue_or_finish(void) {
    if (appState != APP_STATE_SIGNING_EIP712) {
        if ((ui_ctx->filtering_mode == EIP712_FILTERING_BASIC) && !N_storage.dataAllowed &&
            !N_storage.verbose_eip712) {
            ui_error_blind_signing();
            return false;
        }
        if (!ui_712_start(ui_ctx->filtering_mode)) {
            return false;
        }
        io_seproxyhal_send_status(SWO_SUCCESS, 0, false, false);
    } else {
        if (ui_ctx->end_reached) {
            return ui_sign_712_v1(ui_ctx->filtering_mode);
        } else {
            io_seproxyhal_send_status(SWO_SUCCESS, 0, false, false);
            explicit_bzero(&strings, sizeof(strings));
        }
    }
    return true;
}

/**
 * Show the hash of the message on the generic UI step
 */
bool ui_712_message_hash(void) {
    const char *title = "Message hash";

    // to prevent showing
    // Message hash > Domain hash > Message hash
    // as the last three fields
    if (!N_storage.displayHash) {
        if (!ui_712_set_title(title, strlen(title))) {
            return false;
        }
        array_bytes_string(strings.tmp.tmp,
                           sizeof(strings.tmp.tmp),
                           tmpCtx.messageSigningContext712.messageHash,
                           CX_KECCAK_256_SIZE);
        if (!ui_712_set_value(strings.tmp.tmp, strlen(strings.tmp.tmp))) {
            return false;
        }
    }
    ui_ctx->end_reached = true;
    return ui_712_continue_or_finish();
}

/**
 * Format a given data as a string
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 */
static bool ui_712_format_str(const uint8_t *data, size_t length) {
    size_t max_len = sizeof(strings.tmp.tmp) - 1;
    size_t cur_len = strlen(strings.tmp.tmp);
    size_t available;
    size_t to_copy;

    // The value is hashed in full (length-delimited) but displayed as a C string:
    // an embedded NUL would hide the trailing bytes from the review while still
    // being signed.
    if (memchr(data, '\0', length) != NULL) {
        PRINTF("Error: EIP-712 string with embedded NUL\n");
        return false;
    }

    if (cur_len >= max_len) {
        // Buffer is full; any further byte is dropped and the display must say so.
        // Overwrite the last 3 bytes of content so the string length stays max_len
        // and a re-entry keeps this path instead of erasing the marker.
        strings.tmp.tmp[max_len] = '\0';
        if (length > 0) {
            memcpy(strings.tmp.tmp + max_len - 3, "...", 3);
        }
        return true;
    }

    available = max_len - cur_len;
    to_copy = MIN(available, length);

    memcpy(strings.tmp.tmp + cur_len, data, to_copy);
    strings.tmp.tmp[cur_len + to_copy] = '\0';

    // truncated - add ellipsis if we couldn't fit everything
    if (to_copy < length) {
        memcpy(strings.tmp.tmp + max_len - 3, "...", 3);
        strings.tmp.tmp[max_len] = '\0';
    }
    return true;
}

/**
 * Format a given data as a string representation of an address
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return if the formatting was successful
 */
static bool ui_712_format_addr(const uint8_t *data, size_t length) {
    // no reason for an address to be received over multiple chunks
    if (length != ADDRESS_LENGTH) {
        return false;
    }
    if (!getEthDisplayableAddress((uint8_t *) data,
                                  strings.tmp.tmp,
                                  sizeof(strings.tmp.tmp),
                                  g_chain_config->chain_id)) {
        return false;
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
static bool ui_712_format_bool(const uint8_t *data, size_t length) {
    size_t max_len = sizeof(strings.tmp.tmp) - 1;
    const char *true_str = "true";
    const char *false_str = "false";
    const char *str;

    if (length != 1) {
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
 * @return if the formatting was successful
 */
static bool ui_712_format_bytes(const uint8_t *data, size_t length) {
    size_t max_len = sizeof(strings.tmp.tmp) - 1;
    size_t cur_len = strlen(strings.tmp.tmp);

    memcpy(strings.tmp.tmp, "0x", MIN(max_len, 2));
    cur_len += 2;
    if (format_hex(data,
                   MIN((max_len - cur_len) / 2, length),
                   strings.tmp.tmp + cur_len,
                   max_len + 1 - cur_len) < 0) {
        return false;
    }
    // truncated
    if (((max_len - cur_len) / 2) < length) {
        memcpy(strings.tmp.tmp + max_len - 3, "...", 3);
    }
    return true;
}

/**
 * Format given data as a string representation of an integer
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @param[in] field_ptr pointer to the EIP-712 field
 * @return if the formatting was successful
 */
static bool ui_712_format_int(const uint8_t *data,
                              size_t length,
                              const s_struct_712_field *field_ptr) {
    if (!format_signed_int_be(data,
                              length,
                              field_ptr->type_size,
                              strings.tmp.tmp,
                              sizeof(strings.tmp.tmp))) {
        return false;
    }
    return true;
}

/**
 * Format given data as a string representation of an unsigned integer
 *
 * @param[in] data the data that needs formatting
 * @param[in] length its length
 * @return if the formatting was successful
 */
static bool ui_712_format_uint(const uint8_t *data, size_t length) {
    return uint256_to_decimal(data, length, strings.tmp.tmp, sizeof(strings.tmp.tmp));
}

static s_amount_join *get_amount_join(uint8_t id) {
    s_amount_join *tmp;
    s_amount_join *new;

    for (tmp = ui_ctx->amount.joins; tmp != NULL;
         tmp = (s_amount_join *) ((flist_node_t *) tmp)->next) {
        if (id == tmp->id) break;
    }
    if (tmp != NULL) return tmp;

    // does not exist, create it
    if ((new = APP_MEM_ALLOC(sizeof(*new))) != NULL) {
        explicit_bzero(new, sizeof(*new));
        new->id = id;
        flist_push_back((flist_node_t **) &ui_ctx->amount.joins, (flist_node_t *) new);
    }
    return new;
}

/**
 * Format given data as an amount with its ticker and value with correct decimals
 *
 * @return whether it was successful or not
 */
static bool ui_712_format_amount_join(const s_amount_join *amount_join) {
    const s_token_info *token_info;
    uint64_t domain_chain_id;

    if (!td_get_domain_chain_id(&domain_chain_id)) {
        return false;
    }

    token_info = get_matching_token_info(&domain_chain_id, amount_join->address);
    if (ismaxint(amount_join->value, sizeof(amount_join->value))) {
        strlcpy(strings.tmp.tmp, "Unlimited ", sizeof(strings.tmp.tmp));
        strlcat(strings.tmp.tmp,
                (token_info != NULL) ? token_info->ticker : g_unknown_ticker,
                sizeof(strings.tmp.tmp));
    } else {
        if (!amountToString(amount_join->value,
                            sizeof(amount_join->value),
                            (token_info != NULL) ? token_info->decimals : 0,
                            (token_info != NULL) ? token_info->ticker : g_unknown_ticker,
                            strings.tmp.tmp,
                            sizeof(strings.tmp.tmp))) {
            return false;
        }
    }
    ui_ctx->field_flags |= UI_712_FIELD_SHOWN;
    if (!ui_712_set_title(amount_join->name, strlen(amount_join->name))) {
        return false;
    }
    flist_remove((flist_node_t **) &ui_ctx->amount.joins,
                 (flist_node_t *) amount_join,
                 (f_list_node_del) delete_amount_join);
    return true;
}

/**
 * Set the current amount-join's token address and mark it as received
 */
bool ui_712_set_amount_join_token_addr(const uint8_t *addr) {
    s_amount_join *amount_join = get_amount_join(ui_ctx->amount.current_id);

    if (amount_join == NULL) return false;
    // A half already received must not be overwritten before being displayed
    if (amount_join->flags & AMOUNT_JOIN_FLAG_TOKEN) return false;
    memcpy(amount_join->address, addr, sizeof(amount_join->address));
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
static bool update_amount_join(s_amount_join *amount_join, const uint8_t *data, uint8_t length) {
    switch (ui_ctx->amount.state) {
        case AMOUNT_JOIN_STATE_TOKEN:
            if ((amount_join->flags & AMOUNT_JOIN_FLAG_TOKEN) ||
                (length > sizeof(amount_join->address))) {
                return false;
            }
            buf_shrink_expand(data, length, amount_join->address, sizeof(amount_join->address));
            amount_join->flags |= AMOUNT_JOIN_FLAG_TOKEN;
            break;

        case AMOUNT_JOIN_STATE_VALUE:
            if ((amount_join->flags & AMOUNT_JOIN_FLAG_VALUE) ||
                (length > sizeof(amount_join->value))) {
                return false;
            }
            buf_shrink_expand(data, length, amount_join->value, sizeof(amount_join->value));
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
static bool ui_712_format_trusted_name(const uint8_t *data, size_t length) {
    const s_trusted_name *trusted_name;
    uint64_t domain_chain_id;

    if (length != ADDRESS_LENGTH) {
        return false;
    }
    if (!td_get_domain_chain_id(&domain_chain_id)) {
        return false;
    }
#ifdef HAVE_ADDRESS_BOOK
    // Address Book contacts take priority over Trusted Names for account addresses.
    for (uint8_t i = 0; i < ui_ctx->tn_type_count; i++) {
        if (ui_ctx->tn_types[i] == TN_TYPE_ACCOUNT) {
            const s_ab_contact *ab_contact = get_address_book_contact(domain_chain_id, data);
            if ((ab_contact != NULL) &&
                is_displayable_string(
                    ab_contact->contact_name,
                    strnlen(ab_contact->contact_name, sizeof(ab_contact->contact_name)))) {
                strlcpy(strings.tmp.tmp, ab_contact->contact_name, sizeof(strings.tmp.tmp));
                return true;
            }
            break;
        }
    }
#endif  // HAVE_ADDRESS_BOOK
    // Fall back to a Trusted Name; if none matches, strings.tmp.tmp retains the raw
    // hex address already written by ui_712_format_addr().
    if ((trusted_name = get_trusted_name(ui_ctx->tn_type_count,
                                         ui_ctx->tn_types,
                                         ui_ctx->tn_source_count,
                                         ui_ctx->tn_sources,
                                         &domain_chain_id,
                                         data)) != NULL) {
        strlcpy(strings.tmp.tmp, trusted_name->name, sizeof(strings.tmp.tmp));
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
                                   size_t length,
                                   const s_struct_712_field *field_ptr) {
    time_t timestamp;
    uint64_t value;

    _Static_assert(sizeof(time_t) == sizeof(int64_t), "time_t is not 64-bit");

    if ((length >= field_ptr->type_size) && ismaxint((uint8_t *) data, length)) {
        snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "Unlimited");
        return true;
    }
    // u64_from_BE() reads from the front of the buffer; for a wide EIP-712
    // type (e.g. uint256), the trailing 8 bytes carry the value and the
    // leading bytes are zero-padding. Strip the padding before decoding so
    // the displayed timestamp matches what is being hashed, and refuse the
    // value entirely if it doesn't fit in a uint64.
    if (length > sizeof(uint64_t)) {
        uint8_t leading = length - sizeof(uint64_t);
        if (!is_zeroes_buffer(data, leading)) {
            PRINTF("Datetime value too large for time_t\n");
            return false;
        }
        data += leading;
        length = sizeof(uint64_t);
    }
    value = u64_from_BE(data, length);
    // The value is rendered through time_t, which is signed: one that it cannot
    // represent would be shown as a different (possibly negative) date than the
    // one committed to the EIP-712 hash.
    if (value > (uint64_t) INT64_MAX) {
        PRINTF("Datetime value outside the representable time_t range\n");
        return false;
    }
    timestamp = (time_t) value;
    return time_format_to_utc(&timestamp, strings.tmp.tmp, sizeof(strings.tmp.tmp));
}

static bool ui_712_set_intent_field(const char *value) {
    const char key[] = "Transaction type";

    return ui_712_set_title(key, strlen(key)) && ui_712_set_value(value, strlen(value));
}

static bool handle_fallback_empty_calldata(const s_eip712_calldata_info *calldata_info) {
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint64_t chain_id;
    uint8_t decimals;
    const char *ticker;

    if (calldata_info->amount_state == CALLDATA_INFO_PARAM_SET) {
        if (!ui_712_set_intent_field("Send")) {
            return false;
        }

        if (calldata_info->chain_id != 0) {
            chain_id = calldata_info->chain_id;
        } else {
            if (!td_get_domain_chain_id(&chain_id)) {
                return false;
            }
        }

        ticker = get_displayable_ticker(&chain_id, g_chain_config, true);
        decimals = WEI_TO_ETHER;
        if (!amountToString(calldata_info->amount,
                            sizeof(calldata_info->amount),
                            decimals,
                            ticker,
                            buf,
                            buf_size)) {
            return false;
        }
        if (!ui_712_set_title("Amount", 6) || !ui_712_set_value(buf, strlen(buf))) {
            return false;
        }
    } else {
        if (!ui_712_set_intent_field("Empty transaction")) {
            return false;
        }
    }

    e_name_type types[] = {TN_TYPE_ACCOUNT};
    e_name_source sources[] = {TN_SOURCE_ENS, TN_SOURCE_LAB, TN_SOURCE_MAB};
    const s_trusted_name *trusted_name;

    if (!ui_712_set_title("To", 2)) {
        return false;
    }
#ifdef HAVE_ADDRESS_BOOK
    // Address Book contacts take priority over Trusted Names for the callee address.
    {
        const s_ab_contact *ab_contact =
            get_address_book_contact(calldata_info->chain_id, calldata_info->callee);
        if ((ab_contact != NULL) &&
            is_displayable_string(
                ab_contact->contact_name,
                strnlen(ab_contact->contact_name, sizeof(ab_contact->contact_name)))) {
            if (!ui_712_set_value(ab_contact->contact_name, strlen(ab_contact->contact_name))) {
                return false;
            }
            return true;
        }
    }
#endif  // HAVE_ADDRESS_BOOK
    // Fall back to a Trusted Name, then to the raw checksummed hex address.
    if ((trusted_name = get_trusted_name(ARRAYLEN(types),
                                         types,
                                         ARRAYLEN(sources),
                                         sources,
                                         &calldata_info->chain_id,
                                         calldata_info->callee)) != NULL) {
        if (!ui_712_set_value(trusted_name->name, strlen(trusted_name->name))) {
            return false;
        }
    } else {
        if (!getEthDisplayableAddress(calldata_info->callee,
                                      buf,
                                      buf_size,
                                      calldata_info->chain_id)) {
            return false;
        }
        if (!ui_712_set_value(buf, strlen(buf))) {
            return false;
        }
    }
    return true;
}

static bool update_calldata_value(const uint8_t *data,
                                  uint8_t length,
                                  const uint16_t *complete_length,
                                  bool last,
                                  s_eip712_calldata_info *calldata_info) {
    const uint8_t *selector = NULL;
    size_t calldata_size;

    if (calldata_info->value_state != CALLDATA_INFO_PARAM_UNSET) return false;
    if (complete_length != NULL) {
        calldata_size = *complete_length;
        // A zero-argument call with a separately-provided selector is still a call:
        // the calldata object must exist so the selector is not dropped.
        if ((calldata_size > 0) || (calldata_info->selector_state != CALLDATA_INFO_PARAM_NONE)) {
            if (calldata_info->selector_state == CALLDATA_INFO_PARAM_NONE) {
                if ((length < CALLDATA_SELECTOR_SIZE) || (calldata_size < CALLDATA_SELECTOR_SIZE)) {
                    return false;
                }
                selector = data;
                data += CALLDATA_SELECTOR_SIZE;
                length -= CALLDATA_SELECTOR_SIZE;
                calldata_size -= CALLDATA_SELECTOR_SIZE;
            } else if (calldata_info->selector_state == CALLDATA_INFO_PARAM_SET) {
                selector = calldata_info->selector;
            }
            if ((g_parked_calldata = calldata_init(calldata_size, selector)) == NULL) {
                return false;
            }
        }
    }
    if (g_parked_calldata != NULL) {
        if (!calldata_append(g_parked_calldata, data, length)) {
            return false;
        }
    } else {
        // won't receive a TX info & descriptors about a non-existent calldata
        calldata_info->processed = true;
    }
    if (last) calldata_info->value_state = CALLDATA_INFO_PARAM_SET;
    return true;
}

static bool update_calldata_callee(const uint8_t *data,
                                   uint8_t length,
                                   s_eip712_calldata_info *calldata_info) {
    if (calldata_info->callee_state != CALLDATA_INFO_PARAM_UNSET) return false;
    if (length > sizeof(calldata_info->callee)) {
        return false;
    }
    buf_shrink_expand(data, length, calldata_info->callee, sizeof(calldata_info->callee));
    calldata_info->callee_state = CALLDATA_INFO_PARAM_SET;
    return true;
}

static bool update_calldata_chain_id(const uint8_t *data,
                                     uint8_t length,
                                     s_eip712_calldata_info *calldata_info) {
    uint8_t chain_id_buf[sizeof(uint64_t)];

    if (calldata_info->chain_id_state != CALLDATA_INFO_PARAM_UNSET) return false;
    if (length > sizeof(chain_id_buf)) {
        return false;
    }
    buf_shrink_expand(data, length, chain_id_buf, sizeof(chain_id_buf));
    calldata_info->chain_id = read_u64_be(chain_id_buf, 0);
    calldata_info->chain_id_state = CALLDATA_INFO_PARAM_SET;
    return true;
}

static bool update_calldata_selector(const uint8_t *data,
                                     uint8_t length,
                                     s_eip712_calldata_info *calldata_info) {
    if (calldata_info->selector_state != CALLDATA_INFO_PARAM_UNSET) return false;
    if (length > sizeof(calldata_info->selector)) {
        return false;
    }
    buf_shrink_expand(data, length, calldata_info->selector, sizeof(calldata_info->selector));
    calldata_info->selector_state = CALLDATA_INFO_PARAM_SET;
    if (calldata_info->value_state == CALLDATA_INFO_PARAM_SET) {
        if (!calldata_set_selector(g_parked_calldata, calldata_info->selector)) {
            return false;
        }
    }
    return true;
}

static bool update_calldata_amount(const uint8_t *data,
                                   uint8_t length,
                                   s_eip712_calldata_info *calldata_info) {
    if (calldata_info->amount_state != CALLDATA_INFO_PARAM_UNSET) return false;
    if (length > sizeof(calldata_info->amount)) {
        return false;
    }
    buf_shrink_expand(data, length, calldata_info->amount, sizeof(calldata_info->amount));
    calldata_info->amount_state = CALLDATA_INFO_PARAM_SET;
    return true;
}

static bool update_calldata_spender(const uint8_t *data,
                                    uint8_t length,
                                    s_eip712_calldata_info *calldata_info) {
    if (calldata_info->spender_state != CALLDATA_INFO_PARAM_UNSET) return false;
    if (length > sizeof(calldata_info->spender)) {
        return false;
    }
    buf_shrink_expand(data, length, calldata_info->spender, sizeof(calldata_info->spender));
    calldata_info->spender_state = CALLDATA_INFO_PARAM_SET;
    return true;
}

static bool update_calldata(const uint8_t *data,
                            uint8_t length,
                            const uint16_t *complete_length,
                            bool last) {
    s_eip712_calldata_info *calldata_info = get_calldata_info(ui_ctx->calldata_index);

    if (calldata_info == NULL) return false;
    switch (calldata_info->state) {
        case EIP712_CALLDATA_VALUE:
            if (!update_calldata_value(data, length, complete_length, last, calldata_info))
                return false;
            break;
        case EIP712_CALLDATA_CALLEE:
            if (!update_calldata_callee(data, length, calldata_info)) return false;
            break;
        case EIP712_CALLDATA_CHAIN_ID:
            if (!update_calldata_chain_id(data, length, calldata_info)) return false;
            break;
        case EIP712_CALLDATA_SELECTOR:
            if (!update_calldata_selector(data, length, calldata_info)) return false;
            break;
        case EIP712_CALLDATA_AMOUNT:
            if (!update_calldata_amount(data, length, calldata_info)) return false;
            break;
        case EIP712_CALLDATA_SPENDER:
            if (!update_calldata_spender(data, length, calldata_info)) return false;
            break;
        default:
            return false;
    }
    if (calldata_info_all_received(calldata_info)) {
        if (g_parked_calldata == NULL) {
            if (!handle_fallback_empty_calldata(calldata_info)) return false;
        } else {
            if (!tx_ctx_init(g_parked_calldata,
                             calldata_info->spender,
                             calldata_info->callee,
                             calldata_info->amount,
                             &calldata_info->chain_id)) {
                calldata_delete(g_parked_calldata);
                g_parked_calldata = NULL;
                return false;
            }
            g_parked_calldata = NULL;
        }
    }
    return true;
}

/**
 * Accumulate a fully-received leaf value into the UI display system.
 * impl APDUs never trigger display directly. Runs only in EIP712_FILTERING_FULL
 * mode. Resets field flags when done.
 */
bool ui_712_accumulate_value(const s_struct_712_field *field_ptr,
                             const uint8_t *data,
                             size_t length) {
    if (ui_ctx == NULL) {
        return true;
    }
    if (ui_ctx->filtering_mode != EIP712_FILTERING_FULL) {
        return true;
    }

    // Clear accumulation buffer for fresh formatting.
    explicit_bzero(strings.tmp.tmp, sizeof(strings.tmp.tmp));

    if (ui_712_field_shown()) {
        switch (field_ptr->type) {
            case TYPE_SOL_STRING:
                if (!ui_712_format_str(data, length)) {
                    return false;
                }
                break;
            case TYPE_SOL_ADDRESS:
                if (!ui_712_format_addr(data, length)) {
                    return false;
                }
                break;
            case TYPE_SOL_BOOL:
                if (!ui_712_format_bool(data, length)) {
                    return false;
                }
                break;
            case TYPE_SOL_BYTES_FIX:
            case TYPE_SOL_BYTES_DYN:
                if (!ui_712_format_bytes(data, length)) {
                    return false;
                }
                break;
            case TYPE_SOL_INT:
                if (!ui_712_format_int(data, length, field_ptr)) {
                    return false;
                }
                break;
            case TYPE_SOL_UINT:
                if (!ui_712_format_uint(data, length)) {
                    return false;
                }
                break;
            default:
                break;
        }
    }

    if (ui_ctx->field_flags & UI_712_AMOUNT_JOIN) {
        s_amount_join *amount_join = get_amount_join(ui_ctx->amount.current_id);
        if (amount_join == NULL) {
            return false;
        }
        if (!update_amount_join(amount_join, data, (uint8_t) MIN(length, 32))) {
            return false;
        }
        if (amount_join->flags == (AMOUNT_JOIN_FLAG_TOKEN | AMOUNT_JOIN_FLAG_VALUE)) {
            if (!ui_712_format_amount_join(amount_join)) {
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

    if (ui_ctx->field_flags & UI_712_CALLDATA) {
        uint16_t remaining = length;
        const uint8_t *ptr = data;
        uint16_t total = length;
        bool first = true;
        // do-while ensures an empty calldata field (length == 0) still triggers update_calldata
        // once
        do {
            uint8_t chunk = (uint8_t) MIN(remaining, 0xFF);
            bool last_chunk = (remaining <= 0xFF);
            if (!update_calldata(ptr, chunk, first ? &total : NULL, last_chunk)) {
                return false;
            }
            ptr += chunk;
            remaining -= chunk;
            first = false;
        } while (remaining > 0);
    }

    if (ui_712_field_shown()) {
        if (!ui_712_set_value(strings.tmp.tmp, strlen(strings.tmp.tmp))) {
            return false;
        }
    }
    ui_712_field_flags_reset();
    return true;
}

static bool crc_list_contains(const s_filter_crc *list, uint32_t crc) {
    for (; list != NULL; list = (s_filter_crc *) ((flist_node_t *) list)->next) {
        if (list->value == crc) {
            return true;
        }
    }
    return false;
}

/**
 * Check filter enforcement for the field about to be finalized (path still points at it).
 *
 * In full filtering mode, paths are only identified by their schema form (".[]" for array
 * elements), so every occurrence of an array element shares one registered filter path.
 * Once a path is registered, every further occurrence must come with a freshly verified
 * filter (any filter sets at least one field flag). Paths of unfiltered occurrences are
 * recorded so that a filter registered for them afterwards is caught as well.
 */
void ui_712_check_field_filtering(void) {
    uint32_t path_crc = 0;
    s_filter_crc *new_crc;

    if ((ui_ctx == NULL) || (ui_ctx->filtering_mode != EIP712_FILTERING_FULL)) {
        return;
    }
    if (!filtering_compute_current_path_crc(&path_crc)) {
        ui_ctx->filtering_broken = true;
        return;
    }
    if (crc_list_contains(ui_ctx->filters_crc, path_crc)) {
        // calldata-filtered fields are registered but intentionally never SHOWN,
        // only their decoded form is displayed
        if (ui_ctx->field_flags == 0) {
            PRINTF("EIP-712: filtered path 0x%x occurrence without filter!\n", path_crc);
            ui_ctx->filtering_broken = true;
        }
        return;
    }
    if ((ui_ctx->field_flags == 0) && !crc_list_contains(ui_ctx->unfiltered_crc, path_crc)) {
        if (APP_MEM_CALLOC((void **) &new_crc, sizeof(*new_crc)) == false) {
            ui_ctx->filtering_broken = true;
            return;
        }
        new_crc->value = path_crc;
        flist_push_back((flist_node_t **) &ui_ctx->unfiltered_crc, (flist_node_t *) new_crc);
    }
}

/**
 * Whether a registered filtered path was consumed without a fresh filter
 *
 * @return whether the filtering accounting was violated
 */
bool ui_712_filtering_broken(void) {
    return (ui_ctx != NULL) && ui_ctx->filtering_broken;
}

/**
 * Whether an amount join is still waiting for one of its two halves
 *
 * A join is removed as soon as its token/value pair has been rendered, so anything
 * left in the list is a filtered amount that never made it to the review.
 *
 * @return whether at least one amount join is still pending
 */
bool ui_712_pending_amount_joins(void) {
    return (ui_ctx != NULL) && (ui_ctx->amount.joins != NULL);
}

/**
 * Used to signal that we are done with reviewing the structs and we can now have
 * the option to approve or reject the signature
 */
bool ui_712_end_sign(void) {
    if (ui_ctx == NULL) {
        return false;
    }

#ifdef SCREEN_SIZE_WALLET
    {
#else
    if (N_storage.verbose_eip712 || (ui_ctx->filtering_mode == EIP712_FILTERING_FULL)) {
#endif
        ui_ctx->end_reached = true;
        return ui_sign_712_v1(ui_ctx->filtering_mode);
    }
    return true;
}

/**
 * Initializes the UI context structure in memory
 */
bool ui_712_init(void) {
    if (ui_ctx != NULL) {
        ui_712_deinit();
        return false;
    }

    if (APP_MEM_CALLOC((void **) &ui_ctx, sizeof(*ui_ctx)) == false) {
    } else {
        ui_712_set_filtering_mode(EIP712_FILTERING_BASIC);
        explicit_bzero(strings.tmp.tmp, sizeof(strings.tmp.tmp));
    }
    return ui_ctx != NULL;
}

static void delete_calldata_info(s_eip712_calldata_info *node) {
    APP_MEM_FREE(node);
}

/**
 * Deinit function that simply unsets the struct pointer to NULL
 */
void ui_712_deinit(void) {
    eip712_hash_strs_cleanup();
    if (ui_ctx != NULL) {
        if (ui_ctx->filters_crc != NULL) {
            flist_clear((flist_node_t **) &ui_ctx->filters_crc,
                        (f_list_node_del) &delete_filter_crc);
        }
        if (ui_ctx->unfiltered_crc != NULL) {
            flist_clear((flist_node_t **) &ui_ctx->unfiltered_crc,
                        (f_list_node_del) &delete_filter_crc);
        }
        if (ui_ctx->ui_pairs != NULL) {
            flist_clear((flist_node_t **) &ui_ctx->ui_pairs, (f_list_node_del) &delete_ui_pair);
        }
        if (ui_ctx->amount.joins != NULL) {
            flist_clear((flist_node_t **) &ui_ctx->amount.joins,
                        (f_list_node_del) &delete_amount_join);
        }
        if (ui_ctx->calldata_info != NULL) {
            flist_clear((flist_node_t **) &ui_ctx->calldata_info,
                        (f_list_node_del) &delete_calldata_info);
            gcs_cleanup();
        }
        ui_712_clear_discarded_path();
        APP_MEM_FREE_AND_NULL((void **) &ui_ctx);
    }
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
                       bool trusted_name,
                       bool calldata) {
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
    if (calldata) {
        ui_ctx->field_flags |= UI_712_CALLDATA;
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
    if (ui_ctx == NULL) {
        return EIP712_FILTERING_BASIC;
    }
    return ui_ctx->filtering_mode;
}

/**
 * Set the number of filters this message should process
 *
 * @param[in] count number of filters
 */
void ui_712_set_filters_count(uint8_t count) {
    ui_ctx->filters_to_process = count;
    ui_ctx->message_info_received = true;
}

/**
 * Get the number of filters left to process
 *
 * @return number of filters
 */
uint8_t ui_712_remaining_filters(void) {
    return ui_ctx->filters_to_process - flist_size((flist_node_t **) &ui_ctx->filters_crc);
}

bool ui_712_message_info_received(void) {
    return ui_ctx->message_info_received;
}

/**
 * Reset all the UI struct field flags
 */
void ui_712_field_flags_reset(void) {
    ui_ctx->field_flags = 0;
}

/**
 * Per-traversal state for verbose-mode formatting. Tracks whether the current
 * node is the tree root, so only the root struct (domain or message) gets a
 * "Review struct" screen.
 */
typedef struct {
    bool is_root;
} s_verbose_ctx;

/**
 * Formats a value tree node as a UI pair for verbose mode. VAL_ATOMIC leaves
 * become a key/value pair. VAL_STRUCT root nodes get a "Review struct" screen
 * (see s_verbose_ctx). Ignores VAL_ARRAY.
 * Shared body for format_node_for_verbose() and format_node_for_verbose_consuming().
 *
 * @param consume if true, frees the leaf's raw data right after formatting it,
 *                to reduce peak memory usage for large nested messages. Only
 *                safe for the message tree, and only after td_hash_pass() has
 *                already computed the message hash — the value can no longer
 *                be re-hashed once freed. Must be false for the domain tree:
 *                gating checks read domain leaves again later via
 *                td_get_domain_chain_id()/td_get_domain_contract_addr().
 */
static bool format_node_for_verbose_internal(const s_struct_712_value *node,
                                             void *context,
                                             bool consume) {
    s_verbose_ctx *ctx = context;

    if (node->kind == VAL_STRUCT) {
        if (ctx->is_root) {
            ctx->is_root = false;
#ifdef SCREEN_SIZE_WALLET
            bool show = true;
#else
            bool show = N_storage.verbose_eip712;
#endif
            if (show && (node->struct_type != NULL) && (node->struct_type->name != NULL)) {
                const char *title = "Review struct";
                if (!ui_712_set_title(title, strlen(title)) ||
                    !ui_712_set_value(node->struct_type->name, strlen(node->struct_type->name))) {
                    return false;
                }
            }
        }
        return true;
    }

    if (node->kind != VAL_ATOMIC) {
        return true;  // VAL_ARRAY: nothing to show directly
    }

    const s_struct_712_field *field = node->field;
    const uint8_t *data = node->data;
    size_t length = node->length;

    if (field == NULL || field->key_name == NULL) {
        return true;
    }

    // Set title
    if (!ui_712_set_title(field->key_name, strlen(field->key_name))) {
        return false;
    }

    // Clear temp buffer
    explicit_bzero(strings.tmp.tmp, sizeof(strings.tmp.tmp));

    // Format value based on type
    switch (field->type) {
        case TYPE_SOL_STRING:
            if (!ui_712_format_str(data, length)) {
                return false;
            }
            break;
        case TYPE_SOL_ADDRESS:
            if (!ui_712_format_addr(data, length)) {
                return false;
            }
            break;
        case TYPE_SOL_BOOL:
            if (!ui_712_format_bool(data, length)) {
                return false;
            }
            break;
        case TYPE_SOL_BYTES_FIX:
        case TYPE_SOL_BYTES_DYN:
            if (!ui_712_format_bytes(data, length)) {
                return false;
            }
            break;
        case TYPE_SOL_INT:
            if (!ui_712_format_int(data, length, field)) {
                return false;
            }
            break;
        case TYPE_SOL_UINT:
            if (!ui_712_format_uint(data, length)) {
                return false;
            }
            break;
        default:
            return true;  // Skip unknown types
    }

    // Set value. The flattened g_pairs display array is (re)built once, by the
    // caller, after the full domain/message traversal completes — not per-leaf.
    // Calling ui_712_push_pairs() here would redundantly free+reallocate the
    // whole array at a size one larger for every single leaf, which for large
    // nested messages both wastes cycles and fragments the heap needlessly.
    if (!ui_712_set_value(strings.tmp.tmp, strlen(strings.tmp.tmp))) {
        return false;
    }

    if (consume) {
        td_release_leaf_value(node);
    }
    return true;
}

/**
 * Visitor callback for verbose mode tree traversal (domain tree).
 */
static bool format_node_for_verbose(const s_struct_712_value *node, void *context) {
    return format_node_for_verbose_internal(node, context, false);
}

/**
 * Visitor callback for verbose mode tree traversal (message tree). See
 * format_node_for_verbose_internal()'s @p consume doc for when this is safe.
 */
static bool format_node_for_verbose_consuming(const s_struct_712_value *node, void *context) {
    return format_node_for_verbose_internal(node, context, true);
}

/**
 * Build UI pairs from value tree for verbose/no-filtering mode.
 * Walks domain (all devices).
 * Message fields: Stax/Flex/Apex always, Nano only if verbose_eip712 enabled.
 * Message hash for Nano non-verbose is handled separately by ui_712_message_hash().
 * Domain/Message hashes when displayHash enabled are added later by ui_712_end_sign.
 *
 * Message leaves are freed via format_leaf_for_verbose_consuming() right after
 * being displayed, to reduce peak memory usage for large nested messages —
 * safe here because this is only ever called from handle_eip712_v1_sign()
 * after td_hash_pass() has already computed the message hash. Domain leaves
 * are NOT freed this way: gating checks read them again later via
 * td_get_domain_chain_id()/td_get_domain_contract_addr().
 *
 * The flattened g_pairs display array is built explicitly after domain traversal
 * (needed since Nano non-verbose mode never traverses the message and never
 * reaches ui_sign_712_v1(), which is otherwise responsible for this) and again,
 * once, by ui_sign_712_v1()/ui_712_end_sign() after this function returns for
 * every other mode — never per-leaf during traversal itself.
 */
bool ui_712_populate_from_value_tree(void) {
    if (ui_ctx == NULL) {
        return false;
    }

    // Domain fields
    if (!td_traverse_domain(format_node_for_verbose, &(s_verbose_ctx) {.is_root = true})) {
        return false;
    }
    if (!ui_712_push_pairs()) {
        return false;
    }

#ifdef SCREEN_SIZE_WALLET
    // Stax/Flex/Apex: always show message fields
    if (!td_traverse_message(format_node_for_verbose_consuming,
                             &(s_verbose_ctx) {.is_root = true})) {
        return false;
    }
#else
    // Nano: show message fields only if raw messages enabled
    if (N_storage.verbose_eip712) {
        if (!td_traverse_message(format_node_for_verbose_consuming,
                                 &(s_verbose_ctx) {.is_root = true})) {
            return false;
        }
    }
    // Nano non-verbose: message hash is added by ui_712_message_hash() in handle_eip712_sign
#endif

    return true;
}

void ui_712_token_join_prepare_addr_check(uint8_t id) {
    ui_ctx->amount.current_id = id;
    ui_ctx->amount.state = AMOUNT_JOIN_STATE_TOKEN;
}

bool ui_712_token_join_prepare_amount(uint8_t id, const char *name, uint8_t name_length) {
    s_amount_join *amount_join = get_amount_join(id);
    uint8_t cpy_len;

    if (amount_join == NULL) {
        return false;
    }
    cpy_len = MIN(sizeof(amount_join->name) - 1, name_length);
    ui_ctx->amount.current_id = id;
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
        if (!ui_712_set_title(key, strlen(key))) {
            return false;
        }
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
         tmp = (s_filter_crc *) ((flist_node_t *) tmp)->next) {
        if (tmp->value == path_crc) {
            PRINTF("EIP-712 path CRC (%x) already found!\n", path_crc);
            return true;
        }
        filter_count += 1;
    }

    if (filter_count >= ui_ctx->filters_to_process) {
        return false;
    }
    if (crc_list_contains(ui_ctx->unfiltered_crc, path_crc)) {
        PRINTF("EIP-712: path 0x%x already had an occurrence without filter!\n", path_crc);
        ui_ctx->filtering_broken = true;
    }
    // allocate it
    if (APP_MEM_CALLOC((void **) &new_crc, sizeof(*new_crc)) == false) {
        return false;
    }
    new_crc->value = path_crc;

    PRINTF("Pushing new EIP-712 path CRC (%x)\n", path_crc);
    flist_push_back((flist_node_t **) &ui_ctx->filters_crc, (flist_node_t *) new_crc);
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
    if ((ui_ctx->discarded_path = APP_MEM_ALLOC(length + 1)) == NULL) {
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
    APP_MEM_FREE_AND_NULL((void **) &ui_ctx->discarded_path);
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

/**
 * Set the tag/value pairs for the review
 *
 */
bool ui_712_push_pairs(void) {
    size_t nbPairs;
    uint8_t pair = 0;
    s_ui_712_pair *tmp = NULL;
    uint8_t tx_idx = 0;

    // Keep the queued-pair count at its native width: narrowing it here would
    // wrap past the 8-bit pair counter and allocate fewer entries than the loop
    // below writes. ui_pairs_init() rejects anything the counter cannot hold.
    nbPairs = flist_size((flist_node_t **) &ui_ctx->ui_pairs);
    if (N_storage.displayHash) {
        nbPairs += 2;
    }

    if (!ui_pairs_init(nbPairs)) {
        return false;
    }
    // Initialize the tag/value pairs from the chain list
    tmp = ui_ctx->ui_pairs;
    while (tmp != NULL) {
        if (pair >= g_pairsList->nbPairs) {
            return false;
        }
        if (tmp->start_intent) {
            // Batch intermediate page
            tx_idx++;
            // Replace "nn of mm" placeholder, initialized in ui_712_set_intent()
            snprintf(tmp->value, N_OF_M_LENGTH, "%d of %d", tx_idx, txContext.batch_nb_tx);
            g_pairs[pair].centeredInfo = true;
        }
        g_pairs[pair].item = tmp->key;
        g_pairs[pair].value = tmp->value;
        pair++;
        if ((tmp->end_intent) && (txContext.batch_nb_tx > 1) && (pair < g_pairsList->nbPairs)) {
            // End of batch transaction : start next info on full page
            g_pairs[pair].forcePageStart = true;
        }
        tmp = (s_ui_712_pair *) ((flist_node_t *) tmp)->next;
    }

    if (N_storage.displayHash) {
        if ((pair + 1) >= g_pairsList->nbPairs) {
            return false;
        }
        // Prepare the pairs list with the hashes
        eip712_format_hash(pair);
        g_pairs[pair].forcePageStart = true;
    }
    return true;
}

void add_calldata_info(s_eip712_calldata_info *node) {
    flist_push_back((flist_node_t **) &ui_ctx->calldata_info, (flist_node_t *) node);
}

s_eip712_calldata_info *get_calldata_info(uint8_t index) {
    for (s_eip712_calldata_info *tmp = ui_ctx->calldata_info; tmp != NULL;
         tmp = (s_eip712_calldata_info *) ((flist_node_t *) tmp)->next) {
        if (index == tmp->index) {
            return tmp;
        }
    }
    return NULL;
}

s_eip712_calldata_info *get_current_calldata_info(void) {
    return get_calldata_info(ui_ctx->calldata_index);
}

bool all_calldata_info_processed(void) {
    for (const s_eip712_calldata_info *tmp = ui_ctx->calldata_info; tmp != NULL;
         tmp = (const s_eip712_calldata_info *) ((const flist_node_t *) tmp)->next) {
        if (!tmp->processed) return false;
    }
    return true;
}

void calldata_info_set_state(uint8_t index, e_eip712_calldata_state state) {
    s_eip712_calldata_info *calldata_info = get_calldata_info(index);

    ui_ctx->calldata_index = index;
    if (calldata_info != NULL) {
        calldata_info->state = state;
    }
}

bool calldata_info_all_received(const s_eip712_calldata_info *calldata_info) {
    if (calldata_info->value_state != CALLDATA_INFO_PARAM_SET) return false;
    if (calldata_info->callee_state != CALLDATA_INFO_PARAM_SET) return false;
    switch (calldata_info->chain_id_state) {
        case CALLDATA_INFO_PARAM_NONE:
        case CALLDATA_INFO_PARAM_SET:
            break;
        default:
            return false;
    }
    switch (calldata_info->selector_state) {
        case CALLDATA_INFO_PARAM_NONE:
        case CALLDATA_INFO_PARAM_SET:
            break;
        default:
            return false;
    }
    switch (calldata_info->amount_state) {
        case CALLDATA_INFO_PARAM_NONE:
        case CALLDATA_INFO_PARAM_SET:
            break;
        default:
            return false;
    }
    switch (calldata_info->spender_state) {
        case CALLDATA_INFO_PARAM_NONE:
        case CALLDATA_INFO_PARAM_SET:
            break;
        default:
            return false;
    }
    return true;
}
