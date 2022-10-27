#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "apdu_constants.h"
#include "stark_utils.h"
#include "poorstream.h"
#include "ethUtils.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"

#define U8BE(buf, off) \
    (uint64_t)((((uint64_t) U4BE(buf, off)) << 32) | (((uint64_t) U4BE(buf, off + 4)) & 0xFFFFFFFF))
#define TMP_OFFSET 140

void handleStarkwareSignMessage(uint8_t p1,
                                uint8_t p2,
                                uint8_t *dataBuffer,
                                uint8_t dataLength,
                                unsigned int *flags,
                                __attribute__((unused)) unsigned int *tx) {
    uint8_t privateKeyData[INT256_LENGTH];
    uint32_t i;
    uint8_t bip32PathLength;
    uint8_t offset = 1;
    cx_ecfp_private_key_t privateKey;
    poorstream_t bitstream;
    bool selfTransfer = false;
    uint8_t order = 1;
    uint8_t protocol = 2;
    uint8_t preOffset, postOffset;
    uint8_t zeroTest;

    // Initial checks
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }

    if (dataLength < 1) {
        PRINTF("Invalid data\n");
        THROW(0x6a80);
    }

    bip32PathLength = *(dataBuffer);

    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH)) {
        PRINTF("Invalid path\n");
        THROW(0x6a80);
    }
    switch (p1) {
        case P1_STARK_ORDER:
            protocol = 1;
            break;
        case P1_STARK_TRANSFER:
            protocol = 1;
            order = 0;
            break;
        case P1_STARK_ORDER_V2:
            break;
        case P1_STARK_TRANSFER_V2:
        case P1_STARK_CONDITIONAL_TRANSFER:
            order = 0;
            break;
        default:
            THROW(0x6B00);
    }
    postOffset = (protocol == 2 ? 1 + 32 : 0);
    preOffset = (protocol == 2 ? 1 : 0);
    if (order) {
        if (dataLength != (20 + 32 + 20 + 32 + 4 + 4 + 8 + 8 + 4 + 4 + 1 + 4 * bip32PathLength +
                           2 * postOffset)) {
            THROW(0x6700);
        }
    } else {
        if (dataLength != (20 + 32 + 32 + 4 + 4 + 8 + 4 + 4 + 1 + 4 * bip32PathLength + postOffset +
                           (p1 == P1_STARK_CONDITIONAL_TRANSFER ? 32 + 20 : 0))) {
            THROW(0x6700);
        }
    }
    if (p2 != 0) {
        THROW(0x6B00);
    }
    tmpCtx.transactionContext.bip32.length = bip32PathLength;
    for (i = 0; i < bip32PathLength; i++) {
        tmpCtx.transactionContext.bip32.path[i] = U4BE(dataBuffer, offset);
        PRINTF("Storing path %d %d\n", i, tmpCtx.transactionContext.bip32.path[i]);
        offset += 4;
    }
    // Discard the path to use part of dataBuffer as a temporary buffer
    memmove(dataBuffer, dataBuffer + offset, dataLength - offset);
    dataContext.starkContext.conditional = (p1 == P1_STARK_CONDITIONAL_TRANSFER);
    if (dataContext.starkContext.conditional) {
        memmove(dataContext.starkContext.fact,
                dataBuffer + 20 + 32 + postOffset + 32 + 4 + 4 + 8 + 4 + 4,
                32);
        memmove(dataContext.starkContext.conditionAddress,
                dataBuffer + 20 + 32 + postOffset + 32 + 4 + 4 + 8 + 4 + 4 + 32,
                20);
        PRINTF("Fact %.*H\n", 32, dataContext.starkContext.fact);
        PRINTF("Address %.*H\n", 20, dataContext.starkContext.conditionAddress);
    }

    zeroTest = allzeroes(dataBuffer + preOffset, 20);
    if (zeroTest && (protocol == 2) && (dataBuffer[0] != STARK_QUANTUM_ETH)) {
        PRINTF("stark - unexpected quantum descriptor type for null first address %d\n",
               dataBuffer[0]);
        THROW(0x6A80);
    }
    if (!zeroTest && getKnownToken(dataBuffer + preOffset) == NULL) {
        PRINTF("stark - cannot process unknown token %.*H", 20, dataBuffer + preOffset);
        THROW(0x6A80);
    }
    if (order) {
        zeroTest = allzeroes(dataBuffer + 20 + 32 + postOffset + preOffset, 20);
        if (zeroTest && (protocol == 2) && (dataBuffer[1 + 20 + 32 + 32] != STARK_QUANTUM_ETH)) {
            PRINTF("stark - unexpected quantum descriptor type for null second address %d\n",
                   dataBuffer[1 + 20 + 32 + 32]);
            THROW(0x6A80);
        }
        if (!zeroTest && getKnownToken(dataBuffer + 20 + 32 + postOffset + preOffset) == NULL) {
            PRINTF("stark - cannot process unknown token %.*H",
                   20,
                   dataBuffer + 20 + 32 + postOffset + preOffset);
            THROW(0x6A80);
        }
    }
    // Prepare the Stark parameters
    io_seproxyhal_io_heartbeat();
    compute_token_id(&global_sha3,
                     dataBuffer + preOffset,
                     (protocol == 2 ? dataBuffer[0] : STARK_QUANTUM_LEGACY),
                     dataBuffer + preOffset + 20,
                     (protocol == 2 ? dataBuffer + 1 + 20 + 32 : NULL),
                     false,
                     dataContext.starkContext.w1);
    if (order) {
        io_seproxyhal_io_heartbeat();
        compute_token_id(&global_sha3,
                         dataBuffer + 20 + 32 + postOffset + preOffset,
                         (protocol == 2 ? dataBuffer[1 + 20 + 32 + 32] : STARK_QUANTUM_LEGACY),
                         dataBuffer + 20 + 32 + postOffset + preOffset + 20,
                         (protocol == 2 ? dataBuffer + 1 + 20 + 32 + 32 + 1 + 20 + 32 : NULL),
                         false,
                         dataContext.starkContext.w2);
        offset = 20 + 32 + postOffset + 20 + 32 + postOffset;
    } else {
        memmove(dataContext.starkContext.w2, dataBuffer + 20 + 32 + postOffset, 32);
        offset = 20 + 32 + postOffset + 32;
    }

    poorstream_init(&bitstream, dataContext.starkContext.w3);
    poorstream_write_bits(&bitstream, 0, 11);  // padding
    poorstream_write_bits(&bitstream,
                          (p1 == P1_STARK_CONDITIONAL_TRANSFER ? STARK_CONDITIONAL_TRANSFER_TYPE
                           : order                             ? STARK_ORDER_TYPE
                                                               : STARK_TRANSFER_TYPE),
                          4);
    poorstream_write_bits(&bitstream, U4BE(dataBuffer, offset), 31);
    poorstream_write_bits(&bitstream, U4BE(dataBuffer, offset + 4), 31);
    poorstream_write_bits(&bitstream, U8BE(dataBuffer, offset + 4 + 4), 63);
    if (order) {
        poorstream_write_bits(&bitstream, U8BE(dataBuffer, offset + 4 + 4 + 8), 63);
        offset += 4 + 4 + 8 + 8;
    } else {
        poorstream_write_bits(&bitstream, 0, 63);
        offset += 4 + 4 + 8;
    }
    poorstream_write_bits(&bitstream, U4BE(dataBuffer, offset), 31);
    poorstream_write_bits(&bitstream, U4BE(dataBuffer, offset + 4), 22);

    PRINTF("stark w1 %.*H\n", 32, dataContext.starkContext.w1);
    PRINTF("stark w2 %.*H\n", 32, dataContext.starkContext.w2);
    PRINTF("stark w3 %.*H\n", 32, dataContext.starkContext.w3);

    if (dataContext.starkContext.conditional) {
        cx_keccak_init(&global_sha3, 256);
        cx_hash((cx_hash_t *) &global_sha3,
                0,
                dataContext.starkContext.conditionAddress,
                20,
                NULL,
                0);
        cx_hash((cx_hash_t *) &global_sha3,
                CX_LAST,
                dataContext.starkContext.fact,
                32,
                dataContext.starkContext.w4,
                32);
        dataContext.starkContext.w4[0] &= 0x03;
        PRINTF("stark w4 %.*H\n", 32, dataContext.starkContext.w4);
    }
    // Prepare the UI
    if (order) {
        io_seproxyhal_io_heartbeat();
        // amount to sell
        stark_get_amount_string(dataBuffer + preOffset,
                                dataBuffer + preOffset + 20,
                                dataBuffer + 20 + 32 + postOffset + 20 + 32 + postOffset + 4 + 4,
                                (char *) (dataBuffer + TMP_OFFSET),
                                strings.common.fullAmount);
        io_seproxyhal_io_heartbeat();
        // amount to buy
        stark_get_amount_string(
            dataBuffer + 20 + 32 + postOffset + preOffset,
            dataBuffer + 20 + 32 + postOffset + preOffset + 20,
            dataBuffer + 20 + 32 + postOffset + 20 + 32 + postOffset + 4 + 4 + 8,
            (char *) (dataBuffer + TMP_OFFSET),
            strings.common.maxFee);
        // src vault ID
        snprintf(strings.common.fullAddress,
                 sizeof(strings.common.fullAddress),
                 "%d",
                 U4BE(dataBuffer, 20 + 32 + postOffset + 20 + 32 + postOffset));
    } else {
        cx_ecfp_public_key_t publicKey;
        // Check if the transfer is a self transfer
        io_seproxyhal_io_heartbeat();
        starkDerivePrivateKey(tmpCtx.transactionContext.bip32.path,
                              bip32PathLength,
                              privateKeyData);
        cx_ecfp_init_private_key(CX_CURVE_Stark256, privateKeyData, 32, &privateKey);
        io_seproxyhal_io_heartbeat();
        cx_ecfp_generate_pair(CX_CURVE_Stark256, &publicKey, &privateKey, 1);
        explicit_bzero(&privateKey, sizeof(privateKey));
        explicit_bzero(privateKeyData, sizeof(privateKeyData));
        io_seproxyhal_io_heartbeat();
        selfTransfer = (memcmp(publicKey.W + 1, dataBuffer + 20 + 32 + postOffset, 32) == 0);
        PRINTF("self transfer %d\n", selfTransfer);
        io_seproxyhal_io_heartbeat();
        // amount to transfer
        stark_get_amount_string(dataBuffer + preOffset,
                                dataBuffer + preOffset + 20,
                                dataBuffer + 20 + 32 + postOffset + 32 + 4 + 4,
                                (char *) (dataBuffer + TMP_OFFSET),
                                tmpContent.tmp);
        // dest vault ID
        snprintf(strings.tmp.tmp2,
                 sizeof(strings.tmp.tmp2),
                 "%d",
                 U4BE(dataBuffer, 20 + 32 + postOffset + 32 + 4));
        if (!selfTransfer) {
            memmove(dataContext.starkContext.transferDestination,
                    dataBuffer + 20 + 32 + postOffset,
                    32);
            snprintf(strings.tmp.tmp,
                     sizeof(strings.tmp.tmp),
                     "0x%.*H",
                     32,
                     dataBuffer + 20 + 32 + postOffset);
        }
    }
    if (order) {
        ui_stark_limit_order();
    } else {
        ui_stark_transfer(selfTransfer, dataContext.starkContext.conditional);
    }

    *flags |= IO_ASYNCH_REPLY;
}

#endif
