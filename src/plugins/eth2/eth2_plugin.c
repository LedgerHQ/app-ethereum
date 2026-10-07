#ifdef HAVE_ETH2

#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "common_utils.h"
#include "network.h"
#include "eth2_plugin.h"
#include "feature_get_eth2_public_key.h"
#include "uint256.h"

static const uint8_t ETH2_DEPOSIT_SELECTOR[SELECTOR_SIZE] = {0x22, 0x89, 0x51, 0x18};
const uint8_t *const ETH2_SELECTORS[NUM_ETH2_SELECTORS] = {ETH2_DEPOSIT_SELECTOR};

#define WITHDRAWAL_KEY_PATH_1 12381
#define WITHDRAWAL_KEY_PATH_2 3600
#define WITHDRAWAL_KEY_PATH_4 0

#define ETH2_DEPOSIT_PUBKEY_OFFSET         0x80
#define ETH2_WITHDRAWAL_CREDENTIALS_OFFSET 0xE0
#define ETH2_SIGNATURE_OFFSET              0x120

static const uint8_t deposit_contract_address[ADDRESS_LENGTH] = {
    0x00, 0x00, 0x00, 0x00, 0x21, 0x9a, 0xb5, 0x40, 0x35, 0x6c,
    0xbb, 0x83, 0x9c, 0xbe, 0x05, 0x30, 0x3d, 0x77, 0x05, 0xfa,
};

const uint8_t *const ETH2_ADDRESSES[NUM_ETH2_ADDRESSES] = {deposit_contract_address};

// Highest index for withdrawal derivation path.
#define INDEX_MAX 65536  // 2 ^ 16 : arbitrary value to protect from path attacks.

// 1 Gwei, big-endian: the unit the deposit amount is committed in
static const uint8_t GWEI_BE[] = {0x3b, 0x9a, 0xca, 0x00};

// deposit(bytes pubkey, bytes withdrawal_credentials, bytes signature, bytes32
// deposit_data_root) is 13 calldata words: three dynamic offsets, the root,
// then a length and its data for each of the three byte strings.  Every one of
// them has to have been received and checked before the deposit may be
// presented as validated.
#define ETH2_DEPOSIT_WORD_COUNT 13
#define ETH2_DEPOSIT_ALL_WORDS  ((1u << ETH2_DEPOSIT_WORD_COUNT) - 1)

#define ETH2_DEPOSIT_ROOT_WORD  3
#define ETH2_DEPOSIT_SIG_WORD_1 10

typedef struct eth2_deposit_parameters_t {
    uint8_t valid;
    char deposit_address[BLS12381_G1_COMPRESSED_PUBKEY_LENGTH];
    // Fields the deposit_data_root commits to, kept so that it can be
    // recomputed and compared once the whole calldata has been received
    uint8_t withdrawal_credentials[INT256_LENGTH];
    uint8_t signature[BLS12381_G2_COMPRESSED_SIGNATURE_LENGTH];
    uint8_t deposit_data_root[INT256_LENGTH];
    // Bitmap of the calldata words received so far
    uint16_t received_words;
    // Withdrawal index captured at init, used for both the check and the review
    uint32_t withdrawal_index;
} eth2_deposit_parameters_t;

// Validate one of the fixed ABI offset/length words against its expected value.
// Any mismatch marks the deposit context as invalid.
static void check_offset_or_length(ethPluginProvideParameter_t *msg,
                                   eth2_deposit_parameters_t *context) {
    uint32_t check = 0;
    switch (msg->parameterOffset) {
        case 4 + (PARAMETER_LENGTH * 0):
            check = ETH2_DEPOSIT_PUBKEY_OFFSET;
            break;
        case 4 + (PARAMETER_LENGTH * 1):
            check = ETH2_WITHDRAWAL_CREDENTIALS_OFFSET;
            break;
        case 4 + (PARAMETER_LENGTH * 2):
            check = ETH2_SIGNATURE_OFFSET;
            break;
        case 4 + (PARAMETER_LENGTH * 4):
            check = BLS12381_G1_COMPRESSED_PUBKEY_LENGTH;
            break;
        case 4 + (PARAMETER_LENGTH * 7):
            check = INT256_LENGTH;
            break;
        case 4 + (PARAMETER_LENGTH * 9):
            check = BLS12381_G2_COMPRESSED_SIGNATURE_LENGTH;
            break;
        default:
            context->valid = 0;
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            return;
    }
    uint32_t index = U4BE(msg->parameter, PARAMETER_LENGTH - 4);
    if (index != check) {
        PRINTF("eth2 plugin parameter check %d failed, expected %d got %d\n",
               msg->parameterOffset,
               check,
               index);
        context->valid = 0;
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }
    msg->result = ETH_PLUGIN_RESULT_OK;
}

