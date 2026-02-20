/**
 * @file lang_fr.c
 * @brief French (FR) language string table for binary size comparison testing.
 *
 * Compiled only when HAVE_LANG_FRENCH is defined:
 *   make TARGET=flex ADD_FRENCH_LANG=1
 *
 * The table is anchored (kept alive by the linker's --gc-sections) via a
 * volatile reference inserted in reset_app_context() (src/main.c).
 *
 * What this measures
 * ------------------
 * The delta in ROM between the baseline (English strings already hardcoded in
 * the UI source files) and a build that ALSO embeds a French translation table.
 * English originals come from src/nbgl/ *.c -- 78 genuine display strings only.
 * Format strings (containing % specifiers used in snprintf) are intentionally
 * excluded, as are C string-literal fragments that are only parts of larger
 * concatenated strings.
 */

#ifdef HAVE_LANG_FRENCH

#include "lang_fr.h"

/* ---------------------------------------------------------------------------
 * French translations (78 strings, parallel to the English originals).
 * Sorted alphabetically by English original.
 * --------------------------------------------------------------------------- */
__attribute__((used)) const char *const LANG_FR_STRINGS[LANG_FR_STRING_COUNT] = {
    /* 00  "Accept risk and sign"                                             */
    "Accepter le risque et signer",
    /* 01  "Accept threat and sign"                                           */
    "Accepter la menace et signer",
    /* 02  "Account"                                                          */
    "Compte",
    /* 03  "Address"                                                          */
    "Adresse",
    /* 04  "Always display the transaction hash"                              */
    "Toujours afficher le hachage de transaction",
    /* 05  "Always display the transaction or message hash"                   */
    "Toujours afficher le hachage de transaction ou de message",
    /* 06  "Amount"                                                           */
    "Montant",
    /* 07  "Back to safety"                                                   */
    "Retour \u00e0 la s\u00e9curit\u00e9",
    /* 08  "Blind signing must\nbe enabled in\nsettings"                      */
    "La signature aveugle doit\n\u00eatre activ\u00e9e dans\nles param\u00e8tres",
    /* 09  "Blind signing"                                                    */
    "Signature aveugle",
    /* 10  "By enabling, you accept T&Cs: ledger.com/tx-check"               */
    "En activant, vous acceptez les CGU\u00a0: ledger.com/tx-check",
    /* 11  "Contract address"                                                 */
    "Adresse du contrat",
    /* 12  "Contract owner"                                                   */
    "Propri\u00e9taire du contrat",
    /* 13  "Contract"                                                         */
    "Contrat",
    /* 14  "Debug contracts"                                                  */
    "D\u00e9boguer les contrats",
    /* 15  "Debug smart contracts"                                            */
    "D\u00e9boguer les contrats intelligents",
    /* 16  "Delegate on network"                                              */
    "D\u00e9l\u00e9guer sur le r\u00e9seau",
    /* 17  "Delegate to"                                                      */
    "D\u00e9l\u00e9guer \u00e0",
    /* 18  "Deployed on"                                                      */
    "D\u00e9ploy\u00e9 sur",
    /* 19  "Display contract data details"                                    */
    "Afficher les d\u00e9tails des donn\u00e9es du contrat",
    /* 20  "Display contract\ndata details"                                   */
    "Afficher les\nd\u00e9tails du contrat",
    /* 21  "Display nonce in transactions"                                    */
    "Afficher le nonce dans les transactions",
    /* 22  "Displays raw content of EIP712 messages"                         */
    "Affiche le contenu brut des messages EIP712",
    /* 23  "Enable blind signing in the settings to sign this transaction."   */
    "Activez la signature aveugle dans les param\u00e8tres pour signer cette transaction.",
    /* 24  "Enable EIP-7702 authorizations for smart contract delegation"     */
    "Activer les autorisations EIP-7702 pour la d\u00e9l\u00e9gation de contrats intelligents",
    /* 25  "Enable EIP-7702 authorizations"                                  */
    "Activer les autorisations EIP-7702",
    /* 26  "Enable smart account upgrade in the settings to sign this auth."  */
    "Activez la mise \u00e0 niveau des comptes intelligents dans les param\u00e8tres pour signer cette autorisation.",
    /* 27  "Enable transaction blind signing"                                 */
    "Activer la signature aveugle",
    /* 28  "Enable\nTransaction Check?"                                       */
    "Activer la\nv\u00e9rification des transactions\u00a0?",
    /* 29  "Get real-time warnings about risky Ethereum transactions.         *
     *      Powered by service providers."  (C-concatenated into .description)*/
    "Recevez des alertes en temps r\u00e9el sur les transactions Ethereum \u00e0 risque. "
    "Fourni par des prestataires de services.",
    /* 30  "Get real-time warnings about risky transactions. ..."             *
     *      (second opt-in variant, see ui_tx_simulation.c)                  */
    "Recevez des alertes en temps r\u00e9el sur les transactions \u00e0 risque. "
    "En savoir plus\u00a0: ledger.com/tx-check",
    /* 31  "From"                                                             */
    "De",
    /* 32  "Go to settings"                                                   */
    "Aller aux param\u00e8tres",
    /* 33  "How it works"                                                     */
    "Comment \u00e7a fonctionne",
    /* 34  "Interaction with"                                                 */
    "Interaction avec",
    /* 35  "Key"                                                              */
    "Cl\u00e9",
    /* 36  "Max fees"                                                         */
    "Frais maximum",
    /* 37  "Maybe later"                                                      */
    "Plus tard",
    /* 38  "Message"                                                          */
    "Message",
    /* 39  "Network"                                                          */
    "R\u00e9seau",
    /* 40  "Nonce"                                                            */
    "Nonce",
    /* 41  "On network"                                                       */
    "Sur le r\u00e9seau",
    /* 42  "Parameter"                                                        */
    "Param\u00e8tre",
    /* 43  "Provide public\nprivacy key"                                      */
    "Fournir la cl\u00e9\npublique de confidentialit\u00e9",
    /* 44  "Provide public\nsecret key"                                       */
    "Fournir la cl\u00e9\npublique secr\u00e8te",
    /* 45  "Raw messages"                                                     */
    "Messages bruts",
    /* 46  "Reject authorization"                                             */
    "Rejeter l\u2019autorisation",
    /* 47  "Reject transaction"                                               */
    "Rejeter la transaction",
    /* 48  "Review authorization to revoke smart contract delegation?"        */
    "V\u00e9rifier l\u2019autorisation pour r\u00e9voquer la d\u00e9l\u00e9gation de contrat intelligent\u00a0?",
    /* 49  "Review authorization to upgrade into smart contract account?"     */
    "V\u00e9rifier l\u2019autorisation de mise \u00e0 niveau en compte intelligent\u00a0?",
    /* 50  "Review message"                                                   */
    "V\u00e9rifier le message",
    /* 51  "Review transaction"                                               */
    "V\u00e9rifier la transaction",
    /* 52  "Review typed message"                                             */
    "V\u00e9rifier le message typ\u00e9",
    /* 53  "Revoke on network"                                                */
    "R\u00e9voquer sur le r\u00e9seau",
    /* 54  "Safe Address rejected"                                            */
    "Adresse Safe rejet\u00e9e",
    /* 55  "Safe Address validated"                                           */
    "Adresse Safe valid\u00e9e",
    /* 56  "Scan the QR Code on your Ledger device for details on any threat or risk." */
    "Scannez le QR Code sur votre appareil Ledger pour obtenir des d\u00e9tails sur toute menace ou risque.",
    /* 57  "Scan to view on Etherscan"                                        */
    "Scanner pour voir sur Etherscan",
    /* 58  "Sign authorization to revoke smart contract delegation?"          */
    "Signer l\u2019autorisation pour r\u00e9voquer la d\u00e9l\u00e9gation de contrat intelligent\u00a0?",
    /* 59  "Sign authorization to upgrade into smart contract account?"       */
    "Signer l\u2019autorisation de mise \u00e0 niveau en compte intelligent\u00a0?",
    /* 60  "Sign operation"                                                   */
    "Signer l\u2019op\u00e9ration",
    /* 61  "Smart account upgrade"                                            */
    "Mise \u00e0 niveau de compte intelligent",
    /* 62  "Smart accounts"                                                   */
    "Comptes intelligents",
    /* 63  "Smart contract information"                                       */
    "Informations sur le contrat intelligent",
    /* 64  "Smart contract owner"                                             */
    "Propri\u00e9taire du contrat intelligent",
    /* 65  "Smart contract"                                                   */
    "Contrat intelligent",
    /* 66  "The result is displayed: Critical threat, potential risk or no threat." */
    "Le r\u00e9sultat affich\u00e9 : menace critique, risque potentiel ou aucune menace.",
    /* 67  "This authorization cannot be signed"                              */
    "Cette autorisation ne peut pas \u00eatre sign\u00e9e",
    /* 68  "This authorization involves a delegation ..."                     */
    "Cette autorisation implique une d\u00e9l\u00e9gation vers un contrat intelligent "
    "qui n\u2019est pas dans la liste blanche.",
    /* 69  "This transaction cannot be clear-signed"                          */
    "Cette transaction ne peut pas \u00eatre sign\u00e9e en clair",
    /* 70  "Threshold"                                                        */
    "Seuil",
    /* 71  "Transaction Check enabled"                                        */
    "V\u00e9rification des transactions activ\u00e9e",
    /* 72  "Transaction Check"                                                */
    "V\u00e9rification des transactions",
    /* 73  "Transaction hash"                                                 */
    "Hachage de transaction",
    /* 74  "Transaction is checked for threats before signing."               */
    "La transaction est v\u00e9rifi\u00e9e pour les menaces avant la signature.",
    /* 75  "Tx hash"                                                          */
    "Hachage Tx",
    /* 76  "Unable to sign"                                                   */
    "Impossible de signer",
    /* 77  "Verify Safe address"                                              */
    "V\u00e9rifier l\u2019adresse Safe",
};

#endif  // HAVE_LANG_FRENCH
