#include "shared_context.h"
#include "apdu_constants.h"
#ifdef TARGET_BLUE
#include "ui_blue.h"
#endif
#ifdef HAVE_UX_FLOW
#include "ui_flow.h"
#endif
#include "feature_getPublicKey.h"

void handleGetPublicKey(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx) {
  UNUSED(dataLength);
  uint8_t privateKeyData[32];
  uint32_t bip32Path[MAX_BIP32_PATH];
  uint32_t i;
  uint8_t bip32PathLength = *(dataBuffer++);
  cx_ecfp_private_key_t privateKey;
  reset_app_context();
  if ((bip32PathLength < 0x01) ||
      (bip32PathLength > MAX_BIP32_PATH)) {
    PRINTF("Invalid path\n");
    THROW(0x6a80);
  }
  if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
    THROW(0x6B00);
  }
  if ((p2 != P2_CHAINCODE) && (p2 != P2_NO_CHAINCODE)) {
    THROW(0x6B00);
  }
  for (i = 0; i < bip32PathLength; i++) {
    bip32Path[i] = U4BE(dataBuffer, 0);
    dataBuffer += 4;
  }
  tmpCtx.publicKeyContext.getChaincode = (p2 == P2_CHAINCODE);
  io_seproxyhal_io_heartbeat();
  os_perso_derive_node_bip32(CX_CURVE_256K1, bip32Path, bip32PathLength, privateKeyData, (tmpCtx.publicKeyContext.getChaincode ? tmpCtx.publicKeyContext.chainCode : NULL));
  cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
  io_seproxyhal_io_heartbeat();
  cx_ecfp_generate_pair(CX_CURVE_256K1, &tmpCtx.publicKeyContext.publicKey, &privateKey, 1);
  os_memset(&privateKey, 0, sizeof(privateKey));
  os_memset(privateKeyData, 0, sizeof(privateKeyData));
  io_seproxyhal_io_heartbeat();
  getEthAddressStringFromKey(&tmpCtx.publicKeyContext.publicKey, tmpCtx.publicKeyContext.address, &global_sha3, chainConfig);
#ifndef NO_CONSENT
  if (p1 == P1_NON_CONFIRM)
#endif // NO_CONSENT
  {
    *tx = set_result_get_publicKey();
    THROW(0x9000);
  }
#ifndef NO_CONSENT
  else
  {
    /*
    addressSummary[0] = '0';
    addressSummary[1] = 'x';
    os_memmove((unsigned char *)(addressSummary + 2), tmpCtx.publicKeyContext.address, 4);
    os_memmove((unsigned char *)(addressSummary + 6), "...", 3);
    os_memmove((unsigned char *)(addressSummary + 9), tmpCtx.publicKeyContext.address + 40 - 4, 4);
    addressSummary[13] = '\0';
    */

    // prepare for a UI based reply
#if defined(TARGET_BLUE)
    snprintf(strings.common.fullAddress, sizeof(strings.common.fullAddress), "0x%.*s", 40, tmpCtx.publicKeyContext.address);
    UX_DISPLAY(ui_address_blue, ui_address_blue_prepro);
#else
    snprintf(strings.common.fullAddress, sizeof(strings.common.fullAddress), "0x%.*s", 40, tmpCtx.publicKeyContext.address);
    ux_flow_init(0, ux_display_public_flow, NULL);
#endif // #if TARGET_ID

    *flags |= IO_ASYNCH_REPLY;
  }
#endif // NO_CONSENT
}

