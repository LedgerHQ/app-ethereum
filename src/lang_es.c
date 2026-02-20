/**
 * @file lang_es.c
 * @brief Spanish (ES) language string table for binary size comparison testing.
 *
 * Compiled only when HAVE_LANG_SPANISH is defined:
 *   make TARGET=flex ADD_SPANISH_LANG=1
 *
 * 78 genuine UI display strings — mirrors lang_fr.c exactly (same 78 originals).
 * Format strings (snprintf % specifiers) are excluded.
 */

#ifdef HAVE_LANG_SPANISH

#include "lang_es.h"

__attribute__((used)) const char *const LANG_ES_STRINGS[LANG_ES_STRING_COUNT] = {
    /* 00  "Accept risk and sign"                                             */
    "Aceptar riesgo y firmar",
    /* 01  "Accept threat and sign"                                           */
    "Aceptar la amenaza y firmar",
    /* 02  "Account"                                                          */
    "Cuenta",
    /* 03  "Address"                                                          */
    "Direcci\u00f3n",
    /* 04  "Always display the transaction hash"                              */
    "Mostrar siempre el hash de transacci\u00f3n",
    /* 05  "Always display the transaction or message hash"                   */
    "Mostrar siempre el hash de transacci\u00f3n o mensaje",
    /* 06  "Amount"                                                           */
    "Importe",
    /* 07  "Back to safety"                                                   */
    "Volver a la seguridad",
    /* 08  "Blind signing must\nbe enabled in\nsettings"                      */
    "La firma ciega debe\nactivarse en\nla configuraci\u00f3n",
    /* 09  "Blind signing"                                                    */
    "Firma ciega",
    /* 10  "By enabling, you accept T&Cs: ledger.com/tx-check"               */
    "Al activar, acepta los T\u00e9rminos: ledger.com/tx-check",
    /* 11  "Contract address"                                                 */
    "Direcci\u00f3n del contrato",
    /* 12  "Contract owner"                                                   */
    "Propietario del contrato",
    /* 13  "Contract"                                                         */
    "Contrato",
    /* 14  "Debug contracts"                                                  */
    "Depurar contratos",
    /* 15  "Debug smart contracts"                                            */
    "Depurar contratos inteligentes",
    /* 16  "Delegate on network"                                              */
    "Delegar en la red",
    /* 17  "Delegate to"                                                      */
    "Delegar a",
    /* 18  "Deployed on"                                                      */
    "Desplegado en",
    /* 19  "Display contract data details"                                    */
    "Mostrar detalles de datos del contrato",
    /* 20  "Display contract\ndata details"                                   */
    "Mostrar detalles\ndel contrato",
    /* 21  "Display nonce in transactions"                                    */
    "Mostrar nonce en transacciones",
    /* 22  "Displays raw content of EIP712 messages"                         */
    "Muestra el contenido bruto de mensajes EIP712",
    /* 23  "Enable blind signing in the settings to sign this transaction."   */
    "Active la firma ciega en ajustes para firmar esta transacci\u00f3n.",
    /* 24  "Enable EIP-7702 authorizations for smart contract delegation"     */
    "Activar autorizaciones EIP-7702 para delegaci\u00f3n de contrato inteligente",
    /* 25  "Enable EIP-7702 authorizations"                                  */
    "Activar autorizaciones EIP-7702",
    /* 26  "Enable smart account upgrade in the settings to sign this auth."  */
    "Active la actualizaci\u00f3n de cuenta inteligente en ajustes para firmar esta autorizaci\u00f3n.",
    /* 27  "Enable transaction blind signing"                                 */
    "Activar firma ciega de transacciones",
    /* 28  "Enable\nTransaction Check?"                                       */
    "\u00bfActivar\nVerificaci\u00f3n de transacciones?",
    /* 29  "Get real-time warnings ... Powered by service providers."         */
    "Reciba alertas en tiempo real sobre transacciones Ethereum peligrosas. "
    "Impulsado por proveedores de servicios.",
    /* 30  "Get real-time warnings ... (second variant)"                      */
    "Reciba alertas en tiempo real sobre transacciones peligrosas. "
    "M\u00e1s informaci\u00f3n: ledger.com/tx-check",
    /* 31  "From"                                                             */
    "De",
    /* 32  "Go to settings"                                                   */
    "Ir a ajustes",
    /* 33  "How it works"                                                     */
    "C\u00f3mo funciona",
    /* 34  "Interaction with"                                                 */
    "Interacci\u00f3n con",
    /* 35  "Key"                                                              */
    "Clave",
    /* 36  "Max fees"                                                         */
    "Tarifas m\u00e1ximas",
    /* 37  "Maybe later"                                                      */
    "Quiz\u00e1s m\u00e1s tarde",
    /* 38  "Message"                                                          */
    "Mensaje",
    /* 39  "Network"                                                          */
    "Red",
    /* 40  "Nonce"                                                            */
    "Nonce",
    /* 41  "On network"                                                       */
    "En la red",
    /* 42  "Parameter"                                                        */
    "Par\u00e1metro",
    /* 43  "Provide public\nprivacy key"                                      */
    "Proporcionar clave\np\u00fablica de privacidad",
    /* 44  "Provide public\nsecret key"                                       */
    "Proporcionar clave\np\u00fablica secreta",
    /* 45  "Raw messages"                                                     */
    "Mensajes sin procesar",
    /* 46  "Reject authorization"                                             */
    "Rechazar autorizaci\u00f3n",
    /* 47  "Reject transaction"                                               */
    "Rechazar transacci\u00f3n",
    /* 48  "Review authorization to revoke smart contract delegation?"        */
    "\u00bfRevisar autorizaci\u00f3n para revocar delegaci\u00f3n de contrato inteligente?",
    /* 49  "Review authorization to upgrade into smart contract account?"     */
    "\u00bfRevisar autorizaci\u00f3n de actualizaci\u00f3n a cuenta de contrato inteligente?",
    /* 50  "Review message"                                                   */
    "Revisar mensaje",
    /* 51  "Review transaction"                                               */
    "Revisar transacci\u00f3n",
    /* 52  "Review typed message"                                             */
    "Revisar mensaje tipificado",
    /* 53  "Revoke on network"                                                */
    "Revocar en la red",
    /* 54  "Safe Address rejected"                                            */
    "Direcci\u00f3n Safe rechazada",
    /* 55  "Safe Address validated"                                           */
    "Direcci\u00f3n Safe validada",
    /* 56  "Scan the QR Code on your Ledger device for details..."            */
    "Escanee el c\u00f3digo QR en su dispositivo Ledger para obtener "
    "detalles sobre cualquier amenaza o riesgo.",
    /* 57  "Scan to view on Etherscan"                                        */
    "Escanear para ver en Etherscan",
    /* 58  "Sign authorization to revoke smart contract delegation?"          */
    "\u00bfFirmar autorizaci\u00f3n para revocar delegaci\u00f3n de contrato inteligente?",
    /* 59  "Sign authorization to upgrade into smart contract account?"       */
    "\u00bfFirmar autorizaci\u00f3n de actualizaci\u00f3n a cuenta de contrato inteligente?",
    /* 60  "Sign operation"                                                   */
    "Firmar operaci\u00f3n",
    /* 61  "Smart account upgrade"                                            */
    "Actualizaci\u00f3n de cuenta inteligente",
    /* 62  "Smart accounts"                                                   */
    "Cuentas inteligentes",
    /* 63  "Smart contract information"                                       */
    "Informaci\u00f3n del contrato inteligente",
    /* 64  "Smart contract owner"                                             */
    "Propietario del contrato inteligente",
    /* 65  "Smart contract"                                                   */
    "Contrato inteligente",
    /* 66  "The result is displayed: Critical threat, potential risk or no threat." */
    "El resultado mostrado: amenaza cr\u00edtica, riesgo potencial o sin amenaza.",
    /* 67  "This authorization cannot be signed"                              */
    "Esta autorizaci\u00f3n no puede firmarse",
    /* 68  "This authorization involves a delegation ..."                     */
    "Esta autorizaci\u00f3n implica una delegaci\u00f3n a un contrato inteligente "
    "que no est\u00e1 en la lista de permitidos.",
    /* 69  "This transaction cannot be clear-signed"                          */
    "Esta transacci\u00f3n no puede firmarse claramente",
    /* 70  "Threshold"                                                        */
    "Umbral",
    /* 71  "Transaction Check enabled"                                        */
    "Verificaci\u00f3n de transacciones activada",
    /* 72  "Transaction Check"                                                */
    "Verificaci\u00f3n de transacciones",
    /* 73  "Transaction hash"                                                 */
    "Hash de transacci\u00f3n",
    /* 74  "Transaction is checked for threats before signing."               */
    "La transacci\u00f3n se verifica para detectar amenazas antes de firmar.",
    /* 75  "Tx hash"                                                          */
    "Hash Tx",
    /* 76  "Unable to sign"                                                   */
    "No se puede firmar",
    /* 77  "Verify Safe address"                                              */
    "Verificar direcci\u00f3n Safe",
};

#endif  // HAVE_LANG_SPANISH
