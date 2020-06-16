#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"

void prepare_verify_escape_3() {  
    uint8_t address[41];
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, &global_sha3, chainConfig);
    strings.txSummary.fullAddress[0] = '0';
    strings.txSummary.fullAddress[1] = 'x';
    os_memmove((unsigned char *)strings.txSummary.fullAddress+2, address, 40);
    strings.txSummary.fullAddress[42] = '\0';
}

UX_FLOW_DEF_NOCB(ux_approval_starkware_verify_escape_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_FLOW_DEF_NOCB(
    ux_approval_starkware_verify_escape_2_step,
    bnnn_paging,
    {
      .title = "Verify Escape",
      .text = ""
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_verify_escape_3_step,
    bnnn_paging,
    prepare_verify_escape_3(),
    {
      .title = "Contract Name",
      .text = strings.txSummary.fullAddress,
    });

UX_FLOW_DEF_NOCB(
    ux_approval_starkware_verify_escape_4_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.txSummary.maxFee,
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_verify_escape_5_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_verify_escape_6_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_approval_starkware_verify_escape_flow [] = {
  &ux_approval_starkware_verify_escape_1_step,
  &ux_approval_starkware_verify_escape_2_step,
  &ux_approval_starkware_verify_escape_3_step,
  &ux_approval_starkware_verify_escape_4_step,
  &ux_approval_starkware_verify_escape_5_step,
  &ux_approval_starkware_verify_escape_6_step,
  FLOW_END_STEP,
};

#endif
