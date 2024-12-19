#ifdef HAVE_GENERIC_TX_PARSER

#include "ux.h"
#include "gtp_field_table.h"
#include "gtp_tx_info.h"
#include "feature_signTx.h"
#include "ui_callbacks.h"
#include "network.h"
#include "apdu_constants.h"

// forward declaration for BAGL step
static void review_contract_info(void);
static void review(int index);
static void dyn_prev(void);
static void dyn_next(void);
static void switch_pre_dyn(void);
static void switch_post_dyn(void);
static void prepare_review_title(void);
static void switch_network(bool forward);
static void network_prev(void);
static void network_next(void);

static void format_network(void) {
    if (get_network_as_string(strings.tmp.tmp, sizeof(strings.tmp.tmp)) != APDU_RESPONSE_OK) {
        PRINTF("Error: Could not format the network!\n");
    }
}

static void format_fees(void) {
    if (max_transaction_fee_to_string(&tmpContent.txContent.gasprice,
                                      &tmpContent.txContent.startgas,
                                      strings.tmp.tmp,
                                      sizeof(strings.tmp.tmp)) == false) {
        PRINTF("Error: Could not format the max fees!\n");
    }
}

// clang-format off
UX_STEP_NOCB_INIT(
    ui_gcs_review_step,
    pnn,
    prepare_review_title(),
    {
      &C_icon_eye,
      strings.tmp.tmp2,
      strings.tmp.tmp,
    }
);
UX_STEP_CB(
    ui_gcs_contract_info_btn_step,
    pbb,
    review_contract_info(),
    {
      &C_icon_certificate,
      "See contract",
      "information",
    }
);
UX_STEP_CB(
    ui_gcs_back_step,
    pb,
    review(1),
    {
      &C_icon_back_x,
      "Back",
    }
);
UX_STEP_INIT(
    ui_gcs_switch_pre_network_step,
    NULL,
    NULL,
    {
        switch_network(true);
    }
);
UX_STEP_INIT(
    ui_gcs_network_prev_step,
    NULL,
    NULL,
    {
        network_prev();
    }
);
UX_STEP_NOCB_INIT(
    ui_gcs_network_step,
    bnnn_paging,
    format_network(),
    {
      .title = "Network",
      .text = strings.tmp.tmp,
    }
);
UX_STEP_INIT(
    ui_gcs_network_next_step,
    NULL,
    NULL,
    {
        network_next();
    }
);
UX_STEP_INIT(
    ui_gcs_switch_post_network_step,
    NULL,
    NULL,
    {
        switch_network(false);
    }
);
UX_STEP_NOCB_INIT(
    ui_gcs_fees_step,
    bnnn_paging,
    format_fees(),
    {
      .title = "Max fees",
      .text = strings.tmp.tmp,
    }
);
UX_STEP_CB(
    ui_gcs_accept_step,
    pb,
    tx_ok_cb(),
    {
      &C_icon_validate_14,
      "Sign transaction",
    }
);
UX_STEP_CB(
    ui_gcs_reject_step,
    pb,
    tx_cancel_cb(),
    {
      &C_icon_crossmark,
      "Reject",
    }
);

UX_STEP_INIT(
    ui_gcs_dyn_prev_step,
    NULL,
    NULL,
    {
        dyn_prev();
    }
);
UX_STEP_NOCB(
    ui_gcs_dyn_step,
    bnnn_paging,
    {
      .title = strings.tmp.tmp2,
      .text = strings.tmp.tmp,
    }
);
UX_STEP_INIT(
    ui_gcs_dyn_next_step,
    NULL,
    NULL,
    {
        dyn_next();
    }
);

UX_STEP_INIT(
    ui_gcs_switch_pre_dyn_step,
    NULL,
    NULL,
    {
        switch_pre_dyn();
    }
);
UX_STEP_INIT(
    ui_gcs_switch_post_dyn_step,
    NULL,
    NULL,
    {
        switch_post_dyn();
    }
);
// clang-format on

UX_FLOW(ui_gcs_flow,
        &ui_gcs_review_step,
        &ui_gcs_contract_info_btn_step,

        // dynamic field handling
        &ui_gcs_switch_pre_dyn_step,
        &ui_gcs_dyn_prev_step,
        &ui_gcs_dyn_step,
        &ui_gcs_dyn_next_step,
        &ui_gcs_switch_post_dyn_step,

        // network handling
        &ui_gcs_switch_pre_network_step,
        &ui_gcs_network_prev_step,
        &ui_gcs_network_step,
        &ui_gcs_network_next_step,
        &ui_gcs_switch_post_network_step,

        &ui_gcs_fees_step,
        &ui_gcs_accept_step,
        &ui_gcs_reject_step);

UX_FLOW(ui_gcs_contract_info_beg_flow, &ui_gcs_switch_pre_dyn_step);

UX_FLOW(ui_gcs_dyn_beg_flow, &ui_gcs_dyn_step, &ui_gcs_dyn_next_step);

UX_FLOW(ui_gcs_dyn_middle_flow, &ui_gcs_dyn_prev_step, &ui_gcs_dyn_step, &ui_gcs_dyn_next_step);

UX_FLOW(ui_gcs_contract_info_end_flow, &ui_gcs_switch_post_dyn_step, &ui_gcs_back_step);

typedef enum {
    TOP_LEVEL,
    TX_INFO,
} e_level;

static e_level level;
static uint8_t dyn_idx;

static void prepare_review_title(void) {
    const char *op_type;

    snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "Review transaction to");
    if ((op_type = get_operation_type()) != NULL) {
        snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "%s", op_type);
    }
}

