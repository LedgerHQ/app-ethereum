#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "apdu_constants.h"
#include "stark_utils.h"
#ifdef TARGET_BLUE
#include "ui_blue.h"
#endif
#ifdef HAVE_UX_FLOW
#include "ui_flow.h"
#endif
#include "poorstream.h"
#include "ui_callbacks.h"

#define U8BE(buf, off) (uint64_t)((((uint64_t)U4BE(buf, off)) << 32) | (((uint64_t)U4BE(buf, off + 4)) & 0xFFFFFFFF))
#define TMP_OFFSET 140

void handleStarkwareSignMessage(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx) {
  uint8_t privateKeyData[32];
  uint32_t i;
  uint8_t bip32PathLength = *(dataBuffer);
  uint8_t offset = 1;
  cx_ecfp_private_key_t privateKey;
  poorstream_t bitstream;
  bool selfTransfer = false;
  // Initial checks
  if (appState != APP_STATE_IDLE) {
    reset_app_context();
  }
  if ((bip32PathLength < 0x01) ||
      (bip32PathLength > MAX_BIP32_PATH)) {
    PRINTF("Invalid path\n");
    THROW(0x6a80);
  }
  switch(p1) {
    case P1_STARK_ORDER:
      if (dataLength != (20 + 32 + 20 + 32 + 4 + 4 + 8 + 8 + 4 + 4 + 1 + 4 * bip32PathLength)) {
        THROW(0x6700);
      }
      break;
    case P1_STARK_TRANSFER:
      if (dataLength != (20 + 32 + 32 + 4 + 4 + 8 + 4 + 4 + 1 + 4 * bip32PathLength)) {
        THROW(0x6700);
      }
      break;
    default:
      THROW(0x6B00);
  }
  if (p2 != 0) {
    THROW(0x6B00);
  }
  tmpCtx.transactionContext.pathLength = bip32PathLength;
  for (i = 0; i < bip32PathLength; i++) {
    tmpCtx.transactionContext.bip32Path[i] = U4BE(dataBuffer, offset);
    PRINTF("Storing path %d %d\n", i, tmpCtx.transactionContext.bip32Path[i]);
    offset += 4;
  }
  // Discard the path to use part of dataBuffer as a temporary buffer
  os_memmove(dataBuffer, dataBuffer + offset, dataLength - offset);
  // Fail immediately if the contract is unknown
  if (!allzeroes(dataBuffer, 20) && getKnownToken(dataBuffer) == NULL) {
      PRINTF("stark - cannot process unknown token %.*H", 20, dataBuffer);
      THROW(0x6A80);
  }
  if ((p1 == P1_STARK_ORDER) && (!allzeroes(dataBuffer + 20 + 32, 20) && getKnownToken(dataBuffer + 20 + 32) == NULL)) {
      PRINTF("stark - cannot process unknown token %.*H", 20, dataBuffer + 20 + 32);
      THROW(0x6A80);
  }
  // Prepare the Stark parameters
  io_seproxyhal_io_heartbeat();
  compute_token_id(&sha3, dataBuffer, dataBuffer + 20, dataContext.starkContext.w1);
  if (p1 == P1_STARK_ORDER) {
    io_seproxyhal_io_heartbeat();
    compute_token_id(&sha3, dataBuffer + 20 + 32, dataBuffer + 20 + 32 + 20, dataContext.starkContext.w2);
    offset = 20 + 32 + 20 + 32;
  }
  else {
    os_memmove(dataContext.starkContext.w2, dataBuffer + 20 + 32, 32);
    offset = 20 + 32 + 32;
  }
  poorstream_init(&bitstream, dataContext.starkContext.w3);
  poorstream_write_bits(&bitstream, 0, 11); // padding
  poorstream_write_bits(&bitstream, (p1 == P1_STARK_ORDER ? STARK_ORDER_TYPE : STARK_TRANSFER_TYPE), 4);
  poorstream_write_bits(&bitstream, U4BE(dataBuffer, offset), 31);
  poorstream_write_bits(&bitstream, U4BE(dataBuffer, offset + 4), 31);
  poorstream_write_bits(&bitstream, U8BE(dataBuffer, offset + 4 + 4), 63);
  if (p1 == P1_STARK_ORDER) {
    poorstream_write_bits(&bitstream, U8BE(dataBuffer, offset + 4 + 4 + 8), 63);
    offset += 4 + 4 + 8 + 8;
  }
  else {
    poorstream_write_bits(&bitstream, 0, 63);
    offset += 4 + 4 + 8;
  }
  poorstream_write_bits(&bitstream, U4BE(dataBuffer, offset), 31);
  poorstream_write_bits(&bitstream, U4BE(dataBuffer, offset + 4), 22);

  PRINTF("stark w1 %.*H\n", 32, dataContext.starkContext.w1);
  PRINTF("stark w2 %.*H\n", 32, dataContext.starkContext.w2);
  PRINTF("stark w3 %.*H\n", 32, dataContext.starkContext.w3);
  // Prepare the UI
  if (p1 == P1_STARK_ORDER) {
    io_seproxyhal_io_heartbeat();
    // amount to sell
    stark_get_amount_string(dataBuffer, dataBuffer + 20, dataBuffer + 20 + 32 + 20 + 32 + 4 + 4, (char*)(dataBuffer + TMP_OFFSET), strings.common.fullAmount);
    io_seproxyhal_io_heartbeat();
    // amount to buy
    stark_get_amount_string(dataBuffer + 20 + 32, dataBuffer + 20 + 32 + 20, dataBuffer + 20 + 32 + 20 + 32 + 4 + 4 + 8, (char*)(dataBuffer + TMP_OFFSET), strings.common.maxFee);
    // src vault ID
    snprintf(strings.common.fullAddress, sizeof(strings.common.fullAddress), "%d", U4BE(dataBuffer, 20 + 32 + 20 + 32));
  }
  else {
    cx_ecfp_public_key_t publicKey;
    // Check if the transfer is a self transfer
    io_seproxyhal_io_heartbeat();
    starkDerivePrivateKey(tmpCtx.transactionContext.bip32Path, bip32PathLength, privateKeyData);
    cx_ecfp_init_private_key(CX_CURVE_Stark256, privateKeyData, 32, &privateKey);
    io_seproxyhal_io_heartbeat();
    cx_ecfp_generate_pair(CX_CURVE_Stark256, &publicKey, &privateKey, 1);
    os_memset(&privateKey, 0, sizeof(privateKey));
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    io_seproxyhal_io_heartbeat();
    selfTransfer = (os_memcmp(publicKey.W + 1, dataBuffer + 20 + 32, 32) == 0);
    PRINTF("self transfer %d\n", selfTransfer);
    io_seproxyhal_io_heartbeat();
    // amount to transfer
    stark_get_amount_string(dataBuffer, dataBuffer + 20, dataBuffer + 20 + 32 + 32 + 4 + 4, (char*)(dataBuffer + TMP_OFFSET), tmpContent.tmp);
    // dest vault ID
    snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "%d", U4BE(dataBuffer, 20 + 32 + 32 + 4));
    if (!selfTransfer) {
      snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "0x%.*H", 32, dataBuffer + 20 + 32);
    }
  }
  ux_flow_init(0, p1 == P1_STARK_ORDER ? ux_stark_limit_order_flow : selfTransfer ?
    ux_stark_self_transfer_flow : ux_stark_transfer_flow, NULL);

  *flags |= IO_ASYNCH_REPLY;
}

#endif
