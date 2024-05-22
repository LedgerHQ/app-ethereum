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
    e_filtering_type type;

    if (eip712_context == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        ret = false;
    } else {
        switch (apdu_buf[OFFSET_P2]) {
            case P2_FILT_ACTIVATE:
                if (!N_storage.verbose_eip712) {
                    ui_712_set_filtering_mode(EIP712_FILTERING_FULL);
                    ret = compute_schema_hash();
                }
                break;
            case P2_FILT_MESSAGE_INFO:
            case P2_FILT_SHOW_FIELD:
                type = (apdu_buf[OFFSET_P2] == P2_FILT_MESSAGE_INFO)
                           ? FILTERING_PROVIDE_MESSAGE_INFO
                           : FILTERING_SHOW_FIELD;
                if (ui_712_get_filtering_mode() == EIP712_FILTERING_FULL) {
                    ret =
                        provide_filtering_info(&apdu_buf[OFFSET_CDATA], apdu_buf[OFFSET_LC], type);
                    if ((apdu_buf[OFFSET_P2] == P2_FILT_MESSAGE_INFO) && ret) {
                        reply_apdu = false;
                    }
                }
                break;
            default:
                PRINTF("Unknown P2 0x%x for APDU 0x%x\n",
                       apdu_buf[OFFSET_P2],
                       apdu_buf[OFFSET_INS]);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
                ret = false;
        }
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
