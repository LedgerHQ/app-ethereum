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


static t_ui_context *ui_ctx = NULL;



/**
 * Called on the intermediate dummy screen between the dynamic step
 * && the approve/reject screen
 */
void    ui_712_next_field(void)
{
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
    strcpy(strings.tmp.tmp2, "Review struct");
    const char *struct_name;
    uint8_t struct_name_length;
    if ((struct_name = get_struct_name(struct_ptr, &struct_name_length)) != NULL)
    {
        strncpy(strings.tmp.tmp, struct_name, struct_name_length);
        strings.tmp.tmp[struct_name_length] = '\0';
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

    // Key
    if ((key = get_struct_field_keyname(field_ptr, &key_len)) != NULL)
    {
        strncpy(strings.tmp.tmp2, key, MIN(key_len, sizeof(strings.tmp.tmp2) - 1));
        strings.tmp.tmp2[key_len] = '\0';
    }

    // Value
    strings.tmp.tmp[0] = '\0'; // empty string
    switch (struct_field_type(field_ptr))
    {
        case TYPE_SOL_STRING:
            strncat(strings.tmp.tmp, (char*)data, sizeof(strings.tmp.tmp) - 1);
            if (length > (sizeof(strings.tmp.tmp) - 1))
            {
                strings.tmp.tmp[sizeof(strings.tmp.tmp) - 1 - 3] = '\0';
                strcat(strings.tmp.tmp, "...");
            }
            else
            {
                strings.tmp.tmp[length] = '\0';
            }
            break;
        case TYPE_SOL_ADDRESS:
            getEthDisplayableAddress((uint8_t*)data,
                                     strings.tmp.tmp,
                                     sizeof(strings.tmp.tmp),
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        case TYPE_SOL_BOOL:
            strcpy(strings.tmp.tmp, (*data == false) ? "false" : "true");
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
        // TODO: signed integers should be handled differently
        case TYPE_SOL_INT:
        case TYPE_SOL_UINT:
            uint256_to_decimal(data, length, strings.tmp.tmp, sizeof(strings.tmp.tmp));
            break;
        default:
            PRINTF("Unhandled type\n");
    }

    // not pretty, manually changes the internal state of the UX flow
    // so that we always land on the first screen of a paging step without any visible
    // screen glitching (quick screen switching)
    G_ux.flow_stack[G_ux.stack_count - 1].index = 0;
    // since the flow now thinks we are displaying the first step, do next
    ux_flow_next();
}

/**
 * Used to signal that we are done with reviewing the structs and we can now have
 * the option to approve or reject the signature
 */
void    ui_712_end_sign(void)
{
    ui_ctx->end_reached = true;
    ui_712_next_field();
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
