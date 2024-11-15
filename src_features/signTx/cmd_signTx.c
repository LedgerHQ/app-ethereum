#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_signTx.h"
#include "eth_plugin_interface.h"
#include "apdu_constants.h"
#ifdef HAVE_GENERIC_TX_PARSER
#include "gtp_tx_info.h"
#endif
#include "common_ui.h"

typedef enum {
    SIGN_MODE_BASIC = 0,
    SIGN_MODE_STORE = 1,
    SIGN_MODE_START_FLOW = 2,
} e_sign_mode;

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
        return APDU_RESPONSE_INVALID_DATA;
    }
    *offset += (length - length_tmp);

    tmpContent.txContent.dataPresent = false;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;

    if (init_tx(&txContext, &global_sha3, &tmpContent.txContent, mode == SIGN_MODE_STORE) ==
        false) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (*offset >= length) {
        PRINTF("Invalid data\n");
        return APDU_RESPONSE_INVALID_DATA;
    }

    // EIP 2718: TransactionType might be present before the TransactionPayload.
    tx_type = payload[*offset];
    if (tx_type <= MAX_TX_TYPE) {
        switch (tx_type) {
            case EIP1559:
            case EIP2930:
                break;
            default:
                PRINTF("Transaction type %d not supported\n", tx_type);
                return APDU_RESPONSE_TX_TYPE_NOT_SUPPORTED;
        }
        if (cx_hash_no_throw((cx_hash_t *) &global_sha3, 0, &tx_type, sizeof(tx_type), NULL, 0) !=
            CX_OK) {
            return APDU_RESPONSE_INVALID_DATA;
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
            sw = APDU_RESPONSE_OK;
            break;
        case USTREAM_FAULT:
            if (G_called_from_swap) {
                // We have encountered an error while trying to sign a SWAP type transaction
                // Return dedicated error code and flag an early exit back to Exchange
                G_swap_response_ready = true;
                send_swap_error(ERROR_GENERIC, APP_CODE_CALLDATA_ISSUE, NULL, NULL);
                // unreachable
                os_sched_exit(0);
            }
            sw = APDU_RESPONSE_INVALID_DATA;
            break;
        default:
            PRINTF("Unexpected parser status\n");
            sw = APDU_RESPONSE_INVALID_DATA;
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
#ifdef HAVE_GENERIC_TX_PARSER
        case SIGN_MODE_STORE:
#endif
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
                        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
                    }
                    break;
                default:
                    return APDU_RESPONSE_INVALID_P1_P2;
            }
            break;
#ifdef HAVE_GENERIC_TX_PARSER
        case SIGN_MODE_START_FLOW:
            if (appState != APP_STATE_SIGNING_TX) {
                PRINTF("Signature not initialized\n");
                return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            }
            if (length != 0) {
                return APDU_RESPONSE_INVALID_DATA;
            }
            if (!validate_instruction_hash()) {
                PRINTF("Error: instructions hash mismatch!\n");
                return APDU_RESPONSE_INVALID_DATA;
            }
            if (!ui_gcs()) {
                return APDU_RESPONSE_INTERNAL_ERROR;
            }
            *flags |= IO_ASYNCH_REPLY;
            return APDU_NO_RESPONSE;
#endif
        default:
            return APDU_RESPONSE_INVALID_P1_P2;
    }

    if (txContext.currentField == RLP_NONE) {
        PRINTF("Parser not initialized\n");
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    parserStatus_e pstatus = process_tx(&txContext, &payload[offset], length - offset);
    sw = handle_parsing_status(pstatus);
    if (p2 == SIGN_MODE_BASIC) {
        if ((pstatus == USTREAM_FINISHED) && (sw == APDU_RESPONSE_OK)) {
            // don't respond now, will be done after review
            sw = APDU_NO_RESPONSE;
        }
    }
    if (sw != APDU_NO_RESPONSE) {
        return sw;
    }

    *flags |= IO_ASYNCH_REPLY;
    // Return code will be sent after UI approve/cancel
    return sw;
}
