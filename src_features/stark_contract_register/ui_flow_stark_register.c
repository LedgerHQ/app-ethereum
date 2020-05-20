#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"

void prepare_register_3() {  
    uint8_t address[41];
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, &sha3, chainConfig);
    strings.common.fullAddress[0] = '0';
    strings.common.fullAddress[1] = 'x';
    os_memmove((unsigned char *)strings.common.fullAddress+2, address, 40);
    strings.common.fullAddress[42] = '\0';
}

void prepare_register_4() {  
  uint8_t privateKeyData[32];
  uint8_t address[41];
  cx_ecfp_private_key_t privateKey;
  cx_ecfp_public_key_t publicKey;
  os_perso_derive_node_bip32(CX_CURVE_256K1, tmpCtx.transactionContext.bip32Path,
                               tmpCtx.transactionContext.pathLength,
                               privateKeyData, NULL);
  cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
  io_seproxyhal_io_heartbeat();
  cx_ecfp_generate_pair(CX_CURVE_256K1, &publicKey, &privateKey, 1);
  os_memset(&privateKey, 0, sizeof(privateKey));
  os_memset(privateKeyData, 0, sizeof(privateKeyData));
  io_seproxyhal_io_heartbeat();
  getEthAddressStringFromKey(&publicKey, address, &sha3, chainConfig);
  strings.common.fullAddress[0] = '0';
  strings.common.fullAddress[1] = 'x';
  os_memmove((unsigned char *)strings.common.fullAddress+2, address, 40);
  strings.common.fullAddress[42] = '\0';
}

void prepare_register_5() {  
  snprintf(strings.tmp.tmp, 70, "0x%.*H", 32, dataContext.tokenContext.data + 4);  
}

UX_FLOW_DEF_NOCB(ux_approval_starkware_register_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_FLOW_DEF_NOCB(
    ux_approval_starkware_register_2_step,
    bnnn_paging,
    {
      .title = "Registration",
      .text = ""
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_register_3_step,
    bnnn_paging,
    prepare_register_3(),
    {
      .title = "Contract Name",
      .text = strings.common.fullAddress,
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_register_4_step,
    bnnn_paging,
    prepare_register_4(),
    {
      .title = "From ETH address",
      .text = strings.common.fullAddress
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_register_5_step,
    bnnn_paging,
    prepare_register_5(),
    {
      .title = "Master account",
      .text = strings.tmp.tmp
    });


UX_FLOW_DEF_NOCB(
    ux_approval_starkware_register_6_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_register_7_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_register_8_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_approval_starkware_register_flow [] = {
  &ux_approval_starkware_register_1_step,
  &ux_approval_starkware_register_2_step,
  &ux_approval_starkware_register_3_step,
  &ux_approval_starkware_register_4_step,
  &ux_approval_starkware_register_5_step,
  &ux_approval_starkware_register_6_step,
  &ux_approval_starkware_register_7_step,
  &ux_approval_starkware_register_8_step,
  FLOW_END_STEP,
};

#endif

