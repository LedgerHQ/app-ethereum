#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_signTx.h"
#include "eth_plugin_interface.h"
#include "apdu_constants.h"
#include "swap_error_code_helpers.h"
#include "common_ui.h"
#include "ui_callbacks.h"
#include "mem.h"
#include "mem_utils.h"
#include "tx_ctx.h"

typedef enum {
    SIGN_MODE_BASIC = 0,
    SIGN_MODE_STORE = 1,
    SIGN_MODE_START_FLOW = 2,
} e_sign_mode;

cx_sha3_t *g_tx_hash_ctx = NULL;

static uint16_t handle_first_sign_chunk(const uint8_t *payload,
                                        uint8_t length,
                                        uint8_t *offset,
                                        e_sign_mode mode) {
    uint8_t length_tmp = length;
    uint8_t tx_type;

    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    appState = APP_STATE_SIGNING_TX;

    if (parseBip32(&payload[*offset], &length_tmp, &tmpCtx.transactionContext.bip32) == NULL) {
        return SWO_INCORRECT_DATA;
    }
    *offset += (length - length_tmp);

    tmpContent.txContent.dataPresent = false;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;

    if (mem_buffer_allocate((void **) &g_tx_hash_ctx, sizeof(cx_sha3_t)) == false) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    if (init_tx(&txContext, g_tx_hash_ctx, &tmpContent.txContent, mode == SIGN_MODE_STORE) ==
        false) {
        return SWO_INCORRECT_DATA;
    }
    if (*offset >= length) {
        PRINTF("Invalid data\n");
        return SWO_INCORRECT_DATA;
    }

    // EIP 2718: TransactionType might be present before the TransactionPayload.
    tx_type = payload[*offset];
    if (tx_type <= MAX_TX_TYPE) {
        switch (tx_type) {
            case EIP1559:
            case EIP2930:
            case EIP7702:
                break;
            default:
                PRINTF("Transaction type %d not supported\n", tx_type);
                // TODO: change this error code along with
                // https://github.com/LedgerHQ/device-sdk-ts/blob/5c5c92b469dca1b3ef478a8fb3a2d94951c88035/packages/signer/signer-eth/src/internal/app-binder/command/utils/ethAppErrors.ts#L29
                return SWO_MEMORY_WRITE_ERROR;
        }
        if (cx_hash_no_throw((cx_hash_t *) g_tx_hash_ctx, 0, &tx_type, sizeof(tx_type), NULL, 0) !=
            CX_OK) {
            return SWO_INCORRECT_DATA;
        }
        txContext.txType = tx_type;
        *offset += sizeof(tx_type);
    } else {
        txContext.txType = LEGACY;
    }
    PRINTF("TxType: %x\n", txContext.txType);
    return APDU_NO_RESPONSE;
}

uint16_t handle_parsing_status(parserStatus_e status) {
    uint16_t sw = APDU_NO_RESPONSE;

    switch (status) {
        case USTREAM_SUSPENDED:
            break;
        case USTREAM_FINISHED:
            sw = finalize_parsing(&txContext);
            break;
        case USTREAM_PROCESSING:
            sw = SWO_SUCCESS;
            break;
        case USTREAM_FAULT:
            if (G_called_from_swap) {
                // We have encountered an error while trying to sign a SWAP type transaction
                // Return dedicated error code and flag an early exit back to Exchange
                G_swap_response_ready = true;
                send_swap_error_simple(APDU_RESPONSE_MODE_CHECK_FAILED,
                                       SWAP_EC_ERROR_GENERIC,
                                       APP_CODE_CALLDATA_ISSUE);
                // unreachable
                os_sched_exit(0);
            }
            sw = SWO_INCORRECT_DATA;
            break;
        default:
            PRINTF("Unexpected parser status\n");
            sw = SWO_INCORRECT_DATA;
    }
    return sw;
}

uint16_t handleSign(uint8_t p1,
                    uint8_t p2,
                    const uint8_t *payload,
                    uint8_t length,
                    unsigned int *flags) {
    uint16_t sw = APDU_NO_RESPONSE;
    uint8_t offset = 0;

    switch (p2) {
        case SIGN_MODE_BASIC:
        case SIGN_MODE_STORE:
            switch (p1) {
                case P1_FIRST:
                    if ((sw = handle_first_sign_chunk(payload, length, &offset, p2)) !=
                        APDU_NO_RESPONSE) {
                        return sw;
                    }
                    break;
                case P1_MORE:
                    if (appState != APP_STATE_SIGNING_TX) {
                        PRINTF("Signature not initialized\n");
                        return SWO_COMMAND_NOT_ALLOWED;
                    }
                    break;
                default:
                    return SWO_WRONG_P1_P2;
            }
            break;
        case SIGN_MODE_START_FLOW:
            if (appState != APP_STATE_SIGNING_TX) {
                PRINTF("Signature not initialized\n");
                return SWO_COMMAND_NOT_ALLOWED;
            }
            if (length != 0) {
                return SWO_INCORRECT_DATA;
            }
            if (!validate_instruction_hash()) {
                PRINTF("Error: instructions hash mismatch!\n");
                return SWO_INCORRECT_DATA;
            }
            if (get_tx_ctx_count() != 1) {
                PRINTF("Error: remnant unprocessed TX context!\n");
                return SWO_COMMAND_NOT_ALLOWED;
            }
            if (!ui_gcs()) {
                return SWO_NOT_SUPPORTED_ERROR_NO_INFO;
            }
            *flags |= IO_ASYNCH_REPLY;
            return APDU_NO_RESPONSE;
        default:
            return SWO_WRONG_P1_P2;
    }

    if (txContext.currentField == RLP_NONE) {
        PRINTF("Parser not initialized\n");
        return SWO_COMMAND_NOT_ALLOWED;
    }
    parserStatus_e pstatus = process_tx(&txContext, &payload[offset], length - offset);
    sw = handle_parsing_status(pstatus);
    if (p2 == SIGN_MODE_BASIC) {
        if ((pstatus == USTREAM_FINISHED) && (sw == SWO_SUCCESS)) {
            // don't respond now, will be done after review
            sw = APDU_NO_RESPONSE;
        }
    }
    if (sw != APDU_NO_RESPONSE) {
        if ((sw != SWO_SUCCESS) && (g_tx_hash_ctx != NULL)) {
            mem_buffer_cleanup((void **) &g_tx_hash_ctx);
        }
        return sw;
    }

    *flags |= IO_ASYNCH_REPLY;
    // Return code will be sent after UI approve/cancel
    return sw;
}
