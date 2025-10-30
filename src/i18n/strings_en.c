#include "i18n.h"

const char *const g_strings_en[STR_COUNT] = {
    // Home & Settings
    [STR_APP_NAME] = "Ethereum",
    [STR_VERSION] = "Version",
    [STR_DEVELOPER] = "Developer",
    [STR_COPYRIGHT] = "Ledger (c) 2025",

    // Settings switches
    [STR_BLIND_SIGNING] = "Blind signing",
    [STR_BLIND_SIGNING_DESC] = "Enable transaction blind signing",
    [STR_NONCE] = "Nonce",
    [STR_NONCE_DESC] = "Display nonce in transactions",
    [STR_RAW_MESSAGES] = "Raw messages",
    [STR_RAW_MESSAGES_DESC] = "Displays raw content of EIP712 messages",
    [STR_SMART_ACCOUNTS] = "Smart accounts",
    [STR_SMART_ACCOUNTS_DESC] = "Enable EIP-7702 authorizations",
    [STR_SMART_ACCOUNTS_WALLET] = "Smart account upgrade",
    [STR_SMART_ACCOUNTS_WALLET_DESC] = "Enable EIP-7702 authorizations for smart contract delegation",
    [STR_DEBUG_CONTRACTS] = "Debug contracts",
    [STR_DEBUG_CONTRACTS_DESC] = "Display contract\ndata details",
    [STR_DEBUG_CONTRACTS_WALLET] = "Debug smart contracts",
    [STR_DEBUG_CONTRACTS_WALLET_DESC] = "Display contract data details",
    [STR_TRANSACTION_CHECK] = "Transaction Check",
    [STR_TRANSACTION_CHECK_DESC] =
        "Get real-time warnings about risky transactions. Learn more: ledger.com/tx-check",
    [STR_TRANSACTION_HASH] = "Transaction hash",
    [STR_TRANSACTION_HASH_DESC] = "Always display the transaction hash",
    [STR_TRANSACTION_HASH_DESC_WALLET] = "Always display the transaction or message hash",
    [STR_LANGUAGE] = "Language",
    [STR_LANGUAGE_DESC] = "Change interface language",

    // Common UI actions
    [STR_SIGN] = "Sign",
    [STR_REJECT] = "Reject",
    [STR_APPROVE] = "Approve",
    [STR_REVIEW] = "Review",
    [STR_VERIFY] = "Verify",
    [STR_CONFIRM] = "Confirm",
    [STR_CANCEL] = "Cancel",
    [STR_BACK_TO_SAFETY] = "Back to safety",
    [STR_GO_TO_SETTINGS] = "Go to settings",
    [STR_UNABLE_TO_SIGN] = "Unable to sign",

    // Common labels
    [STR_MESSAGE] = "Message",
    [STR_TRANSACTION] = "Transaction",
    [STR_TYPED_MESSAGE] = "typed message",
    [STR_ACCOUNT] = "Account",
    [STR_ADDRESS] = "Address",
    [STR_KEY] = "Key",
    [STR_AMOUNT] = "Amount",
    [STR_FROM] = "From",
    [STR_TO] = "To",
    [STR_MAX_FEES] = "Max fees",
    [STR_NETWORK] = "Network",
    [STR_CONTRACT] = "Contract",
    [STR_CONTRACT_ADDRESS] = "Contract address",
    [STR_SMART_CONTRACT] = "Smart contract",
    [STR_SMART_CONTRACT_INFO] = "Smart contract information",
    [STR_SMART_CONTRACT_OWNER] = "Smart contract owner",
    [STR_CONTRACT_OWNER] = "Contract owner",
    [STR_THRESHOLD] = "Threshold",
    [STR_YOUR_ROLE] = "Your role",
    [STR_PARAMETER] = "Parameter",
    [STR_SELECTOR] = "Selector",
    [STR_TX_HASH] = "Tx hash",

    // Transaction review
    [STR_REVIEW_MESSAGE] = "Review message",
    [STR_REVIEW_TYPED_MESSAGE] = "Review typed message",
    [STR_REVIEW_TRANSACTION] = "Review transaction",
    [STR_REVIEW_TRANSACTION_TO] = "Review transaction to %s",
    [STR_SIGN_MESSAGE] = "Sign message",
    [STR_SIGN_TYPED_MESSAGE] = "Sign typed message",
    [STR_SIGN_TRANSACTION] = "Sign transaction",
    [STR_SIGN_TRANSACTION_TO] = "Sign transaction to %s?",
    [STR_SIGN_OPERATION] = "Sign operation",
    [STR_REJECT_TRANSACTION] = "Reject transaction",
    [STR_REJECT_AUTHORIZATION] = "Reject authorization",
    [STR_INTERACTION_WITH] = "Interaction with",
    [STR_DEPLOYED_ON] = "Deployed on",
    [STR_A_SMART_CONTRACT] = "a smart contract",

    // EIP-7702 Authorization
    [STR_DELEGATE_TO] = "Delegate to",
    [STR_DELEGATE_ON_NETWORK] = "Delegate on network",
    [STR_ON_NETWORK] = "On network",
    [STR_REVOKE_ON_NETWORK] = "Revoke on network",
    [STR_REVIEW_AUTH_UPGRADE] = "Review authorization to upgrade into smart contract account?",
    [STR_SIGN_AUTH_UPGRADE] = "Sign authorization to upgrade into smart contract account?",
    [STR_REVIEW_AUTH_REVOKE] = "Review authorization to revoke smart contract delegation?",
    [STR_SIGN_AUTH_REVOKE] = "Sign authorization to revoke smart contract delegation?",

    // Status messages
    [STR_MESSAGE_SIGNED] = "Message signed",
    [STR_MESSAGE_REJECTED] = "Message rejected",
    [STR_TRANSACTION_SIGNED] = "Transaction signed",
    [STR_TRANSACTION_REJECTED] = "Transaction rejected",

    // Warnings & Errors
    [STR_WARNING] = "Warning",
    [STR_ERROR] = "Error",
    [STR_BLIND_SIGNING_WARNING] = "This transaction cannot be clear-signed",
    [STR_BLIND_SIGNING_ERROR] = "Enable blind signing in the settings to sign this transaction.",
    [STR_BLIND_SIGNING_MUST_ENABLE] = "Blind signing must\nbe enabled in\nsettings",
    [STR_AUTH_CANNOT_BE_SIGNED] = "This authorization cannot be signed",
    [STR_AUTH_NOT_IN_WHITELIST] =
        "This authorization involves a delegation to a smart contract which is not in the whitelist.",
    [STR_ENABLE_SMART_ACCOUNT_SETTING] =
        "Enable smart account upgrade in the settings to sign this authorization.",

    // Transaction Check
    [STR_ENABLE_TX_CHECK] = "Enable\nTransaction Check?",
    [STR_TX_CHECK_DESC_LONG] =
        "Get real-time warnings about risky Ethereum transactions. Powered by service providers.",
    [STR_TX_CHECK_TCS] = "By enabling, you accept T&Cs: ledger.com/tx-check",
    [STR_TX_CHECK_ENABLED] = "Transaction Check enabled",
    [STR_YES_ENABLE] = "Yes, enable",
    [STR_MAYBE_LATER] = "Maybe later",
    [STR_HOW_IT_WORKS] = "How it works",
    [STR_TX_CHECK_INFO_1] = "Transaction is checked for threats before signing.",
    [STR_TX_CHECK_INFO_2] = "The result is displayed: Critical threat, potential risk or no threat.",
    [STR_TX_CHECK_INFO_3] =
        "Scan the QR Code on your Ledger device for details on any threat or risk.",
    [STR_ACCEPT_RISK_AND_SIGN] = "Accept risk and sign",
    [STR_ACCEPT_THREAT_AND_SIGN] = "Accept threat and sign",

    // Safe Account
    [STR_SIGNER_N] = "Signer %d",
    [STR_SAFE_ADDRESS_VALIDATED] = "Safe Address validated",
    [STR_SAFE_ADDRESS_REJECTED] = "Safe Address rejected",
    [STR_VERIFY_SAFE_ADDRESS] = "Verify Safe address",

    // Privacy keys
    [STR_PROVIDE_PRIVACY_KEY] = "Provide public\nprivacy key",
    [STR_PROVIDE_SECRET_KEY] = "Provide public\nsecret key",

    // ETH2
    [STR_VERIFY_ETH2_ADDRESS] = "Verify ETH2\naddress",

    // Etherscan
    [STR_SCAN_ETHERSCAN] = "Scan to view on Etherscan",

    // Confirm selector/parameter
    [STR_VERIFY_FMT] = "Verify %s",
    [STR_CONFIRM_FMT] = "Confirm %s",

    // Plugin format string
    [STR_PLUGIN_TAGLINE_FMT] = "This app enables clear\nsigning transactions for\nthe %s dApp.",
};
