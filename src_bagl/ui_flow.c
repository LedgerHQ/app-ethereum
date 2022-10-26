#include "shared_context.h"
#include "ui_callbacks.h"
#include "common_ui.h"
#include "utils.h"

#define ENABLED_STR   "Enabled"
#define DISABLED_STR  "Disabled"
#define BUF_INCREMENT (MAX(strlen(ENABLED_STR), strlen(DISABLED_STR)) + 1)

void display_settings(const ux_flow_step_t* const start_step);
void switch_settings_blind_signing(void);
void switch_settings_display_data(void);
void switch_settings_display_nonce(void);
#ifdef HAVE_EIP712_FULL_SUPPORT
void switch_settings_verbose_eip712(void);
#endif  // HAVE_EIP712_FULL_SUPPORT

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

// clang-format off
UX_STEP_CB(
    ux_settings_flow_blind_signing_step,
#ifdef TARGET_NANOS
    bnnn_paging,
#else
    bnnn,
#endif
    switch_settings_blind_signing(),
    {
#ifdef TARGET_NANOS
      .title = "Blind signing",
      .text =
#else
      "Blind signing",
      "Transaction",
      "blind signing",
#endif
      strings.common.fullAddress
    });

UX_STEP_CB(
    ux_settings_flow_display_data_step,
#ifdef TARGET_NANOS
    bnnn_paging,
#else
    bnnn,
#endif
    switch_settings_display_data(),
    {
#ifdef TARGET_NANOS
      .title = "Debug data",
      .text =
#else
      "Debug data",
      "Show contract data",
      "details",
#endif
      strings.common.fullAddress + BUF_INCREMENT
    });

UX_STEP_CB(
    ux_settings_flow_display_nonce_step,
#ifdef TARGET_NANOS
    bnnn_paging,
#else
    bnnn,
#endif
    switch_settings_display_nonce(),
    {
#ifdef TARGET_NANOS
      .title = "Account nonce",
      .text =
#else
      "Nonce",
      "Show account nonce",
      "in transactions",
#endif
      strings.common.fullAddress + (BUF_INCREMENT * 2)
    });

#ifdef HAVE_EIP712_FULL_SUPPORT
UX_STEP_CB(
    ux_settings_flow_verbose_eip712_step,
    bnnn,
    switch_settings_verbose_eip712(),
    {
      "Verbose EIP-712",
      "Ignore filtering &",
      "display raw content",
      strings.common.fullAddress + (BUF_INCREMENT * 3)
    });
#endif // HAVE_EIP712_FULL_SUPPORT


UX_STEP_CB(
    ux_settings_flow_back_step,
    pb,
    ui_idle(),
    {
      &C_icon_back_x,
      "Back",
    });
// clang-format on

UX_FLOW(ux_settings_flow,
        &ux_settings_flow_blind_signing_step,
        &ux_settings_flow_display_data_step,
        &ux_settings_flow_display_nonce_step,
#ifdef HAVE_EIP712_FULL_SUPPORT
        &ux_settings_flow_verbose_eip712_step,
#endif  // HAVE_EIP712_FULL_SUPPORT
        &ux_settings_flow_back_step);

void display_settings(const ux_flow_step_t* const start_step) {
    bool settings[] = {N_storage.dataAllowed,
                       N_storage.contractDetails,
                       N_storage.displayNonce,
#ifdef HAVE_EIP712_FULL_SUPPORT
                       N_storage.verbose_eip712
#endif  // HAVE_EIP712_FULL_SUPPORT
    };
    uint8_t offset = 0;

    for (unsigned int i = 0; i < ARRAY_SIZE(settings); ++i) {
        strlcpy(strings.common.fullAddress + offset,
                (settings[i] ? ENABLED_STR : DISABLED_STR),
                sizeof(strings.common.fullAddress) - offset);
        offset += BUF_INCREMENT;
    }

    ux_flow_init(0, ux_settings_flow, start_step);
}

void switch_settings_blind_signing(void) {
    uint8_t value = (N_storage.dataAllowed ? 0 : 1);
    nvm_write((void*) &N_storage.dataAllowed, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_flow_blind_signing_step);
}

void switch_settings_display_data(void) {
    uint8_t value = (N_storage.contractDetails ? 0 : 1);
    nvm_write((void*) &N_storage.contractDetails, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_flow_display_data_step);
}

void switch_settings_display_nonce(void) {
    uint8_t value = (N_storage.displayNonce ? 0 : 1);
    nvm_write((void*) &N_storage.displayNonce, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_flow_display_nonce_step);
}

#ifdef HAVE_EIP712_FULL_SUPPORT
void switch_settings_verbose_eip712(void) {
    bool value = !N_storage.verbose_eip712;
    nvm_write((void*) &N_storage.verbose_eip712, (void*) &value, sizeof(value));
    display_settings(&ux_settings_flow_verbose_eip712_step);
}
#endif  // HAVE_EIP712_FULL_SUPPORT

//////////////////////////////////////////////////////////////////////
// clang-format off
#ifdef TARGET_NANOS
UX_STEP_CB(
    ux_warning_contract_data_step,
    bnnn_paging,
    ui_idle(),
    {
      "Error",
      "Blind signing must be enabled in Settings",
    });
#else
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
