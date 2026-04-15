#include "cmd_safe_account.h"
#include "safe_descriptor.h"
#include "signer_descriptor.h"
#include "apdu_constants.h"
#include "tlv_apdu.h"
#include "common_ui.h"

#define SAFE_DESCRIPTOR   0x00
#define SIGNER_DESCRIPTOR 0x01

/**
 * @brief Handle Safe Account APDU.
 *
 * @param[in] p1 APDU parameter 1 (indicates First or following chunk)
 * @param[in] p2 APDU parameter 2 (indicates Safe or Signers descriptor)
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @param[out] flags APDU flags
 * @return whether the handling was successful
 */
uint16_t handle_safe_account(uint8_t p1,
                             uint8_t p2,
                             const uint8_t *data,
                             uint8_t length,
                             uint32_t *flags) {
    // Check P1 parameter
    if (p1 != P1_FIRST_CHUNK && p1 != P1_FOLLOWING_CHUNK) {
        PRINTF("Error: Invalid P1 (%u)\n", p1);
        return SWO_WRONG_P1_P2;
    }

    // Check P2 parameter and validate state
    switch (p2) {
        case SAFE_DESCRIPTOR:
            if (SAFE_DESC != NULL) {
                PRINTF("Error: Safe descriptor already exists!\n");
                return SWO_FILE_ALREADY_EXISTS;
            }
            break;
        case SIGNER_DESCRIPTOR:
            if (SAFE_DESC == NULL) {
                PRINTF("Error: Safe descriptor does not exist!\n");
                return SWO_COMMAND_NOT_ALLOWED;
            }
            if (SIGNER_DESC.data != NULL) {
                PRINTF("Error: Signer descriptor already exists!\n");
                return SWO_FILE_ALREADY_EXISTS;
            }
            break;
        default:
            PRINTF("Error: Invalid P2 (%u)\n", p2);
            return SWO_WRONG_P1_P2;
    }

    // Parse TLV payload
    bool result = false;
    if (p2 == SAFE_DESCRIPTOR) {
        result = tlv_from_apdu(p1 == P1_FIRST_CHUNK, length, data, &handle_safe_tlv_payload);
    } else {  // SIGNER_DESCRIPTOR
        result = tlv_from_apdu(p1 == P1_FIRST_CHUNK, length, data, &handle_signer_tlv_payload);
    }

    if (!result) {
        return SWO_INCORRECT_DATA;
    }

    // Display UI when all data is received
    if (SIGNER_DESC.is_valid) {
        ui_display_safe_account();
        *flags |= IO_ASYNCH_REPLY;
        return APDU_NO_RESPONSE;
    }
    return SWO_SUCCESS;
}

/**
 * @brief Clear the Safe Account memory.
 *
 */
void clear_safe_account(void) {
    clear_safe_descriptor();
    clear_signer_descriptor();
}
