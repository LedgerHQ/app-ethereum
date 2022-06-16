#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <stdbool.h>
#include "ui_logic.h"
#include "mem.h"
#include "mem_utils.h"
#include "os_io.h"
#include "ux_flow_engine.h"
#include "ui_flow_712.h"
#include "shared_context.h"
#include "eip712.h" // get_struct_name
#include "ethUtils.h" // getEthDisplayableAddress
#include "utils.h" // uint256_to_decimal
#include "common_712.h"
#include "context.h" // eip712_context_deinit
#include "uint256.h" // tostring256 && tostring256_signed


static t_ui_context *ui_ctx = NULL;


static void ui_712_set_buf(const char *const src,
                           size_t src_length,
                           char *const dst,
                           size_t dst_length,
                           bool explicit_trunc)
{
    uint8_t cpy_length;

    if (src_length < dst_length)
    {
        cpy_length = src_length;
    }
    else
    {
        cpy_length = dst_length - 1;
    }
    memcpy(dst, src, cpy_length);
    dst[cpy_length] = '\0';
    if (explicit_trunc && (src_length > dst_length))
    {
        memcpy(dst + cpy_length - 3, "...", 3);
    }
}

/**
 * Set a new title for the EIP-712 generic UX_STEP
 *
 * @param[in] str the new title
 * @param[in] length its length
 */
void    ui_712_set_title(const char *const str, uint8_t length)
{
    ui_712_set_buf(str, length, strings.tmp.tmp2, sizeof(strings.tmp.tmp2), false);
}

/**
 * Set a new value for the EIP-712 generic UX_STEP
 *
 * @param[in] str the new value
 * @param[in] length its length
 */
void    ui_712_set_value(const char *const str, uint8_t length)
{
    ui_712_set_buf(str, length, strings.tmp.tmp, sizeof(strings.tmp.tmp), true);
}

void    ui_712_redraw_generic_step(void)
{
    // not pretty, manually changes the internal state of the UX flow
    // so that we always land on the first screen of a paging step without any visible
    // screen glitching (quick screen switching)
    G_ux.flow_stack[G_ux.stack_count - 1].index = 0;
    // since the flow now thinks we are displaying the first step, do next
    ux_flow_next();
}

/**
 * Called on the intermediate dummy screen between the dynamic step
 * && the approve/reject screen
 */
void    ui_712_next_field(void)
{
    if (ui_ctx == NULL)
    {
        return;
    }
    if (!ui_ctx->end_reached)
    {
        // reply to previous APDU
        G_io_apdu_buffer[0] = 0x90;
        G_io_apdu_buffer[1] = 0x00;
        io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    }
    else
    {
        if (ui_ctx->pos == UI_712_POS_REVIEW)
        {
            ux_flow_next();
            ui_ctx->pos = UI_712_POS_END;
        }
        else
        {
            ux_flow_prev();
            ui_ctx->pos = UI_712_POS_REVIEW;
        }
    }
}

/**
 * Used to notify of a new struct to review (domain or message)
 *
 * @param[in] struct_ptr pointer to the structure
 */
void    ui_712_new_root_struct(const void *const struct_ptr)
{
    const char *struct_name;
    uint8_t struct_name_length;
    const char *const title = "Review struct";

    if (ui_ctx == NULL)
    {
        return;
    }
    ui_712_set_title(title, strlen(title));
    if ((struct_name = get_struct_name(struct_ptr, &struct_name_length)) != NULL)
    {
        ui_712_set_value(struct_name, struct_name_length);
    }
    if (!ui_ctx->shown)
    {
        ux_flow_init(0, ux_712_flow, NULL);
        ui_ctx->shown = true;
    }
    else
    {
        ux_flow_prev();
    }
}

#ifdef HAVE_EIP712_HALF_BLIND
void    ui_712_message_hash(void)
{
    const char *const title = "Message hash";

    ui_712_set_title(title, strlen(titltitlee));
    snprintf(strings.tmp.tmp,
             sizeof(strings.tmp.tmp),
             "0x%.*H",
             KECCAK256_HASH_BYTESIZE,
             tmpCtx.messageSigningContext712.messageHash);
    G_ux.flow_stack[G_ux.stack_count - 1].index = 0;
    ux_flow_next();
}
#endif // HAVE_EIP712_HALF_BLIND

/**
 * Used to notify of a new field to review in the current struct (key + value)
 *
 * @param[in] field_ptr pointer to the new struct field
 * @param[in] data pointer to the field's raw value
 * @param[in] length field's raw value byte-length
 */
