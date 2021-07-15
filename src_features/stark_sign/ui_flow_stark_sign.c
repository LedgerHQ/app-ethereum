#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"

unsigned int io_seproxyhal_touch_stark_ok(const bagl_element_t *e);

void stark_sign_display_master_account() {
    snprintf(strings.tmp.tmp,
             sizeof(strings.tmp.tmp),
             "0x%.*H",
             32,
             dataContext.starkContext.transferDestination);
}

void stark_sign_display_condition_address() {
    strings.tmp.tmp[0] = '0';
    strings.tmp.tmp[1] = 'x';
    getEthAddressStringFromBinary(dataContext.starkContext.conditionAddress,
                                  strings.tmp.tmp + 2,
                                  &global_sha3,
                                  chainConfig);
    strings.tmp.tmp[42] = '\0';
}

void stark_sign_display_condition_fact() {
    snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "0x%.*H", 32, dataContext.starkContext.fact);
}

// clang-format off
UX_STEP_NOCB(ux_stark_limit_order_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_STEP_NOCB(ux_stark_limit_order_2_step,
    bnnn_paging,
    {
      .title = "Limit",
      .text = "Order"
    });

UX_STEP_NOCB(ux_stark_limit_order_3_step,
    bnnn_paging,
    {
      .title = "Trading",
      .text = "Pair"
    });

UX_STEP_NOCB(ux_stark_limit_order_4_step,
    bnnn_paging,
    {
      .title = "Sell",
      .text = strings.common.fullAmount
    });

UX_STEP_NOCB(ux_stark_limit_order_5_step,
    bnnn_paging,
    {
      .title = "Buy",
      .text = strings.common.maxFee
    });

UX_STEP_NOCB(ux_stark_limit_order_6_step,
    bnnn_paging,
    {
      .title = "Token Account",
      .text = strings.common.fullAddress
    });

UX_STEP_CB(
    ux_stark_limit_order_7_step,
    pbb,
    io_seproxyhal_touch_stark_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_STEP_CB(
    ux_stark_limit_order_8_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_stark_limit_order_flow,
        &ux_stark_limit_order_1_step,
        &ux_stark_limit_order_2_step,
        &ux_stark_limit_order_3_step,
        &ux_stark_limit_order_4_step,
        &ux_stark_limit_order_5_step,
        &ux_stark_limit_order_6_step,
        &ux_stark_limit_order_7_step,
        &ux_stark_limit_order_8_step);

//////////////////////////////////////////////////////////////////////
// clang-format off
UX_STEP_NOCB(ux_stark_transfer_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_STEP_NOCB(ux_stark_transfer_2_step,
    bnnn_paging,
    {
      .title = "Transfer",
      .text = " "
    });

UX_STEP_NOCB(ux_stark_self_transfer_2_step,
    bnnn_paging,
    {
      .title = "Self",
      .text = "Transfer"
    });

UX_STEP_NOCB(ux_stark_conditional_transfer_2_step,
    bnnn_paging,
    {
      .title = "Conditional",
      .text = "Transfer"
    });

UX_STEP_NOCB(ux_stark_self_conditional_transfer_2_step,
    bnnn_paging,
    {
      .title = "Conditional",
      .text = "Self Transfer"
    });

UX_STEP_NOCB(ux_stark_transfer_3_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = tmpContent.tmp
    });

UX_STEP_NOCB(ux_stark_transfer_4_step,
    bnnn_paging,
    {
      .title = "Master Account",
      .text = strings.tmp.tmp
    });

UX_STEP_NOCB(ux_stark_transfer_5_step,
    bnnn_paging,
    {
      .title = "Token Account",
      .text = strings.tmp.tmp2
    });

UX_STEP_CB(
    ux_stark_transfer_6_step,
    pbb,
    io_seproxyhal_touch_stark_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_STEP_CB(
    ux_stark_transfer_7_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_STEP_NOCB_INIT(
    ux_stark_conditional_transfer_4_step,
    bnnn_paging,
    stark_sign_display_master_account(),
    {
      .title = "Master Account",
      .text = strings.tmp.tmp
    });

UX_STEP_NOCB_INIT(
    ux_stark_conditional_transfer_8_step,
    bnnn_paging,
    stark_sign_display_condition_address(),
    {
      .title = "Cond. Address",
      .text = strings.tmp.tmp
    });

UX_STEP_NOCB_INIT(
    ux_stark_conditional_transfer_9_step,
    bnnn_paging,
    stark_sign_display_condition_fact(),
    {
      .title = "Cond. Fact",
      .text = strings.tmp.tmp
    });
// clang-format on

UX_FLOW(ux_stark_transfer_flow,
        &ux_stark_transfer_1_step,
        &ux_stark_transfer_2_step,
        &ux_stark_transfer_3_step,
        &ux_stark_transfer_4_step,
        &ux_stark_transfer_5_step,
        &ux_stark_transfer_6_step,
        &ux_stark_transfer_7_step);

UX_FLOW(ux_stark_self_transfer_flow,
        &ux_stark_transfer_1_step,
        &ux_stark_self_transfer_2_step,
        &ux_stark_transfer_3_step,
        &ux_stark_transfer_5_step,
        &ux_stark_transfer_6_step,
        &ux_stark_transfer_7_step);

UX_FLOW(ux_stark_transfer_conditional_flow,
        &ux_stark_transfer_1_step,
        &ux_stark_conditional_transfer_2_step,
        &ux_stark_transfer_3_step,
        &ux_stark_conditional_transfer_4_step,
        &ux_stark_transfer_5_step,
        &ux_stark_conditional_transfer_8_step,
        &ux_stark_conditional_transfer_9_step,
        &ux_stark_transfer_6_step,
        &ux_stark_transfer_7_step);

UX_FLOW(ux_stark_self_transfer_conditional_flow,
        &ux_stark_transfer_1_step,
        &ux_stark_self_conditional_transfer_2_step,
        &ux_stark_transfer_3_step,
        &ux_stark_transfer_5_step,
        &ux_stark_conditional_transfer_8_step,
        &ux_stark_conditional_transfer_9_step,
        &ux_stark_transfer_6_step,
        &ux_stark_transfer_7_step);

#endif
