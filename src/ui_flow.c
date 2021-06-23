#include "shared_context.h"
#include "ui_callbacks.h"

void display_settings(const ux_flow_step_t* const start_step);
void switch_settings_contract_data(void);
void switch_settings_display_data(void);
void switch_settings_display_nonce(void);
void switch_settings_display_fee_details(void);

//////////////////////////////////////////////////////////////////////
// clang-format off
UX_STEP_NOCB(
    ux_idle_flow_1_step,
    nn, //pnn,
    {
      //"", //&C_icon_dashboard,
      "Application",
      "is ready",
    });
UX_STEP_NOCB(
    ux_idle_flow_2_step,
    bn,
    {
      "Version",
      APPVERSION,
    });
UX_STEP_CB(
    ux_idle_flow_3_step,
    pb,
    display_settings(NULL),
    {
      &C_icon_eye,
      "Settings",
    });
UX_STEP_CB(
    ux_idle_flow_4_step,
    pb,
    os_sched_exit(-1),
    {
      &C_icon_dashboard_x,
      "Quit",
    });
// clang-format on

UX_FLOW(ux_idle_flow,
        &ux_idle_flow_1_step,
        &ux_idle_flow_2_step,
        &ux_idle_flow_3_step,
        &ux_idle_flow_4_step,
        FLOW_LOOP);

#if defined(TARGET_NANOS)

// clang-format off
UX_STEP_CB(
    ux_settings_flow_1_step,
    bnnn_paging,
    switch_settings_contract_data(),
    {
      .title = "Contract data",
      .text = strings.common.fullAddress,
    });

UX_STEP_CB(
    ux_settings_flow_2_step,
    bnnn_paging,
    switch_settings_display_data(),
    {
      .title = "Debug data",
      .text = strings.common.fullAddress + 12
    });

UX_STEP_CB(
    ux_settings_flow_3_step,
    bnnn_paging,
    switch_settings_display_nonce(),
    {
      .title = "Account nonce",
      .text = strings.common.fullAddress + 26
    });

UX_STEP_CB(
    ux_settings_flow_4_step,
    bnnn_paging,
    switch_settings_display_fee_details(),
    {
      .title = "Fee Details",
      .text = strings.common.fullAddress + 40
    });

#else

UX_STEP_CB(
    ux_settings_flow_1_step,
    bnnn,
    switch_settings_contract_data(),
    {
      "Contract data",
      "Allow contract data",
      "in transactions",
      strings.common.fullAddress,
    });

UX_STEP_CB(
    ux_settings_flow_2_step,
    bnnn,
    switch_settings_display_data(),
    {
      "Debug data",
      "Display contract data",
      "details",
      strings.common.fullAddress + 12
    });

  UX_STEP_CB(
    ux_settings_flow_3_step,
    bnnn,
    switch_settings_display_nonce(),
    {
      "Nonce",
      "Display account nonce",
      "in transactions",
      strings.common.fullAddress + 26
    });

  UX_STEP_CB(
    ux_settings_flow_4_step,
    bnnn,
    switch_settings_display_fee_details(),
    {
      "Fee Details",
      "Display fee details",
      "when available",
      strings.common.fullAddress + 40
    });

#endif

UX_STEP_CB(
    ux_settings_flow_5_step,
    pb,
    ui_idle(),
    {
      &C_icon_back_x,
      "Back",
    });
// clang-format on

UX_FLOW(ux_settings_flow,
        &ux_settings_flow_1_step,
        &ux_settings_flow_2_step,
        &ux_settings_flow_3_step,
        &ux_settings_flow_4_step,
        &ux_settings_flow_5_step);

void display_settings(const ux_flow_step_t* const start_step) {
    strcpy(strings.common.fullAddress, (N_storage.dataAllowed ? "Allowed" : "NOT Allowed"));
    strcpy(strings.common.fullAddress + 12,
           (N_storage.contractDetails ? "Displayed" : "NOT Displayed"));
    strcpy(strings.common.fullAddress + 26,
           (N_storage.displayNonce ? "Displayed" : "NOT Displayed"));
    strcpy(strings.common.fullAddress + 40,
           (N_storage.displayFeeDetails ? "Displayed" : "NOT Displayed"));
    ux_flow_init(0, ux_settings_flow, start_step);
}

void switch_settings_contract_data() {
    uint8_t value = (N_storage.dataAllowed ? 0 : 1);
    nvm_write((void*) &N_storage.dataAllowed, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_flow_1_step);
}

void switch_settings_display_data() {
    uint8_t value = (N_storage.contractDetails ? 0 : 1);
    nvm_write((void*) &N_storage.contractDetails, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_flow_2_step);
}

void switch_settings_display_nonce() {
    uint8_t value = (N_storage.displayNonce ? 0 : 1);
    nvm_write((void*) &N_storage.displayNonce, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_flow_3_step);
}

void switch_settings_display_fee_details() {
    uint8_t value = (N_storage.displayFeeDetails ? 0 : 1);
    nvm_write((void*) &N_storage.displayFeeDetails, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_flow_4_step);
}