#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"

unsigned int io_seproxyhal_touch_stark_ok(const bagl_element_t *e);

UX_FLOW_DEF_NOCB(ux_stark_limit_order_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_FLOW_DEF_NOCB(ux_stark_limit_order_2_step,
    bnnn_paging,
    {
      .title = "Limit",
      .text = "Order"
    });

UX_FLOW_DEF_NOCB(ux_stark_limit_order_3_step,
    bnnn_paging,
    {
      .title = "Trading",
      .text = "Pair"
    });

UX_FLOW_DEF_NOCB(ux_stark_limit_order_4_step,
    bnnn_paging,
    {
      .title = "Sell",
      .text = strings.txSummary.fullAmount
    });

UX_FLOW_DEF_NOCB(ux_stark_limit_order_5_step,
    bnnn_paging,
    {
      .title = "Buy",
      .text = strings.txSummary.maxFee
    });

UX_FLOW_DEF_NOCB(ux_stark_limit_order_6_step,
    bnnn_paging,
    {
      .title = "Token Accont",
      .text = strings.txSummary.fullAddress
    });

UX_FLOW_DEF_VALID(
    ux_stark_limit_order_7_step,
    pbb,
    io_seproxyhal_touch_stark_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_FLOW_DEF_VALID(
    ux_stark_limit_order_8_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_stark_limit_order_flow [] = {
  &ux_stark_limit_order_1_step,
  &ux_stark_limit_order_2_step,
  &ux_stark_limit_order_3_step,
  &ux_stark_limit_order_4_step,
  &ux_stark_limit_order_5_step,
  &ux_stark_limit_order_6_step,
  &ux_stark_limit_order_7_step,
  &ux_stark_limit_order_8_step,
  FLOW_END_STEP,
};

//////////////////////////////////////////////////////////////////////
UX_FLOW_DEF_NOCB(ux_stark_transfer_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_FLOW_DEF_NOCB(ux_stark_transfer_2_step,
    bnnn_paging,
    {
      .title = "Transfer",
      .text = ""
    });

UX_FLOW_DEF_NOCB(ux_stark_self_transfer_2_step,
    bnnn_paging,
    {
      .title = "Self",
      .text = "Transfer"
    });


UX_FLOW_DEF_NOCB(ux_stark_transfer_3_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = tmpContent.tmp
    });

UX_FLOW_DEF_NOCB(ux_stark_transfer_4_step,
    bnnn_paging,
    {
      .title = "Master Account",
      .text = strings.tmp.tmp
    });

UX_FLOW_DEF_NOCB(ux_stark_transfer_5_step,
    bnnn_paging,
    {
      .title = "Token Accont",
      .text = strings.tmp.tmp2
    });

UX_FLOW_DEF_VALID(
    ux_stark_transfer_6_step,
    pbb,
    io_seproxyhal_touch_stark_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_FLOW_DEF_VALID(
    ux_stark_transfer_7_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_stark_transfer_flow [] = {
  &ux_stark_transfer_1_step,
  &ux_stark_transfer_2_step,
  &ux_stark_transfer_3_step,
  &ux_stark_transfer_4_step,
  &ux_stark_transfer_5_step,
  &ux_stark_transfer_6_step,
  &ux_stark_transfer_7_step,
  FLOW_END_STEP,
};

const ux_flow_step_t *        const ux_stark_self_transfer_flow [] = {
  &ux_stark_transfer_1_step,
  &ux_stark_self_transfer_2_step,
  &ux_stark_transfer_3_step,
  &ux_stark_transfer_5_step,
  &ux_stark_transfer_6_step,
  &ux_stark_transfer_7_step,
  FLOW_END_STEP,
};

#endif
