#include "shared_context.h"
#include "ui_callbacks.h"
#include "common_ui.h"
#include "common_utils.h"
#include "feature_signTx.h"

#define ENABLED_STR   "Enabled"
#define DISABLED_STR  "Disabled"
#define BUF_INCREMENT (MAX(strlen(ENABLED_STR), strlen(DISABLED_STR)) + 1)

// Reuse the strings.common.fullAmount buffer for settings displaying.
// No risk of collision as this buffer is unused in the settings menu
#define SETTING_VERBOSE_DOMAIN_NAME_STATE (strings.common.fullAmount + (BUF_INCREMENT * 0))
#define SETTING_VERBOSE_EIP712_STATE      (strings.common.fullAmount + (BUF_INCREMENT * 1))
#define SETTING_DISPLAY_NONCE_STATE       (strings.common.fullAmount + (BUF_INCREMENT * 2))
#define SETTING_DISPLAY_DATA_STATE        (strings.common.fullAmount + (BUF_INCREMENT * 3))

#define BOOL_TO_STATE_STR(b) (b ? ENABLED_STR : DISABLED_STR)

static void display_settings(const ux_flow_step_t* const start_step);
static void switch_settings_display_data(void);
static void switch_settings_display_nonce(void);
#ifdef HAVE_EIP712_FULL_SUPPORT
static void switch_settings_verbose_eip712(void);
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_DOMAIN_NAME
static void switch_settings_verbose_domain_name(void);
#endif  // HAVE_DOMAIN_NAME

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
    app_exit(),
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
#ifdef HAVE_DOMAIN_NAME
UX_STEP_CB(
    ux_settings_flow_verbose_domain_name_step,
    bnnn,
    switch_settings_verbose_domain_name(),
    {
      "ENS addresses",
      "Displays resolved",
      "addresses from ENS",
      SETTING_VERBOSE_DOMAIN_NAME_STATE
    });
#endif // HAVE_DOMAIN_NAME

#ifdef HAVE_EIP712_FULL_SUPPORT
UX_STEP_CB(
    ux_settings_flow_verbose_eip712_step,
    bnnn,
    switch_settings_verbose_eip712(),
    {
      "Raw messages",
      "Displays raw content",
      "from EIP712 messages",
      SETTING_VERBOSE_EIP712_STATE
    });
#endif // HAVE_EIP712_FULL_SUPPORT

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
      "Displays nonce",
      "in transactions",
#endif
      SETTING_DISPLAY_NONCE_STATE
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
      "Debug contracts",
      "Displays contract",
      "data details",
#endif
      SETTING_DISPLAY_DATA_STATE
    });

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
#ifdef HAVE_DOMAIN_NAME
        &ux_settings_flow_verbose_domain_name_step,
#endif  // HAVE_DOMAIN_NAME
#ifdef HAVE_EIP712_FULL_SUPPORT
        &ux_settings_flow_verbose_eip712_step,
#endif  // HAVE_EIP712_FULL_SUPPORT
        &ux_settings_flow_display_nonce_step,
        &ux_settings_flow_display_data_step,
        &ux_settings_flow_back_step);

static void display_settings(const ux_flow_step_t* const start_step) {
    strlcpy(SETTING_DISPLAY_DATA_STATE,
            BOOL_TO_STATE_STR(N_storage.contractDetails),
            BUF_INCREMENT);
    strlcpy(SETTING_DISPLAY_NONCE_STATE, BOOL_TO_STATE_STR(N_storage.displayNonce), BUF_INCREMENT);
#ifdef HAVE_EIP712_FULL_SUPPORT
    strlcpy(SETTING_VERBOSE_EIP712_STATE,
            BOOL_TO_STATE_STR(N_storage.verbose_eip712),
            BUF_INCREMENT);
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_DOMAIN_NAME
    strlcpy(SETTING_VERBOSE_DOMAIN_NAME_STATE,
            BOOL_TO_STATE_STR(N_storage.verbose_domain_name),
            BUF_INCREMENT);
#endif  // HAVE_DOMAIN_NAME

    ux_flow_init(0, ux_settings_flow, start_step);
}

static void toggle_setting(volatile bool* setting, const ux_flow_step_t* ui_step) {
    bool value = !*setting;
    nvm_write((void*) setting, (void*) &value, sizeof(value));
    display_settings(ui_step);
}

static void switch_settings_display_data(void) {
    toggle_setting(&N_storage.contractDetails, &ux_settings_flow_display_data_step);
}

static void switch_settings_display_nonce(void) {
    toggle_setting(&N_storage.displayNonce, &ux_settings_flow_display_nonce_step);
}

#ifdef HAVE_EIP712_FULL_SUPPORT
static void switch_settings_verbose_eip712(void) {
    toggle_setting(&N_storage.verbose_eip712, &ux_settings_flow_verbose_eip712_step);
}
#endif  // HAVE_EIP712_FULL_SUPPORT

#ifdef HAVE_DOMAIN_NAME
static void switch_settings_verbose_domain_name(void) {
    toggle_setting(&N_storage.verbose_domain_name, &ux_settings_flow_verbose_domain_name_step);
}
#endif  // HAVE_DOMAIN_NAME

//////////////////////////////////////////////////////////////////////
// clang-format off
UX_STEP_NOCB(
    ux_blind_signing_warning_step,
    pbb,
    {
      &C_icon_warning,
#ifdef TARGET_NANOS
      "Transaction",
      "not trusted",
#else
      "This transaction",
      "cannot be trusted",
#endif
    });
#ifndef TARGET_NANOS
UX_STEP_NOCB(
    ux_blind_signing_text1_step,
    nnnn,
    {
      "Your Ledger cannot",
      "decode this",
      "transaction. If you",
      "sign it, you could",
    });
UX_STEP_NOCB(
    ux_blind_signing_text2_step,
    nnnn,
    {
      "be authorizing",
      "malicious actions",
      "that can drain your",
      "wallet.",
    });
#endif
UX_STEP_NOCB(
    ux_blind_signing_link_step,
    nn,
    {
      "Learn more:",
      "ledger.com/e8",
    });
UX_STEP_CB(
    ux_blind_signing_accept_step,
    pbb,
    start_signature_flow(),
    {
      &C_icon_validate_14,
#ifdef TARGET_NANOS
      "Accept risk",
      "and review",
#else
      "Accept risk and",
      "review transaction",
#endif
    });
UX_STEP_CB(
    ux_blind_signing_reject_step,
    pb,
    report_finalize_error(),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_blind_signing_flow,
        &ux_blind_signing_warning_step,
#ifndef TARGET_NANOS
        &ux_blind_signing_text1_step,
        &ux_blind_signing_text2_step,
#endif
        &ux_blind_signing_link_step,
        &ux_blind_signing_accept_step,
        &ux_blind_signing_reject_step);
