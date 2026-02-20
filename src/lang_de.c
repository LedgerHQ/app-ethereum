/**
 * @file lang_de.c
 * @brief German (DE) language string table for binary size comparison testing.
 *
 * Compiled only when HAVE_LANG_GERMAN is defined:
 *   make TARGET=flex ADD_GERMAN_LANG=1
 *
 * 78 genuine UI display strings — mirrors lang_fr.c exactly (same 78 originals).
 * Format strings (snprintf % specifiers) are excluded.
 */

#ifdef HAVE_LANG_GERMAN

#include "lang_de.h"

__attribute__((used)) const char *const LANG_DE_STRINGS[LANG_DE_STRING_COUNT] = {
    /* 00  "Accept risk and sign"                                             */
    "Risiko akzeptieren und signieren",
    /* 01  "Accept threat and sign"                                           */
    "Bedrohung akzeptieren und signieren",
    /* 02  "Account"                                                          */
    "Konto",
    /* 03  "Address"                                                          */
    "Adresse",
    /* 04  "Always display the transaction hash"                              */
    "Transaktions-Hash immer anzeigen",
    /* 05  "Always display the transaction or message hash"                   */
    "Transaktions- oder Nachrichten-Hash immer anzeigen",
    /* 06  "Amount"                                                           */
    "Betrag",
    /* 07  "Back to safety"                                                   */
    "Zur\u00fcck zur Sicherheit",
    /* 08  "Blind signing must\nbe enabled in\nsettings"                      */
    "Blindes Signieren muss\nin den Einstellungen\naktiviert werden",
    /* 09  "Blind signing"                                                    */
    "Blindes Signieren",
    /* 10  "By enabling, you accept T&Cs: ledger.com/tx-check"               */
    "Durch Aktivierung akzeptieren Sie die AGB: ledger.com/tx-check",
    /* 11  "Contract address"                                                 */
    "Vertragsadresse",
    /* 12  "Contract owner"                                                   */
    "Vertragsinhaber",
    /* 13  "Contract"                                                         */
    "Vertrag",
    /* 14  "Debug contracts"                                                  */
    "Vertr\u00e4ge debuggen",
    /* 15  "Debug smart contracts"                                            */
    "Smart Contracts debuggen",
    /* 16  "Delegate on network"                                              */
    "Im Netzwerk delegieren",
    /* 17  "Delegate to"                                                      */
    "Delegieren an",
    /* 18  "Deployed on"                                                      */
    "Bereitgestellt auf",
    /* 19  "Display contract data details"                                    */
    "Vertragsdatendetails anzeigen",
    /* 20  "Display contract\ndata details"                                   */
    "Vertragsdetails\nanzeigen",
    /* 21  "Display nonce in transactions"                                    */
    "Nonce in Transaktionen anzeigen",
    /* 22  "Displays raw content of EIP712 messages"                         */
    "Rohen Inhalt von EIP712-Nachrichten anzeigen",
    /* 23  "Enable blind signing in the settings to sign this transaction."   */
    "Aktivieren Sie blindes Signieren in den Einstellungen, "
    "um diese Transaktion zu signieren.",
    /* 24  "Enable EIP-7702 authorizations for smart contract delegation"     */
    "EIP-7702-Autorisierungen f\u00fcr Smart-Contract-Delegation aktivieren",
    /* 25  "Enable EIP-7702 authorizations"                                  */
    "EIP-7702-Autorisierungen aktivieren",
    /* 26  "Enable smart account upgrade in the settings to sign this auth."  */
    "Aktivieren Sie das Smart-Account-Upgrade in den Einstellungen, "
    "um diese Autorisierung zu signieren.",
    /* 27  "Enable transaction blind signing"                                 */
    "Blindes Transaktions-Signieren aktivieren",
    /* 28  "Enable\nTransaction Check?"                                       */
    "Transaktionspr\u00fcfung\naktivieren?",
    /* 29  "Get real-time warnings ... Powered by service providers."         */
    "Erhalten Sie Echtzeit-Warnungen zu riskanten Ethereum-Transaktionen. "
    "Bereitgestellt von Dienstanbietern.",
    /* 30  "Get real-time warnings ... (second variant)"                      */
    "Erhalten Sie Echtzeit-Warnungen zu riskanten Transaktionen. "
    "Mehr erfahren: ledger.com/tx-check",
    /* 31  "From"                                                             */
    "Von",
    /* 32  "Go to settings"                                                   */
    "Zu den Einstellungen",
    /* 33  "How it works"                                                     */
    "Wie es funktioniert",
    /* 34  "Interaction with"                                                 */
    "Interaktion mit",
    /* 35  "Key"                                                              */
    "Schl\u00fcssel",
    /* 36  "Max fees"                                                         */
    "Maximale Geb\u00fchren",
    /* 37  "Maybe later"                                                      */
    "Vielleicht sp\u00e4ter",
    /* 38  "Message"                                                          */
    "Nachricht",
    /* 39  "Network"                                                          */
    "Netzwerk",
    /* 40  "Nonce"                                                            */
    "Nonce",
    /* 41  "On network"                                                       */
    "Im Netzwerk",
    /* 42  "Parameter"                                                        */
    "Parameter",
    /* 43  "Provide public\nprivacy key"                                      */
    "\u00d6ffentlichen\nDatenschutzschl\u00fcssel angeben",
    /* 44  "Provide public\nsecret key"                                       */
    "\u00d6ffentlichen\ngeheimen Schl\u00fcssel angeben",
    /* 45  "Raw messages"                                                     */
    "Rohe Nachrichten",
    /* 46  "Reject authorization"                                             */
    "Autorisierung ablehnen",
    /* 47  "Reject transaction"                                               */
    "Transaktion ablehnen",
    /* 48  "Review authorization to revoke smart contract delegation?"        */
    "Autorisierung zum Widerrufen der Smart-Contract-Delegation \u00fcberpr\u00fcfen?",
    /* 49  "Review authorization to upgrade into smart contract account?"     */
    "Autorisierung f\u00fcr das Upgrade auf ein Smart-Contract-Konto \u00fcberpr\u00fcfen?",
    /* 50  "Review message"                                                   */
    "Nachricht \u00fcberpr\u00fcfen",
    /* 51  "Review transaction"                                               */
    "Transaktion \u00fcberpr\u00fcfen",
    /* 52  "Review typed message"                                             */
    "Typisierte Nachricht \u00fcberpr\u00fcfen",
    /* 53  "Revoke on network"                                                */
    "Im Netzwerk widerrufen",
    /* 54  "Safe Address rejected"                                            */
    "Safe-Adresse abgelehnt",
    /* 55  "Safe Address validated"                                           */
    "Safe-Adresse validiert",
    /* 56  "Scan the QR Code on your Ledger device for details..."            */
    "Scannen Sie den QR-Code auf Ihrem Ledger-Ger\u00e4t "
    "f\u00fcr Details zu Bedrohungen oder Risiken.",
    /* 57  "Scan to view on Etherscan"                                        */
    "Scannen, um auf Etherscan anzuzeigen",
    /* 58  "Sign authorization to revoke smart contract delegation?"          */
    "Autorisierung zum Widerrufen der Smart-Contract-Delegation signieren?",
    /* 59  "Sign authorization to upgrade into smart contract account?"       */
    "Autorisierung f\u00fcr das Upgrade auf ein Smart-Contract-Konto signieren?",
    /* 60  "Sign operation"                                                   */
    "Vorgang signieren",
    /* 61  "Smart account upgrade"                                            */
    "Smart-Account-Upgrade",
    /* 62  "Smart accounts"                                                   */
    "Smart Accounts",
    /* 63  "Smart contract information"                                       */
    "Smart-Contract-Informationen",
    /* 64  "Smart contract owner"                                             */
    "Smart-Contract-Inhaber",
    /* 65  "Smart contract"                                                   */
    "Smart Contract",
    /* 66  "The result is displayed: Critical threat, potential risk or no threat." */
    "Das angezeigte Ergebnis: Kritische Bedrohung, potenzielles Risiko oder keine Bedrohung.",
    /* 67  "This authorization cannot be signed"                              */
    "Diese Autorisierung kann nicht signiert werden",
    /* 68  "This authorization involves a delegation ..."                     */
    "Diese Autorisierung beinhaltet eine Delegation zu einem Smart Contract, "
    "der nicht auf der Zulassungsliste steht.",
    /* 69  "This transaction cannot be clear-signed"                          */
    "Diese Transaktion kann nicht klar signiert werden",
    /* 70  "Threshold"                                                        */
    "Schwellenwert",
    /* 71  "Transaction Check enabled"                                        */
    "Transaktionspr\u00fcfung aktiviert",
    /* 72  "Transaction Check"                                                */
    "Transaktionspr\u00fcfung",
    /* 73  "Transaction hash"                                                 */
    "Transaktions-Hash",
    /* 74  "Transaction is checked for threats before signing."               */
    "Die Transaktion wird vor dem Signieren auf Bedrohungen gepr\u00fcft.",
    /* 75  "Tx hash"                                                          */
    "Tx-Hash",
    /* 76  "Unable to sign"                                                   */
    "Signieren nicht m\u00f6glich",
    /* 77  "Verify Safe address"                                              */
    "Safe-Adresse verifizieren",
};

#endif  // HAVE_LANG_GERMAN
