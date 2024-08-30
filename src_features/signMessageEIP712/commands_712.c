#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "commands_712.h"
#include "apdu_constants.h"  // APDU response codes
#include "context_712.h"
#include "field_hash.h"
#include "path.h"
#include "ui_logic.h"
#include "typed_data.h"
#include "schema_hash.h"
#include "filtering.h"
#include "common_712.h"
#include "common_ui.h"  // ui_idle
#include "manage_asset_info.h"

// APDUs P1
#define P1_COMPLETE 0x00
#define P1_PARTIAL  0xFF

// APDUs P2
#define P2_DEF_NAME               0x00
#define P2_DEF_FIELD              0xFF
#define P2_IMPL_NAME              P2_DEF_NAME
#define P2_IMPL_ARRAY             0x0F
#define P2_IMPL_FIELD             P2_DEF_FIELD
#define P2_FILT_ACTIVATE          0x00
#define P2_FILT_MESSAGE_INFO      0x0F
#define P2_FILT_DATE_TIME         0xFC
#define P2_FILT_AMOUNT_JOIN_TOKEN 0xFD
#define P2_FILT_AMOUNT_JOIN_VALUE 0xFE
#define P2_FILT_RAW_FIELD         0xFF

/**
 * Send the response to the previous APDU command
 *
 * In case of an error it uses the global variable to retrieve the error code and resets
 * the app context
 *
 * @param[in] success whether the command was successful
 */
static void apdu_reply(bool success) {
    if (success) {
        apdu_response_code = APDU_RESPONSE_OK;
    } else {
        if (apdu_response_code == APDU_RESPONSE_OK) {  // somehow not set
            apdu_response_code = APDU_RESPONSE_ERROR_NO_INFO;
        }
        eip712_context_deinit();
        ui_idle();
    }
}

/**
 * Send the response to the previous APDU command
 *
 * In case of an error it uses the global variable to retrieve the error code and resets
 * the app context
 *
 * @param[in] success whether the command was successful
 */
void handle_eip712_return_code(bool success) {
    apdu_reply(success);

    io_seproxyhal_send_status(apdu_response_code, 0, false, false);
}

/**
 * Process the EIP712 struct definition command
 *
 * @param[in] apdu_buf the APDU payload
 * @return whether the command was successful or not
 */
uint16_t handle_eip712_struct_def(uint8_t p2, const uint8_t *dataBuffer, uint8_t dataLength) {
    bool ret = true;

    if (eip712_context == NULL) {
        ret = eip712_context_init();
    }
    if (struct_state == DEFINED) {
        ret = false;
    }

    if (ret) {
        switch (p2) {
            case P2_DEF_NAME:
                ret = set_struct_name(dataLength, dataBuffer);
                break;
            case P2_DEF_FIELD:
                ret = set_struct_field(dataLength, dataBuffer);
                break;
            default:
                PRINTF("Unknown P2 0x%x\n", p2);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
                ret = false;
        }
    }
    apdu_reply(ret);
    return apdu_response_code;
}

/**
 * Process the EIP712 struct implementation command
 *
 * @param[in] apdu_buf the APDU payload
 * @return whether the command was successful or not
 */
uint16_t handle_eip712_struct_impl(uint8_t p1,
                                   uint8_t p2,
                                   const uint8_t *dataBuffer,
                                   uint8_t dataLength,
                                   uint32_t *flags) {
    bool ret = false;
    bool reply_apdu = true;

    if (eip712_context == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    } else {
        switch (p2) {
            case P2_IMPL_NAME:
                // set root type
                ret = path_set_root((char *) dataBuffer, dataLength);
                if (ret) {
                    if (N_storage.verbose_eip712) {
                        ui_712_review_struct(path_get_root());
                        reply_apdu = false;
                    }
                    ui_712_field_flags_reset();
                }
                break;
            case P2_IMPL_FIELD:
                if ((ret = field_hash(dataBuffer, dataLength, p1 != P1_COMPLETE))) {
                    reply_apdu = false;
                }
                break;
            case P2_IMPL_ARRAY:
                ret = path_new_array_depth(dataBuffer, dataLength);
                break;
            default:
                PRINTF("Unknown P2 0x%x\n", p2);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
        }
    }
    if (reply_apdu) {
        apdu_reply(ret);
        return apdu_response_code;
    }
    *flags |= IO_ASYNCH_REPLY;
    return APDU_NO_RESPONSE;
}