static bool prepare_kv_info(uint8_t idx) {
    bool found = false;
    uint8_t count = 0;
    const char *value;

    const char *value2;
    if ((value = get_creator_legal_name()) != NULL) {
        count += 1;
        if (count == idx) {
            snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "Contract owner");
            if ((value2 = get_creator_url()) != NULL) {
                snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "%s\n%s", value, value2);
            } else {
                snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "%s", value);
            }
            found = true;
        }
    }

    if ((value = get_contract_name()) != NULL) {
        count += 1;
        if (count == idx) {
            snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "Contract");
            snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "%s", value);
            found = true;
        }
    }

    const uint8_t *addr = get_contract_addr();
    count += 1;
    if (count == idx) {
        snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "Contract address");
        if (!getEthDisplayableAddress((uint8_t *) addr,
                                      strings.tmp.tmp,
                                      sizeof(strings.tmp.tmp),
                                      chainConfig->chainId)) {
            return false;
        }
        found = true;
    }

    if ((value = get_deploy_date()) != NULL) {
        count += 1;
        if (count == idx) {
            snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "Deployed on");
            snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "%s", value);
            found = true;
        }
    }
    return found;
}

static bool prepare_key_value(uint8_t idx) {
    bool found = false;
    s_field_table_entry field;

    switch (level) {
        case TX_INFO:
            found = prepare_kv_info(idx);
            break;
        case TOP_LEVEL:
            if ((found = get_from_field_table(idx - 1, &field))) {
                strncpy(strings.tmp.tmp2, field.key, sizeof(strings.tmp.tmp2));
                strncpy(strings.tmp.tmp, field.value, sizeof(strings.tmp.tmp));
            }
            break;
        default:
            break;
    }
    return found;
}

static void dyn_prev(void) {
    const ux_flow_step_t *const *flow = NULL;
    const ux_flow_step_t *step = NULL;

    dyn_idx -= 1;
    if (prepare_key_value(dyn_idx)) {
        // found
        switch (level) {
            case TX_INFO:
                flow = (dyn_idx == 1) ? ui_gcs_dyn_beg_flow : ui_gcs_dyn_middle_flow;
                break;
            case TOP_LEVEL:
                flow = ui_gcs_flow;
                break;
            default:
                break;
        }
        step = &ui_gcs_dyn_step;
    } else {
        // not found
        switch (level) {
            case TX_INFO:
                flow = ui_gcs_contract_info_end_flow;
                step = &ui_gcs_back_step;
                break;
            case TOP_LEVEL:
                flow = ui_gcs_flow;
                step = &ui_gcs_contract_info_btn_step;
                break;
            default:
                break;
        }
    }
    ux_flow_init(0, flow, step);
}

static void dyn_next(void) {
    const ux_flow_step_t *const *flow = NULL;
    const ux_flow_step_t *step = NULL;

    dyn_idx += 1;
    if (prepare_key_value(dyn_idx)) {
        // found
        switch (level) {
            case TX_INFO:
                flow = (dyn_idx == 1) ? ui_gcs_dyn_beg_flow : ui_gcs_dyn_middle_flow;
                step = &ui_gcs_dyn_step;
                break;
            case TOP_LEVEL:
                flow = ui_gcs_flow;
                step = &ui_gcs_dyn_step;
                break;
            default:
                break;
        }
    } else {
        // not found
        dyn_idx -= 1;
        switch (level) {
            case TX_INFO:
                flow = ui_gcs_contract_info_end_flow;
                step = &ui_gcs_back_step;
                break;
            case TOP_LEVEL:
                flow = ui_gcs_flow;
                step = &ui_gcs_switch_pre_network_step;
                break;
            default:
                break;
        }
    }
    ux_flow_init(0, flow, step);
}

static void switch_pre_dyn(void) {
    ux_flow_init(0, ui_gcs_flow, &ui_gcs_dyn_next_step);
}

static void switch_post_dyn(void) {
    const ux_flow_step_t *const *flow = NULL;
    const ux_flow_step_t *step = NULL;

    switch (level) {
        case TX_INFO:
            flow = ui_gcs_dyn_middle_flow;
            step = &ui_gcs_dyn_step;
            break;
        case TOP_LEVEL:
            if (dyn_idx == 0) {
                // no field has been shown
                step = &ui_gcs_contract_info_btn_step;
            } else {
                // artificially lower the index then ask for the next one
                dyn_idx -= 1;
                step = &ui_gcs_dyn_next_step;
            }
            flow = ui_gcs_flow;
            break;
        default:
            break;
    }
    ux_flow_init(0, flow, step);
}

static void network_prev(void) {
    ux_flow_init(0, ui_gcs_flow, &ui_gcs_switch_post_dyn_step);
}

static void network_next(void) {
    ux_flow_init(0, ui_gcs_flow, &ui_gcs_fees_step);
}

static void switch_network(bool forward) {
    uint64_t chain_id = get_tx_chain_id();

    if (chain_id != chainConfig->chainId) {
        ux_flow_init(0, ui_gcs_flow, &ui_gcs_network_step);
    } else {
        if (forward) {
            network_next();
        } else {
            network_prev();
        }
    }
}

static void review_contract_info(void) {
    dyn_idx = 0;
    level = TX_INFO;
    ux_flow_init(0, ui_gcs_dyn_middle_flow, &ui_gcs_dyn_next_step);
}

static void review(int index) {
    dyn_idx = 0;
    level = TOP_LEVEL;
    ux_flow_init(0, ui_gcs_flow, ui_gcs_flow[index]);
}

bool ui_gcs(void) {
    review(0);
    return true;
}

void ui_gcs_cleanup(void) {
}

#endif  // HAVE_GENERIC_TX_PARSER
