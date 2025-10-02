#ifndef I18N_H
#define I18N_H

#include <stdint.h>
#include <stdbool.h>

// Supported languages
typedef enum {
    LANG_ENGLISH = 0,
    LANG_FRENCH = 1,
    LANG_COUNT
} language_e;

// String IDs for all translatable strings
typedef enum {
    // Home & Settings
    STR_APP_NAME = 0,
    STR_VERSION,
    STR_DEVELOPER,
    STR_COPYRIGHT,

    // Settings switches
    STR_BLIND_SIGNING,
    STR_BLIND_SIGNING_DESC,
    STR_NONCE,
    STR_NONCE_DESC,
    STR_RAW_MESSAGES,
    STR_RAW_MESSAGES_DESC,
    STR_SMART_ACCOUNTS,
    STR_SMART_ACCOUNTS_DESC,
    STR_SMART_ACCOUNTS_WALLET,
    STR_SMART_ACCOUNTS_WALLET_DESC,
    STR_DEBUG_CONTRACTS,
    STR_DEBUG_CONTRACTS_DESC,
    STR_DEBUG_CONTRACTS_WALLET,
    STR_DEBUG_CONTRACTS_WALLET_DESC,
    STR_TRANSACTION_CHECK,
    STR_TRANSACTION_CHECK_DESC,
    STR_TRANSACTION_HASH,
    STR_TRANSACTION_HASH_DESC,
    STR_TRANSACTION_HASH_DESC_WALLET,
    STR_LANGUAGE,
    STR_LANGUAGE_DESC,

    // Common UI actions
    STR_SIGN,
    STR_REJECT,
    STR_APPROVE,
    STR_REVIEW,
    STR_VERIFY,
    STR_CONFIRM,
    STR_CANCEL,
    STR_BACK_TO_SAFETY,
    STR_GO_TO_SETTINGS,
    STR_UNABLE_TO_SIGN,

    // Common labels
    STR_MESSAGE,
    STR_TRANSACTION,
    STR_TYPED_MESSAGE,
    STR_ACCOUNT,
    STR_ADDRESS,
    STR_KEY,
    STR_AMOUNT,
    STR_FROM,
    STR_TO,
    STR_MAX_FEES,
    STR_NETWORK,
    STR_CONTRACT,
    STR_CONTRACT_ADDRESS,
    STR_SMART_CONTRACT,
    STR_SMART_CONTRACT_INFO,
    STR_SMART_CONTRACT_OWNER,
    STR_CONTRACT_OWNER,
    STR_THRESHOLD,
    STR_YOUR_ROLE,
    STR_PARAMETER,
    STR_SELECTOR,
    STR_TX_HASH,

    // Transaction review
    STR_REVIEW_MESSAGE,
    STR_REVIEW_TYPED_MESSAGE,
    STR_REVIEW_TRANSACTION,
    STR_REVIEW_TRANSACTION_TO,
    STR_SIGN_MESSAGE,
    STR_SIGN_TYPED_MESSAGE,
    STR_SIGN_TRANSACTION,
    STR_SIGN_TRANSACTION_TO,
    STR_SIGN_OPERATION,
    STR_REJECT_TRANSACTION,
    STR_REJECT_AUTHORIZATION,
    STR_INTERACTION_WITH,
    STR_DEPLOYED_ON,
    STR_A_SMART_CONTRACT,

    // EIP-7702 Authorization
    STR_DELEGATE_TO,
    STR_DELEGATE_ON_NETWORK,
    STR_ON_NETWORK,
    STR_REVOKE_ON_NETWORK,
    STR_REVIEW_AUTH_UPGRADE,
    STR_SIGN_AUTH_UPGRADE,
    STR_REVIEW_AUTH_REVOKE,
    STR_SIGN_AUTH_REVOKE,

    // Status messages
    STR_MESSAGE_SIGNED,
    STR_MESSAGE_REJECTED,
    STR_TRANSACTION_SIGNED,
    STR_TRANSACTION_REJECTED,

    // Warnings & Errors
    STR_WARNING,
    STR_ERROR,
    STR_BLIND_SIGNING_WARNING,
    STR_BLIND_SIGNING_ERROR,
    STR_BLIND_SIGNING_MUST_ENABLE,
    STR_AUTH_CANNOT_BE_SIGNED,
    STR_AUTH_NOT_IN_WHITELIST,
    STR_ENABLE_SMART_ACCOUNT_SETTING,

    // Transaction Check
    STR_ENABLE_TX_CHECK,
    STR_TX_CHECK_DESC_LONG,
    STR_TX_CHECK_TCS,
    STR_TX_CHECK_ENABLED,
    STR_YES_ENABLE,
    STR_MAYBE_LATER,
    STR_HOW_IT_WORKS,
    STR_TX_CHECK_INFO_1,
    STR_TX_CHECK_INFO_2,
    STR_TX_CHECK_INFO_3,
    STR_ACCEPT_RISK_AND_SIGN,
    STR_ACCEPT_THREAT_AND_SIGN,

    // Safe Account
    STR_SIGNER_N,
    STR_SAFE_ADDRESS_VALIDATED,
    STR_SAFE_ADDRESS_REJECTED,
    STR_VERIFY_SAFE_ADDRESS,

    // Privacy keys
    STR_PROVIDE_PRIVACY_KEY,
    STR_PROVIDE_SECRET_KEY,

    // ETH2
    STR_VERIFY_ETH2_ADDRESS,

    // Etherscan
    STR_SCAN_ETHERSCAN,

    // Confirm selector/parameter
    STR_VERIFY_FMT,
    STR_CONFIRM_FMT,

    // Plugin format string
    STR_PLUGIN_TAGLINE_FMT,

    // Must be last
    STR_COUNT
} string_id_e;

/**
 * Get the current language setting
 *
 * @return Current language enum value
 */
language_e i18n_get_language(void);

/**
 * Set the current language
 *
 * @param[in] lang Language to set
 */
void i18n_set_language(language_e lang);

/**
 * Get a translated string by ID
 *
 * @param[in] id String ID
 * @return Pointer to translated string (never NULL)
 */
const char *i18n_get_string(string_id_e id);

/**
 * Convenience macro for getting strings
 */
#define STR(id) i18n_get_string(STR_##id)

#endif // I18N_H
