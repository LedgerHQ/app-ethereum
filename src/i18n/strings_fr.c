#include "i18n.h"

const char *const g_strings_fr[STR_COUNT] = {
    // Home & Settings
    [STR_APP_NAME] = "Ethereum",
    [STR_VERSION] = "Version",
    [STR_DEVELOPER] = "Développeur",
    [STR_COPYRIGHT] = "Ledger (c) 2025",

    // Settings switches
    [STR_BLIND_SIGNING] = "Signature aveugle",
    [STR_BLIND_SIGNING_DESC] = "Activer la signature aveugle des transactions",
    [STR_NONCE] = "Nonce",
    [STR_NONCE_DESC] = "Afficher le nonce dans les transactions",
    [STR_RAW_MESSAGES] = "Messages bruts",
    [STR_RAW_MESSAGES_DESC] = "Affiche le contenu brut des messages EIP712",
    [STR_SMART_ACCOUNTS] = "Comptes intelligents",
    [STR_SMART_ACCOUNTS_DESC] = "Activer les autorisations EIP-7702",
    [STR_SMART_ACCOUNTS_WALLET] = "Mise à niveau compte intelligent",
    [STR_SMART_ACCOUNTS_WALLET_DESC] =
        "Activer les autorisations EIP-7702 pour la délégation de contrats intelligents",
    [STR_DEBUG_CONTRACTS] = "Déboguer contrats",
    [STR_DEBUG_CONTRACTS_DESC] = "Afficher les détails\ndes contrats",
    [STR_DEBUG_CONTRACTS_WALLET] = "Déboguer contrats intelligents",
    [STR_DEBUG_CONTRACTS_WALLET_DESC] = "Afficher les détails des données contractuelles",
    [STR_TRANSACTION_CHECK] = "Vérification de transaction",
    [STR_TRANSACTION_CHECK_DESC] =
        "Recevez des alertes en temps réel sur les transactions risquées. En savoir plus : ledger.com/tx-check",
    [STR_TRANSACTION_HASH] = "Hash de transaction",
    [STR_TRANSACTION_HASH_DESC] = "Toujours afficher le hash de transaction",
    [STR_TRANSACTION_HASH_DESC_WALLET] = "Toujours afficher le hash de transaction ou message",
    [STR_LANGUAGE] = "Langue",
    [STR_LANGUAGE_DESC] = "Changer la langue de l'interface",

    // Common UI actions
    [STR_SIGN] = "Signer",
    [STR_REJECT] = "Rejeter",
    [STR_APPROVE] = "Approuver",
    [STR_REVIEW] = "Vérifier",
    [STR_VERIFY] = "Vérifier",
    [STR_CONFIRM] = "Confirmer",
    [STR_CANCEL] = "Annuler",
    [STR_BACK_TO_SAFETY] = "Retour en sécurité",
    [STR_GO_TO_SETTINGS] = "Aller aux paramètres",
    [STR_UNABLE_TO_SIGN] = "Impossible de signer",

    // Common labels
    [STR_MESSAGE] = "Message",
    [STR_TRANSACTION] = "Transaction",
    [STR_TYPED_MESSAGE] = "message typé",
    [STR_ACCOUNT] = "Compte",
    [STR_ADDRESS] = "Adresse",
    [STR_KEY] = "Clé",
    [STR_AMOUNT] = "Montant",
    [STR_FROM] = "De",
    [STR_TO] = "À",
    [STR_MAX_FEES] = "Frais max",
    [STR_NETWORK] = "Réseau",
    [STR_CONTRACT] = "Contrat",
    [STR_CONTRACT_ADDRESS] = "Adresse du contrat",
    [STR_SMART_CONTRACT] = "Contrat intelligent",
    [STR_SMART_CONTRACT_INFO] = "Informations contrat intelligent",
    [STR_SMART_CONTRACT_OWNER] = "Propriétaire du contrat intelligent",
    [STR_CONTRACT_OWNER] = "Propriétaire du contrat",
    [STR_THRESHOLD] = "Seuil",
    [STR_YOUR_ROLE] = "Votre rôle",
    [STR_PARAMETER] = "Paramètre",
    [STR_SELECTOR] = "Sélecteur",
    [STR_TX_HASH] = "Hash tx",

    // Transaction review
    [STR_REVIEW_MESSAGE] = "Vérifier le message",
    [STR_REVIEW_TYPED_MESSAGE] = "Vérifier le message typé",
    [STR_REVIEW_TRANSACTION] = "Vérifier la transaction",
    [STR_REVIEW_TRANSACTION_TO] = "Vérifier la transaction vers %s",
    [STR_SIGN_MESSAGE] = "Signer le message",
    [STR_SIGN_TYPED_MESSAGE] = "Signer le message typé",
    [STR_SIGN_TRANSACTION] = "Signer la transaction",
    [STR_SIGN_TRANSACTION_TO] = "Signer la transaction vers %s ?",
    [STR_SIGN_OPERATION] = "Signer l'opération",
    [STR_REJECT_TRANSACTION] = "Rejeter la transaction",
    [STR_REJECT_AUTHORIZATION] = "Rejeter l'autorisation",
    [STR_INTERACTION_WITH] = "Interaction avec",
    [STR_DEPLOYED_ON] = "Déployé sur",
    [STR_A_SMART_CONTRACT] = "un contrat intelligent",

    // EIP-7702 Authorization
    [STR_DELEGATE_TO] = "Déléguer à",
    [STR_DELEGATE_ON_NETWORK] = "Déléguer sur le réseau",
    [STR_ON_NETWORK] = "Sur le réseau",
    [STR_REVOKE_ON_NETWORK] = "Révoquer sur le réseau",
    [STR_REVIEW_AUTH_UPGRADE] =
        "Vérifier l'autorisation de mise à niveau en compte de contrat intelligent ?",
    [STR_SIGN_AUTH_UPGRADE] =
        "Signer l'autorisation de mise à niveau en compte de contrat intelligent ?",
    [STR_REVIEW_AUTH_REVOKE] = "Vérifier l'autorisation de révocation de délégation de contrat intelligent ?",
    [STR_SIGN_AUTH_REVOKE] = "Signer l'autorisation de révocation de délégation de contrat intelligent ?",

    // Status messages
    [STR_MESSAGE_SIGNED] = "Message signé",
    [STR_MESSAGE_REJECTED] = "Message rejeté",
    [STR_TRANSACTION_SIGNED] = "Transaction signée",
    [STR_TRANSACTION_REJECTED] = "Transaction rejetée",

    // Warnings & Errors
    [STR_WARNING] = "Avertissement",
    [STR_ERROR] = "Erreur",
    [STR_BLIND_SIGNING_WARNING] = "Cette transaction ne peut pas être signée clairement",
    [STR_BLIND_SIGNING_ERROR] = "Activez la signature aveugle dans les paramètres pour signer cette transaction.",
    [STR_BLIND_SIGNING_MUST_ENABLE] = "La signature aveugle\ndoit être activée\ndans les paramètres",
    [STR_AUTH_CANNOT_BE_SIGNED] = "Cette autorisation ne peut pas être signée",
    [STR_AUTH_NOT_IN_WHITELIST] =
        "Cette autorisation implique une délégation à un contrat intelligent qui n'est pas dans la liste blanche.",
    [STR_ENABLE_SMART_ACCOUNT_SETTING] =
        "Activez la mise à niveau de compte intelligent dans les paramètres pour signer cette autorisation.",

    // Transaction Check
    [STR_ENABLE_TX_CHECK] = "Activer la\nVérification de transaction ?",
    [STR_TX_CHECK_DESC_LONG] =
        "Recevez des alertes en temps réel sur les transactions Ethereum risquées. Fourni par des prestataires de services.",
    [STR_TX_CHECK_TCS] = "En activant, vous acceptez les CGU : ledger.com/tx-check",
    [STR_TX_CHECK_ENABLED] = "Vérification de transaction activée",
    [STR_YES_ENABLE] = "Oui, activer",
    [STR_MAYBE_LATER] = "Peut-être plus tard",
    [STR_HOW_IT_WORKS] = "Comment ça marche",
    [STR_TX_CHECK_INFO_1] = "La transaction est vérifiée avant la signature.",
    [STR_TX_CHECK_INFO_2] = "Le résultat est affiché : Menace critique, risque potentiel ou aucune menace.",
    [STR_TX_CHECK_INFO_3] =
        "Scannez le QR Code sur votre Ledger pour les détails sur toute menace ou risque.",
    [STR_ACCEPT_RISK_AND_SIGN] = "Accepter le risque et signer",
    [STR_ACCEPT_THREAT_AND_SIGN] = "Accepter la menace et signer",

    // Safe Account
    [STR_SIGNER_N] = "Signataire %d",
    [STR_SAFE_ADDRESS_VALIDATED] = "Adresse Safe validée",
    [STR_SAFE_ADDRESS_REJECTED] = "Adresse Safe rejetée",
    [STR_VERIFY_SAFE_ADDRESS] = "Vérifier l'adresse Safe",

    // Privacy keys
    [STR_PROVIDE_PRIVACY_KEY] = "Fournir la clé\npublique de confidentialité",
    [STR_PROVIDE_SECRET_KEY] = "Fournir la clé\npublique secrète",

    // ETH2
    [STR_VERIFY_ETH2_ADDRESS] = "Vérifier l'adresse\nETH2",

    // Etherscan
    [STR_SCAN_ETHERSCAN] = "Scanner pour voir sur Etherscan",

    // Confirm selector/parameter
    [STR_VERIFY_FMT] = "Vérifier %s",
    [STR_CONFIRM_FMT] = "Confirmer %s",

    // Plugin format string
    [STR_PLUGIN_TAGLINE_FMT] =
        "Cette app permet la signature\nclaire de transactions pour\nla dApp %s.",
};