void    ui_712_new_field(const void *const field_ptr, const uint8_t *const data, uint8_t length)
{
    const char *key;
    uint8_t key_len;
    uint256_t value256;
    uint128_t value128;
    int32_t value32;
    int16_t value16;

    if (ui_ctx == NULL)
    {
        return;
    }

    // Key
    if ((key = get_struct_field_keyname(field_ptr, &key_len)) != NULL)
    {
        ui_712_set_title(key, key_len);
    }

    // Value
    ui_712_set_value("", 0);
    switch (struct_field_type(field_ptr))
    {
        case TYPE_SOL_STRING:
            ui_712_set_value((char*)data, length);
            break;
        case TYPE_SOL_ADDRESS:
            getEthDisplayableAddress((uint8_t*)data,
                                     strings.tmp.tmp,
                                     sizeof(strings.tmp.tmp),
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        case TYPE_SOL_BOOL:
            if (*data)
            {
                ui_712_set_value("true", 4);
            }
            else
            {
                ui_712_set_value("false", 5);
            }
            break;
        case TYPE_SOL_BYTES_FIX:
        case TYPE_SOL_BYTES_DYN:
            snprintf(strings.tmp.tmp,
                     sizeof(strings.tmp.tmp),
                     "0x%.*H",
                     length,
                     data);
            // +2 for the "0x"
            // x2 for each byte value is represented by 2 ASCII characters
            if ((2 + (length * 2)) > (sizeof(strings.tmp.tmp) - 1))
            {
                strings.tmp.tmp[sizeof(strings.tmp.tmp) - 1 - 3] = '\0';
                strcat(strings.tmp.tmp, "...");
            }
            break;
        case TYPE_SOL_INT:
            switch (get_struct_field_typesize(field_ptr) * 8)
            {
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
                    for (int i = 0; i < length; ++i)
                    {
                        ((uint8_t*)&value32)[length - 1 - i] = data[i];
                    }
                    snprintf(strings.tmp.tmp,
                             sizeof(strings.tmp.tmp),
                             "%d",
                             value32);
                    break;
                case 16:
                    value16 = 0;
                    for (int i = 0; i < length; ++i)
                    {
                        ((uint8_t*)&value16)[length - 1 - i] = data[i];
                    }
                    snprintf(strings.tmp.tmp,
                             sizeof(strings.tmp.tmp),
                             "%d",
                             value16); // expanded to 32 bits
                    break;
                case 8:
                    snprintf(strings.tmp.tmp,
                             sizeof(strings.tmp.tmp),
                             "%d",
                             ((int8_t*)data)[0]); // expanded to 32 bits
                    break;
                default:
                    PRINTF("Unhandled field typesize\n");
            }
            break;
        case TYPE_SOL_UINT:
            convertUint256BE(data, length, &value256);
            tostring256(&value256, 10, strings.tmp.tmp, sizeof(strings.tmp.tmp));
            break;
        default:
            PRINTF("Unhandled type\n");
    }
    ui_712_redraw_generic_step();
}

/**
 * Used to signal that we are done with reviewing the structs and we can now have
 * the option to approve or reject the signature
 */
void    ui_712_end_sign(void)
{
    if (ui_ctx == NULL)
    {
        return;
    }
    ui_ctx->end_reached = true;

#ifndef HAVE_EIP712_HALF_BLIND
    ui_712_next_field();
#endif // HAVE_EIP712_HALF_BLIND
}

/**
 * Initializes the UI context structure in memory
 */
bool    ui_712_init(void)
{
    if ((ui_ctx = MEM_ALLOC_AND_ALIGN_TO_TYPE(sizeof(*ui_ctx), *ui_ctx)))
    {
        ui_ctx->shown = false;
        ui_ctx->end_reached = false;
        ui_ctx->pos = UI_712_POS_REVIEW;
    }
    return ui_ctx != NULL;
}

/**
 * Deinit function that simply unsets the struct pointer to NULL
 */
void    ui_712_deinit(void)
{
    ui_ctx = NULL;
}

/**
 * Approve button handling, calls the common handler function then
 * deinitializes the EIP712 context altogether.
 * @param[in] e unused here, just needed to match the UI function signature
 * @return unused here, just needed to match the UI function signature
 */
unsigned int ui_712_approve(const bagl_element_t *e)
{
    ui_712_approve_cb(e);
    eip712_context_deinit();
    return 0;
}

/**
 * Reject button handling, calls the common handler function then
 * deinitializes the EIP712 context altogether.
 * @param[in] e unused here, just needed to match the UI function signature
 * @return unused here, just needed to match the UI function signature
 */
unsigned int ui_712_reject(const bagl_element_t *e)
{
    ui_712_reject_cb(e);
    eip712_context_deinit();
    return 0;
}

#endif // HAVE_EIP712_FULL_SUPPORT