/**
 * Process the EIP712 filtering command
 *
 * @param[in] apdu_buf the APDU payload
 * @return whether the command was successful or not
 */
uint16_t handle_eip712_filtering(uint8_t p2,
                                 const uint8_t *dataBuffer,
                                 uint8_t dataLength,
                                 uint32_t *flags) {
    bool ret = true;
    bool reply_apdu = true;

    if (eip712_context == NULL) {
        apdu_reply(false);
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    if ((p2 != P2_FILT_ACTIVATE) && (ui_712_get_filtering_mode() != EIP712_FILTERING_FULL)) {
        return APDU_RESPONSE_OK;
    }
    switch (p2) {
        case P2_FILT_ACTIVATE:
            if (!N_storage.verbose_eip712) {
                ui_712_set_filtering_mode(EIP712_FILTERING_FULL);
                ret = compute_schema_hash();
            }
            forget_known_assets();
            break;
        case P2_FILT_MESSAGE_INFO:
            ret = filtering_message_info(dataBuffer, dataLength);
            if (ret) {
                reply_apdu = false;
            }
            break;
        case P2_FILT_DATE_TIME:
            ret = filtering_date_time(dataBuffer, dataLength);
            break;
        case P2_FILT_AMOUNT_JOIN_TOKEN:
            ret = filtering_amount_join_token(dataBuffer, dataLength);
            break;
        case P2_FILT_AMOUNT_JOIN_VALUE:
            ret = filtering_amount_join_value(dataBuffer, dataLength);
            break;
        case P2_FILT_RAW_FIELD:
            ret = filtering_raw_field(dataBuffer, dataLength);
            break;
        default:
            PRINTF("Unknown P2 0x%x\n", p2);
            apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
            ret = false;
    }
    if ((p2 > P2_FILT_MESSAGE_INFO) && ret) {
        if (ui_712_push_new_filter_path()) {
            if (!ui_712_filters_counter_incr()) {
                ret = false;
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            }
        }
    }
    if (reply_apdu) {
        apdu_reply(ret);
        return apdu_response_code;
    }
    *flags |= IO_ASYNCH_REPLY;
    return APDU_NO_RESPONSE;
}

/**
 * Process the EIP712 sign command
 *
 * @param[in] apdu_buf the APDU payload
 * @return whether the command was successful or not
 */
uint16_t handle_eip712_sign(const uint8_t *dataBuffer, uint8_t dataLength, uint32_t *flags) {
    bool ret = false;

    if (eip712_context == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    // if the final hashes are still zero or if there are some unimplemented fields
    else if (allzeroes(tmpCtx.messageSigningContext712.domainHash,
                       sizeof(tmpCtx.messageSigningContext712.domainHash)) ||
             allzeroes(tmpCtx.messageSigningContext712.messageHash,
                       sizeof(tmpCtx.messageSigningContext712.messageHash)) ||
             (path_get_field() != NULL)) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    } else if ((ui_712_get_filtering_mode() == EIP712_FILTERING_FULL) &&
               (ui_712_remaining_filters() != 0)) {
        PRINTF("%d EIP712 filters are missing\n", ui_712_remaining_filters());
        apdu_response_code = APDU_RESPONSE_REF_DATA_NOT_FOUND;
    } else if (parseBip32(dataBuffer, &dataLength, &tmpCtx.messageSigningContext.bip32) == NULL) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
    } else {
        if (!N_storage.verbose_eip712 && (ui_712_get_filtering_mode() == EIP712_FILTERING_BASIC)) {
            ui_712_message_hash();
        }
        ret = true;
        ui_712_end_sign();
    }

    if (!ret) {
        apdu_reply(false);
        return apdu_response_code;
    }
    *flags |= IO_ASYNCH_REPLY;
    return APDU_NO_RESPONSE;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
