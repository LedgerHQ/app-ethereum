#include "shared_context.h"
#include "ui_callbacks.h"

void display_settings(const ux_flow_step_t* const start_step);
void switch_settings_blind_signing(void);
void switch_settings_display_data(void);
void switch_settings_display_nonce(void);

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
    switch_settings_blind_signing(),
    {
      .title = "Blind signing",
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

#else

UX_STEP_CB(
    ux_settings_flow_1_step,
    bnnn,
    switch_settings_blind_signing(),
    {
      "Blind signing",
      "Enable transaction",
      "blind signing",
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

#endif

UX_STEP_CB(
    ux_settings_flow_4_step,
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
        &ux_settings_flow_4_step);

void display_settings(const ux_flow_step_t* const start_step) {
    strlcpy(strings.common.fullAddress, (N_storage.dataAllowed ? "Enabled" : "NOT Enabled"), 12);
    strlcpy(strings.common.fullAddress + 12,
            (N_storage.contractDetails ? "Displayed" : "NOT Displayed"),
            26 - 12);
    strlcpy(strings.common.fullAddress + 26,
            (N_storage.displayNonce ? "Displayed" : "NOT Displayed"),
            sizeof(strings.common.fullAddress) - 26);
    ux_flow_init(0, ux_settings_flow, start_step);
}

void switch_settings_blind_signing() {
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

//////////////////////////////////////////////////////////////////////
// clang-format off
#if defined(TARGET_NANOS)
UX_STEP_CB(
    ux_warning_contract_data_step,
    bnnn_paging,
    ui_idle(),
    {
      "Error",
      "Blind signing must be enabled in Settings",
    });
#elif defined(TARGET_NANOX) || defined(TARGET_NANOS2)
UX_STEP_CB(
    ux_warning_contract_data_step,
    pnn,
    ui_idle(),
    {
      &C_icon_crossmark,
      "Blind signing must be",
      "enabled in Settings",
    });
#endif
// clang-format on

UX_FLOW(ux_warning_contract_data_flow, &ux_warning_contract_data_step);