// Recompute the expected withdrawal credentials from the device's withdrawal
// key path and compare them against the provided ones.
static void check_withdrawal_credentials(ethPluginProvideParameter_t *msg,
                                         eth2_deposit_parameters_t *context) {
    uint8_t tmp[BLS12381_G1_COMPRESSED_PUBKEY_LENGTH] = {0};
    uint32_t withdrawalKeyPath[4];
    withdrawalKeyPath[0] = WITHDRAWAL_KEY_PATH_1;
    withdrawalKeyPath[1] = WITHDRAWAL_KEY_PATH_2;
    if (context->withdrawal_index > INDEX_MAX) {
        PRINTF("eth2 plugin: withdrawal index is too big\n");
        PRINTF("Got %u which is higher than INDEX_MAX (%u)\n",
               context->withdrawal_index,
               INDEX_MAX);
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        context->valid = 0;
        return;
    }
    withdrawalKeyPath[2] = context->withdrawal_index;
    withdrawalKeyPath[3] = WITHDRAWAL_KEY_PATH_4;
    // A failed derivation leaves tmp at its all-zeroes init, which would hash
    // into the public constant 0x00 || SHA256(zeros[48])[1:] credential
    if (get_eth2_public_key(withdrawalKeyPath, 4, tmp) != CX_OK) {
        PRINTF("eth2 plugin: failed to derive withdrawal public key\n");
        explicit_bzero(tmp, sizeof(tmp));
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        context->valid = 0;
        return;
    }
    PRINTF("eth2 plugin computed withdrawal public key %.*H\n",
           BLS12381_G1_COMPRESSED_PUBKEY_LENGTH,
           tmp);
    cx_hash_sha256(tmp, BLS12381_G1_COMPRESSED_PUBKEY_LENGTH, tmp, INT256_LENGTH);
    tmp[0] = 0;
    if (memcmp(tmp, msg->parameter, INT256_LENGTH) != 0) {
        PRINTF("eth2 plugin invalid withdrawal credentials\n");
        PRINTF("Got %.*H\n", INT256_LENGTH, msg->parameter);
        PRINTF("Expected %.*H\n", INT256_LENGTH, tmp);
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        context->valid = 0;
    } else {
        msg->result = ETH_PLUGIN_RESULT_OK;
    }
}

// The deposit value is committed to the deposit_data_root in whole Gwei
static bool tx_value_to_gwei(const txInt256_t *value, uint64_t *amount_gwei) {
    uint256_t wei;
    uint256_t unit;
    uint256_t amount;
    uint256_t remainder;

    if (value->length > INT256_LENGTH) {
        return false;
    }
    if (value->length == 0) {
        // RLP encodes zero as an empty field
        *amount_gwei = 0;
        return true;
    }
    convertUint256BE(value->value, value->length, &wei);
    convertUint256BE(GWEI_BE, sizeof(GWEI_BE), &unit);
    divmod256(&wei, &unit, &amount, &remainder);
    if (!zero256(&remainder)) {
        PRINTF("eth2 plugin: deposit value is not a whole number of Gwei\n");
        return false;
    }
    if (!zero128(&amount.elements[0]) || (amount.elements[1].elements[0] != 0)) {
        PRINTF("eth2 plugin: deposit amount does not fit a uint64\n");
        return false;
    }
    *amount_gwei = amount.elements[1].elements[1];
    return true;
}

/**
 * @brief Compute the SSZ hash_tree_root of the DepositData
 *
 * DepositData is {pubkey: Bytes48, withdrawal_credentials: Bytes32,
 * amount: uint64, signature: Bytes96}.  Each field is the root of its own
 * zero-padded 32-byte chunks, and the four resulting leaves are merkleised
 * into a two-level tree.
 *
 * @param[in] context the plugin context holding the received fields
 * @param[in] amount_gwei the deposit amount, in Gwei
 * @param[out] root where to store the computed root
 * @return whether the computation succeeded
 */
