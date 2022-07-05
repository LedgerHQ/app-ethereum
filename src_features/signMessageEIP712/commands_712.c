#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "commands_712.h"
#include "apdu_constants.h" // APDU response codes
#include "context.h"
#include "field_hash.h"
#include "path.h"
#include "ui_logic.h"
#include "typed_data.h"
#include "schema_hash.h"
#include "filtering.h"
#include "common_712.h"

static void handle_eip712_return_code(bool ret)
{
    if (ret)
    {
        apdu_response_code = APDU_RESPONSE_OK;
    }
    *(uint16_t*)G_io_apdu_buffer = __builtin_bswap16(apdu_response_code);

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    if (!ret)
    {
        eip712_context_deinit();
    }
}

bool    handle_eip712_struct_def(const uint8_t *const apdu_buf)
{
    bool ret = true;

    if (eip712_context == NULL)
    {
        ret = eip712_context_init();
    }
    if (ret)
    {
        switch (apdu_buf[OFFSET_P2])
        {
            case P2_NAME:
                ret = set_struct_name(apdu_buf[OFFSET_LC], &apdu_buf[OFFSET_CDATA]);
                break;
            case P2_FIELD:
                ret = set_struct_field(apdu_buf);
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

bool    handle_eip712_struct_impl(const uint8_t *const apdu_buf)
{
    bool ret = false;
    bool reply_apdu = true;

    if (eip712_context == NULL)
    {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    else
    {
        switch (apdu_buf[OFFSET_P2])
        {
            case P2_NAME:
                // set root type
                if ((ret = path_set_root((char*)&apdu_buf[OFFSET_CDATA],
                                         apdu_buf[OFFSET_LC])))
                {
                    if (N_storage.verbose_eip712)
                    {
                        ui_712_review_struct(path_get_root());
                        reply_apdu = false;
                    }
                    ui_712_field_flags_reset();
                }
                break;
            case P2_FIELD:
                if ((ret = field_hash(&apdu_buf[OFFSET_CDATA],
                                      apdu_buf[OFFSET_LC],
                                      apdu_buf[OFFSET_P1] != P1_COMPLETE)))
                {
                    reply_apdu = false;
                }
                break;
            case P2_ARRAY:
                ret = path_new_array_depth(apdu_buf[OFFSET_CDATA]);
                break;
            default:
                PRINTF("Unknown P2 0x%x for APDU 0x%x\n",
                       apdu_buf[OFFSET_P2],
                       apdu_buf[OFFSET_INS]);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
        }
    }
    if (reply_apdu)
    {
        handle_eip712_return_code(ret);
    }
    return ret;
}

bool    handle_eip712_filtering(const uint8_t *const apdu_buf)
{
    bool ret = true;
    bool reply_apdu = true;

    if (eip712_context == NULL)
    {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        ret = false;
    }
    else
    {
        switch (apdu_buf[OFFSET_P1])
        {
            case P1_ACTIVATE:
                if (!N_storage.verbose_eip712)
                {
                    ui_712_set_filtering_mode(EIP712_FILTERING_FULL);
                    ret = compute_schema_hash();
                }
                break;
            case P1_CONTRACT_NAME:
            case P1_FIELD_NAME:
                if (ui_712_get_filtering_mode() == EIP712_FILTERING_FULL)
                {
                    ret = provide_filtering_info(&apdu_buf[OFFSET_CDATA],
                            apdu_buf[OFFSET_LC],
                            apdu_buf[OFFSET_P1]);
                    if ((apdu_buf[OFFSET_P1] == P1_CONTRACT_NAME) && ret)
                    {
                        reply_apdu = false;
                    }
                }
                break;
            default:
                PRINTF("Unknown P1 0x%x for APDU 0x%x\n",
                        apdu_buf[OFFSET_P1],
                        apdu_buf[OFFSET_INS]);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
                ret = false;
        }
    }
    if (reply_apdu)
    {
        handle_eip712_return_code(ret);
    }
    return ret;
}

bool    handle_eip712_sign(const uint8_t *const apdu_buf)
{
    bool ret = false;
    uint8_t length = apdu_buf[OFFSET_LC];

    if (eip712_context == NULL)
    {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    else if (parseBip32(&apdu_buf[OFFSET_CDATA],
                        &length,
                        &tmpCtx.messageSigningContext.bip32) != NULL)
    {
        if (!N_storage.verbose_eip712 && (ui_712_get_filtering_mode() == EIP712_FILTERING_BASIC))
        {
            ui_712_message_hash();
        }
        ret = true;
        ui_712_end_sign();
    }
    if (!ret)
    {
        handle_eip712_return_code(ret);
    }
    return ret;
}


#endif // HAVE_EIP712_FULL_SUPPORT
