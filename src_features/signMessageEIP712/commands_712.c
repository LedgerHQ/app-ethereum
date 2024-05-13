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
void handle_eip712_return_code(bool success) {
    if (success) {
        apdu_response_code = APDU_RESPONSE_OK;
    } else if (apdu_response_code == APDU_RESPONSE_OK) {  // somehow not set
        apdu_response_code = APDU_RESPONSE_ERROR_NO_INFO;
    }

    G_io_apdu_buffer[0] = (apdu_response_code >> 8) & 0xff;
    G_io_apdu_buffer[1] = apdu_response_code & 0xff;

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    if (!success) {
        eip712_context_deinit();
        ui_idle();
    }
}

/**
 * Process the EIP712 struct definition command
 *
 * @param[in] apdu_buf the APDU payload
 * @return whether the command was successful or not
 */
bool handle_eip712_struct_def(const uint8_t *const apdu_buf) {
    bool ret = true;

    if (eip712_context == NULL) {
        ret = eip712_context_init();
    }

    if (struct_state == DEFINED) {
        ret = false;
    }

    if (ret) {
        switch (apdu_buf[OFFSET_P2]) {
            case P2_DEF_NAME:
                ret = set_struct_name(apdu_buf[OFFSET_LC], &apdu_buf[OFFSET_CDATA]);
                break;
            case P2_DEF_FIELD:
                ret = set_struct_field(apdu_buf[OFFSET_LC], &apdu_buf[OFFSET_CDATA]);
                break;
            default:
                PRINTF("Unknown P2 0x%x for APDU 0x%x\n",
                       apdu_buf[OFFSET_P2],
                       apdu_buf[OFFSET_INS]);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
                ret = false;
        }
    }
    handle_eip712_return_code(ret);
    return ret;
}

/**
 * Process the EIP712 struct implementation command
 *
 * @param[in] apdu_buf the APDU payload
 * @return whether the command was successful or not
 */
bool handle_eip712_struct_impl(const uint8_t *const apdu_buf) {
    bool ret = false;
    bool reply_apdu = true;

    if (eip712_context == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    } else {
        switch (apdu_buf[OFFSET_P2]) {
            case P2_IMPL_NAME:
                // set root type
                ret = path_set_root((char *) &apdu_buf[OFFSET_CDATA], apdu_buf[OFFSET_LC]);
                if (ret) {
                    if (N_storage.verbose_eip712) {
                        ui_712_review_struct(path_get_root());
                        reply_apdu = false;
                    }
                    ui_712_field_flags_reset();
                }
                break;
            case P2_IMPL_FIELD:
                if ((ret = field_hash(&apdu_buf[OFFSET_CDATA],
                                      apdu_buf[OFFSET_LC],
                                      apdu_buf[OFFSET_P1] != P1_COMPLETE))) {
                    reply_apdu = false;
                }
                break;
            case P2_IMPL_ARRAY:
                ret = path_new_array_depth(&apdu_buf[OFFSET_CDATA], apdu_buf[OFFSET_LC]);
                break;
            default:
                PRINTF("Unknown P2 0x%x for APDU 0x%x\n",
                       apdu_buf[OFFSET_P2],
                       apdu_buf[OFFSET_INS]);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
        }
    }
    if (reply_apdu) {
        handle_eip712_return_code(ret);
    }
    return ret;
}

/**
 * Process the EIP712 filtering command
 *
 * @param[in] apdu_buf the APDU payload
 * @return whether the command was successful or not
 */
bool handle_eip712_filtering(const uint8_t *const apdu_buf) {
    bool ret = true;
    bool reply_apdu = true;

    if (eip712_context == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }
    if ((apdu_buf[OFFSET_P2] != P2_FILT_ACTIVATE) &&
        (ui_712_get_filtering_mode() != EIP712_FILTERING_FULL)) {
        handle_eip712_return_code(true);
        return true;
    }
    switch (apdu_buf[OFFSET_P2]) {
        case P2_FILT_ACTIVATE:
            if (!N_storage.verbose_eip712) {
                ui_712_set_filtering_mode(EIP712_FILTERING_FULL);
                ret = compute_schema_hash();
            }
            forget_known_assets();
            break;
        case P2_FILT_MESSAGE_INFO:
            ret = filtering_message_info(&apdu_buf[OFFSET_CDATA], apdu_buf[OFFSET_LC]);
            if (ret) {
                reply_apdu = false;
            }
            break;
        case P2_FILT_DATE_TIME:
            ret = filtering_date_time(&apdu_buf[OFFSET_CDATA], apdu_buf[OFFSET_LC]);
            break;
        case P2_FILT_AMOUNT_JOIN_TOKEN:
            ret = filtering_amount_join_token(&apdu_buf[OFFSET_CDATA], apdu_buf[OFFSET_LC]);
            break;
        case P2_FILT_AMOUNT_JOIN_VALUE:
            ret = filtering_amount_join_value(&apdu_buf[OFFSET_CDATA], apdu_buf[OFFSET_LC]);
            break;
        case P2_FILT_RAW_FIELD:
            ret = filtering_raw_field(&apdu_buf[OFFSET_CDATA], apdu_buf[OFFSET_LC]);
            break;
        default:
            PRINTF("Unknown P2 0x%x for APDU 0x%x\n", apdu_buf[OFFSET_P2], apdu_buf[OFFSET_INS]);
            apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
            ret = false;
    }
    if (reply_apdu) {
        handle_eip712_return_code(ret);
    }
    return ret;
}

/**
 * Process the EIP712 sign command
 *
 * @param[in] apdu_buf the APDU payload
 * @return whether the command was successful or not
 */
bool handle_eip712_sign(const uint8_t *const apdu_buf) {
    bool ret = false;
    uint8_t length = apdu_buf[OFFSET_LC];

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
    } else if (parseBip32(&apdu_buf[OFFSET_CDATA], &length, &tmpCtx.messageSigningContext.bip32) !=
               NULL) {
        if (!N_storage.verbose_eip712 && (ui_712_get_filtering_mode() == EIP712_FILTERING_BASIC)) {
            ui_712_message_hash();
        }
        ret = true;
        ui_712_end_sign();
    }
    if (!ret) {
        handle_eip712_return_code(ret);
    }
    return ret;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
