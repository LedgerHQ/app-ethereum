#include "shared_context.h"
#include "ui_callbacks.h"
#include "common_ui.h"
#include "utils.h"

#define ENABLED_STR   "Enabled"
#define DISABLED_STR  "Disabled"
#define BUF_INCREMENT (MAX(strlen(ENABLED_STR), strlen(DISABLED_STR)) + 1)

static void display_settings(const ux_flow_step_t* const start_step);
static void switch_settings_blind_signing(void);
static void switch_settings_display_data(void);
static void switch_settings_display_nonce(void);
#ifdef HAVE_EIP712_FULL_SUPPORT
static void switch_settings_verbose_eip712(void);
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_TRUSTED_NAME
static void switch_settings_verbose_trusted_name(void);
#endif  // HAVE_TRUSTED_NAME

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
      strings.common.fullAmount
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
      strings.common.fullAmount + BUF_INCREMENT
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
      strings.common.fullAmount + (BUF_INCREMENT * 2)
    });

#ifdef HAVE_TRUSTED_NAME
UX_STEP_CB(
    ux_settings_flow_verbose_trusted_name_step,
#ifdef TARGET_NANOS
    bnnn_paging,
#else
    bnnn,
#endif
    switch_settings_verbose_trusted_name(),
    {
#ifdef TARGET_NANOS
      .title = "Verbose ENS",
      .text =
#else
      "Verbose ENS",
      "Show addresses",
      "alongside domains",
#endif
      strings.common.fullAmount + (BUF_INCREMENT * 3)
    });
#endif // HAVE_TRUSTED_NAME

#ifdef HAVE_EIP712_FULL_SUPPORT
UX_STEP_CB(
    ux_settings_flow_verbose_eip712_step,
    bnnn,
    switch_settings_verbose_eip712(),
    {
      "Verbose EIP-712",
      "Ignore filtering &",
      "display raw content",
      strings.common.fullAmount + (BUF_INCREMENT * 4)
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
#ifdef HAVE_TRUSTED_NAME
        &ux_settings_flow_verbose_trusted_name_step,
#endif  // HAVE_TRUSTED_NAME
#ifdef HAVE_EIP712_FULL_SUPPORT
        &ux_settings_flow_verbose_eip712_step,
#endif  // HAVE_EIP712_FULL_SUPPORT
        &ux_settings_flow_back_step);

static void display_settings(const ux_flow_step_t* const start_step) {
    bool settings[] = {
        N_storage.dataAllowed,
        N_storage.contractDetails,
        N_storage.displayNonce,
#ifdef HAVE_TRUSTED_NAME
        N_storage.verbose_trusted_name,
#endif  // HAVE_TRUSTED_NAME
#ifdef HAVE_EIP712_FULL_SUPPORT
        N_storage.verbose_eip712,
#endif  // HAVE_EIP712_FULL_SUPPORT
    };
    uint8_t offset = 0;

    for (unsigned int i = 0; i < ARRAY_SIZE(settings); ++i) {
        if ((sizeof(strings.common.fullAmount) - offset) >= BUF_INCREMENT) {
            strlcpy(strings.common.fullAmount + offset,
                    (settings[i] ? ENABLED_STR : DISABLED_STR),
                    BUF_INCREMENT);
        }
        offset += BUF_INCREMENT;
    }

    ux_flow_init(0, ux_settings_flow, start_step);
}

static void toggle_setting(volatile bool* setting, const ux_flow_step_t* ui_step) {
    bool value = !*setting;
    nvm_write((void*) setting, (void*) &value, sizeof(value));
    display_settings(ui_step);
}

static void switch_settings_blind_signing(void) {
    toggle_setting(&N_storage.dataAllowed, &ux_settings_flow_blind_signing_step);
}

static void switch_settings_display_data(void) {
    toggle_setting(&N_storage.contractDetails, &ux_settings_flow_display_data_step);
}

static void switch_settings_display_nonce(void) {
    toggle_setting(&N_storage.displayNonce, &ux_settings_flow_display_nonce_step);
}

#ifdef HAVE_TRUSTED_NAME
static void switch_settings_verbose_trusted_name(void) {
    toggle_setting(&N_storage.verbose_trusted_name, &ux_settings_flow_verbose_trusted_name_step);
}
#endif  // HAVE_TRUSTED_NAME

#ifdef HAVE_EIP712_FULL_SUPPORT
static void switch_settings_verbose_eip712(void) {
    toggle_setting(&N_storage.verbose_eip712, &ux_settings_flow_verbose_eip712_step);
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