static bool compute_deposit_data_root(const eth2_deposit_parameters_t *context,
                                      uint64_t amount_gwei,
                                      uint8_t *root) {
    uint8_t buf[2 * INT256_LENGTH];
    uint8_t left[INT256_LENGTH];
    uint8_t right[INT256_LENGTH];
    uint8_t tmp[INT256_LENGTH];

    // hash_tree_root(pubkey): 48 bytes zero-padded to two chunks
    explicit_bzero(buf, sizeof(buf));
    memcpy(buf, context->deposit_address, sizeof(context->deposit_address));
    if (cx_hash_sha256(buf, sizeof(buf), left, sizeof(left)) == 0) {
        return false;
    }
    // left leaf: node(hash_tree_root(pubkey), withdrawal_credentials)
    memcpy(buf, left, INT256_LENGTH);
    memcpy(buf + INT256_LENGTH, context->withdrawal_credentials, INT256_LENGTH);
    if (cx_hash_sha256(buf, sizeof(buf), left, sizeof(left)) == 0) {
        return false;
    }

    // hash_tree_root(signature): three chunks padded to four
    if (cx_hash_sha256(context->signature, 2 * INT256_LENGTH, right, sizeof(right)) == 0) {
        return false;
    }
    explicit_bzero(buf, sizeof(buf));
    memcpy(buf, context->signature + (2 * INT256_LENGTH), INT256_LENGTH);
    if (cx_hash_sha256(buf, sizeof(buf), tmp, sizeof(tmp)) == 0) {
        return false;
    }
    memcpy(buf, right, INT256_LENGTH);
    memcpy(buf + INT256_LENGTH, tmp, INT256_LENGTH);
    if (cx_hash_sha256(buf, sizeof(buf), right, sizeof(right)) == 0) {
        return false;
    }

    // right leaf: node(amount, hash_tree_root(signature)), the amount being a
    // little-endian uint64 in its own chunk
    explicit_bzero(buf, sizeof(buf));
    for (uint8_t i = 0; i < sizeof(amount_gwei); i++) {
        buf[i] = (amount_gwei >> (8 * i)) & 0xff;
    }
    memcpy(buf + INT256_LENGTH, right, INT256_LENGTH);
    if (cx_hash_sha256(buf, sizeof(buf), right, sizeof(right)) == 0) {
        return false;
    }

    memcpy(buf, left, INT256_LENGTH);
    memcpy(buf + INT256_LENGTH, right, INT256_LENGTH);
    return cx_hash_sha256(buf, sizeof(buf), root, INT256_LENGTH) != 0;
}

/**
 * @brief Check the deposit_data_root carried by the calldata against the fields
 *
 * @param[in] context the plugin context holding the received fields
 * @param[in] tx_content the transaction being signed
 * @return whether the root commits to exactly what was received and displayed
 */
static bool check_deposit_data_root(const eth2_deposit_parameters_t *context,
                                    const txContent_t *tx_content) {
    uint8_t root[INT256_LENGTH];
    uint64_t amount_gwei;

    if (tx_content == NULL) {
        return false;
    }
    if (!tx_value_to_gwei(&tx_content->value, &amount_gwei)) {
        return false;
    }
    if (!compute_deposit_data_root(context, amount_gwei, root)) {
        PRINTF("eth2 plugin: could not compute the deposit data root\n");
        return false;
    }
    if (memcmp(root, context->deposit_data_root, sizeof(root)) != 0) {
        PRINTF("eth2 plugin: deposit_data_root mismatch\n");
        PRINTF("  calldata %.*H\n", INT256_LENGTH, context->deposit_data_root);
        PRINTF("  computed %.*H\n", INT256_LENGTH, root);
        return false;
    }
    return true;
}

void eth2_plugin_call(eth_plugin_msg_t message, void *parameters) {
    if (parameters == NULL) {
        return;
    }
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            eth2_deposit_parameters_t *context = (eth2_deposit_parameters_t *) msg->pluginContext;
            explicit_bzero(context, sizeof(*context));
            context->valid = 1;
            context->withdrawal_index = eth2WithdrawalIndex;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
            eth2_deposit_parameters_t *context = (eth2_deposit_parameters_t *) msg->pluginContext;
            uint32_t word;

            PRINTF("eth2 plugin provide parameter %d %.*H\n",
                   msg->parameterOffset,
                   PARAMETER_LENGTH,
                   msg->parameter);
            if ((msg->parameterOffset < SELECTOR_SIZE) ||
                (((msg->parameterOffset - SELECTOR_SIZE) % PARAMETER_LENGTH) != 0)) {
                PRINTF("eth2 plugin: misaligned parameter offset %d\n", msg->parameterOffset);
                context->valid = 0;
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                return;
            }
            // Record which word this is, so that FINALIZE can require the whole
            // calldata to have gone through the checks below
            word = (msg->parameterOffset - SELECTOR_SIZE) / PARAMETER_LENGTH;
            if (word < ETH2_DEPOSIT_WORD_COUNT) {
                context->received_words |= (uint16_t) (1u << word);
            }
            switch (msg->parameterOffset) {
                case 4 + (PARAMETER_LENGTH * 0):  // pubkey offset
                case 4 + (PARAMETER_LENGTH * 1):  // withdrawal credentials offset
                case 4 + (PARAMETER_LENGTH * 2):  // signature offset
                case 4 + (PARAMETER_LENGTH * 4):  // deposit pubkey length
                case 4 + (PARAMETER_LENGTH * 7):  // withdrawal credentials length
                case 4 + (PARAMETER_LENGTH * 9):  // signature length
                    check_offset_or_length(msg, context);
                    break;

                case 4 + (PARAMETER_LENGTH * 5):  // deposit pubkey 1
                {
                    memcpy(context->deposit_address, msg->parameter, PARAMETER_LENGTH);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                }
                case 4 + (PARAMETER_LENGTH * 6):  // deposit pubkey 2
                {
                    // Copy the last 16 bytes. Storage stays raw (48-byte BLS
                    // G1 compressed pubkey); the screen renders the full
                    // value as hex in the QUERY_CONTRACT_UI handler.
                    memcpy(context->deposit_address + PARAMETER_LENGTH,
                           msg->parameter,
                           sizeof(context->deposit_address) - PARAMETER_LENGTH);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                }
                case 4 + (PARAMETER_LENGTH * 3):  // deposit data root
                    memcpy(context->deposit_data_root, msg->parameter, INT256_LENGTH);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 4 + (PARAMETER_LENGTH * 10):  // signature
                case 4 + (PARAMETER_LENGTH * 11):
                case 4 + (PARAMETER_LENGTH * 12):
                    memcpy(context->signature + ((word - ETH2_DEPOSIT_SIG_WORD_1) * INT256_LENGTH),
                           msg->parameter,
                           INT256_LENGTH);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 4 + (PARAMETER_LENGTH * 8):  // withdrawal credentials
                    check_withdrawal_credentials(msg, context);
                    if ((msg->result == ETH_PLUGIN_RESULT_OK) && context->valid) {
                        // Keep the credentials the root has to commit to
                        memcpy(context->withdrawal_credentials, msg->parameter, INT256_LENGTH);
                    }
                    break;

                default:
                    PRINTF("Unhandled parameter offset\n");
                    break;
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            eth2_deposit_parameters_t *context = (eth2_deposit_parameters_t *) msg->pluginContext;
            PRINTF("eth2 plugin finalize\n");
            if (get_tx_chain_id() != ETHEREUM_MAINNET_CHAINID) {
                PRINTF("eth2: deposit contract only valid on Ethereum mainnet\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            if (!context->valid) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            // A deposit is only validated once every field it commits to has
            // been received and checked, not merely because nothing failed
            if (context->received_words != ETH2_DEPOSIT_ALL_WORDS) {
                PRINTF("eth2 plugin: incomplete deposit calldata (words 0x%04x)\n",
                       context->received_words);
                context->valid = 0;
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            if (!check_deposit_data_root(context, msg->txContent)) {
                context->valid = 0;
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            // Amount, Validator, and the withdrawal key index the credentials were
            // validated against: that index comes from a separate unauthenticated
            // command, so it must not decide where the stake can be withdrawn
            // without the user seeing it.
            msg->numScreens = 3;
            msg->uiType = ETH_UI_TYPE_GENERIC;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            strlcpy(msg->name, "ETH2", msg->nameLength);
            strlcpy(msg->version, "Deposit", msg->versionLength);
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            eth2_deposit_parameters_t *context = (eth2_deposit_parameters_t *) msg->pluginContext;
            switch (msg->screenIndex) {
                case 0: {  // Amount screen
                    uint8_t decimals = WEI_TO_ETHER;
                    const char *ticker = g_chain_config->ticker;
                    strlcpy(msg->title, "Amount", msg->titleLength);
                    if (!amountToString(tmpContent.txContent.value.value,
                                        tmpContent.txContent.value.length,
                                        decimals,
                                        ticker,
                                        msg->msg,
                                        msg->msgLength)) {
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;
                case 1: {  // Deposit pubkey screen
                    strlcpy(msg->title, "Validator", msg->titleLength);
                    if (msg->msgLength < 3) {
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                    }
                    msg->msg[0] = '0';
                    msg->msg[1] = 'x';
                    if (bytes_to_lowercase_hex(&msg->msg[2],
                                               msg->msgLength - 2,
                                               context->deposit_address,
                                               sizeof(context->deposit_address)) != 0) {
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;
                case 2: {  // Withdrawal key index screen
                    strlcpy(msg->title, "Withdrawal key", msg->titleLength);
                    if (snprintf(msg->msg,
                                 msg->msgLength,
                                 "Index %u",
                                 (unsigned int) context->withdrawal_index) <= 0) {
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;
                default:
                    break;
            }
        } break;

        default:
            PRINTF("Unhandled message %d\n", message);
    }
}

#endif
