#include <string.h>
#include "eth_plugin_interface.h"
#include "shared_context.h"       // TODO : rewrite as independant code
#include "eth_plugin_internal.h"  // TODO : rewrite as independant code

#ifdef HAVE_STARKWARE

typedef enum {

    STARKWARE_REGISTER = 0,
    STARKWARE_DEPOSIT_TOKEN,
    STARKWARE_DEPOSIT_ETH,
    STARKWARE_DEPOSIT_CANCEL,
    STARKWARE_DEPOSIT_RECLAIM,
    STARKWARE_WITHDRAW,
    STARKWARE_FULL_WITHDRAW,
    STARKWARE_FREEZE,
    STARKWARE_ESCAPE,
    STARKWARE_VERIFY_ESCAPE,
    STARKWARE_WITHDRAW_TO,
    STARKWARE_DEPOSIT_NFT,
    STARKWARE_DEPOSIT_NFT_RECLAIM,
    STARKWARE_WITHDRAW_AND_MINT,
    STARKWARE_WITHDRAW_NFT,
    STARKWARE_WITHDRAW_NFT_TO
} starkwareSelector_t;

#ifndef HAVE_TOKENS_EXTRA_LIST

static const uint8_t DEVERSIFI_CONTRACT[] = {0x01, 0x5d, 0x22, 0x04, 0x5d, 0xac, 0xea,
                                             0xb0, 0x3b, 0x15, 0x80, 0x31, 0xec, 0xb7,
                                             0xd9, 0xd0, 0x6f, 0xad, 0x24, 0x60, 0x9b};

#else

static const uint8_t DEVERSIFI_CONTRACT[] = {
    0x02, 0xe7, 0x3a, 0x39, 0x4a, 0xde, 0x4d, 0x94, 0xa0, 0x73, 0x50, 0x2d, 0xa8, 0x70,
    0x3e, 0xa2, 0x34, 0x90, 0xdc, 0x7b, 0x6a, 0x69, 0xC6, 0x39, 0x2E, 0xb0, 0x2a, 0x28,
    0x82, 0x31, 0x41, 0x34, 0xc9, 0x8D, 0xDC, 0xBF, 0x73, 0xB7, 0xAd, 0xBa, 0xb1};

#endif

// register : address (20), stark key (32), drop param 3
// Registration
// Contract Name
// From ETH address
// Master account
// deposit token : stark key (32), verify assetType (32), vaultId (4), quantized Amount (32)
// Deposit
// Contract Name
// Master Account
// Token Account
// Amount
// deposit : stark key (32), verify assetType (32), vaultId (4)
// Flow similar to deposit
// deposit cancel, deposit reclaim : stark key (32), assetType (reclaim) / assetId (cancel) (32)
// ignored, vaultId(4) full withdrawal, freeze : stark key (32), vaultId (4) Cancel Deposit |
// Reclaim Deposit | Full Withdrawal | Freeze Contract Name Master Account Token Account
// withdrawal : stark key (32), verify assetType (32)
// Withdrawal
// Contract Name
// Master Account
// To Eth Address
// Token Symbol
// withdrawal to : stark key (32), verify assetType (32), address (20)
// Withdrawal To
// Contract Name
// Master Account
// To Eth Address
// Token Symbol
// escape : stark key (32), vaultId (4), verify assetType (32), quantized Amount (32)
// Escape
// Contract Name
// Amount
// Master Account
// Token Account
// verify escape : escapeProof (ignore)
// Verify Escape
// Contract Name
// deposit NFT : stark key (32), verify assetType (32), vault id (4), token id (32)
// Deposit
// Contract Name
// Master Account
// Token Account
// NFT Contract
// Token ID
// deposit NFT reclaim : stark key (32), verify assetType (32), vault id (4), token id (32)
// Reclaim Deposit
// Contract Name
// Master Account
// Token Account
// NFT Contract
// Token ID
// withdraw and mint : stark key (32), verify assetType (32), mintable blob (ignored variable)
// Withdrawal
// Contract Name
// Master Account
// Asset Contract
// withdraw NFT : stark key (32), verify assetType (32), token id (32)
// Withdrawal
// Contract Name
// Master Account
// To Eth Address
// NFT Contract
// Token ID
// withdraw NFT To : stark key (32), verify assetType (32), token id (32), address (20)
// Withdrawal To
// Contract Name
// Master Account
// To Eth Address
// NFT Contract
// Token ID

static const uint8_t STARKWARE_EXPECTED_DATA_SIZE[] = {0,
                                                       4 + 32 + 32 + 32 + 32,
                                                       4 + 32 + 32 + 32,
                                                       4 + 32 + 32 + 32,
                                                       4 + 32 + 32 + 32,
                                                       4 + 32 + 32,
                                                       4 + 32 + 32,
                                                       4 + 32 + 32,
                                                       4 + 32 + 32 + 32 + 32,
                                                       0,
                                                       4 + 32 + 32 + 32,
                                                       4 + 32 + 32 + 32 + 32,
                                                       4 + 32 + 32 + 32 + 32,
                                                       0,
                                                       4 + 32 + 32 + 32,
                                                       4 + 32 + 32 + 32 + 32};

static const uint8_t STARKWARE_NUM_SCREENS[] = {4 - 1,
                                                5 - 1,
                                                5 - 1,
                                                4 - 1,
                                                4 - 1,
                                                5 - 1,
                                                4 - 1,
                                                4 - 1,
                                                5 - 1,
                                                2 - 1,
                                                5 - 1,
                                                6 - 1,
                                                6 - 1,
                                                4 - 1,
                                                6 - 1,
                                                6 - 1};

typedef struct starkware_parameters_t {
    uint8_t vaultId[4];
    uint8_t selectorIndex;
    uint8_t starkKey[32];
    uint8_t amount[32];
    uint8_t validToken;

} starkware_parameters_t;

bool is_deversify_contract(const uint8_t *address) {
    uint32_t offset = 0;
    uint8_t size = DEVERSIFI_CONTRACT[0];
    uint8_t i;

    for (i = 0; i < DEVERSIFI_CONTRACT[0]; i++) {
        if (memcmp(address, DEVERSIFI_CONTRACT + offset + 1, 20) == 0) {
            return true;
        }
        offset += 20;
    }
    return false;
}

// TODO : rewrite as independant code
bool starkware_verify_asset_id(uint8_t *tmp32, uint8_t *tokenId, bool assetTypeOnly) {
    if (quantumSet) {
        cx_sha3_t sha3;
        tokenDefinition_t *currentToken = NULL;
        if (dataContext.tokenContext.quantumIndex != MAX_TOKEN) {
            currentToken = &tmpCtx.transactionContext.tokens[dataContext.tokenContext.quantumIndex];
        }
        cx_keccak_init(&sha3, 256);
        compute_token_id(&sha3,
                         (currentToken != NULL ? currentToken->address : NULL),
                         dataContext.tokenContext.quantumType,
                         dataContext.tokenContext.quantum,
                         dataContext.tokenContext.mintingBlob,
                         assetTypeOnly,
                         tmp32);
        if (memcmp(tokenId, tmp32, 32) != 0) {
            PRINTF("Token ID not matching - computed %.*H\n", 32, tmp32);
            PRINTF("Current quantum %.*H\n", 32, dataContext.tokenContext.quantum);
            PRINTF("Requested %.*H\n", 32, tokenId);
            return false;
        }
    } else {
        PRINTF("Quantum not set\n");
        return false;
    }
    return true;
}

bool starkware_verify_nft_token_id(uint8_t *tokenId) {
    if (!quantumSet) {
        PRINTF("Quantum not set\n");
        return false;
    }
    switch (dataContext.tokenContext.quantumType) {
        case STARK_QUANTUM_ERC721:
        case STARK_QUANTUM_MINTABLE_ERC721:
            break;
        default:
            PRINTF("Unexpected quantum type for NFT token id check %d\n",
                   dataContext.tokenContext.quantumType);
            return false;
    }
    if (memcmp(dataContext.tokenContext.quantum, tokenId, 32) != 0) {
        PRINTF("Token ID not matching - expected %.*H\n", 32, dataContext.tokenContext.quantum);
        PRINTF("Current token ID %.*H\n", 32, tokenId);
        return false;
    }
    return true;
}

void starkware_print_vault_id(uint32_t vaultId, char *destination) {
    snprintf(destination, 10, "%d", vaultId);
}

void starkware_print_stark_key(uint8_t *starkKey, char *destination) {
    snprintf(destination, 70, "0x%.*H", 32, starkKey);
}

// TODO : rewrite as independant code
void starkware_print_eth_address(uint8_t *address, char *destination) {
    destination[0] = '0';
    destination[1] = 'x';
    getEthAddressStringFromBinary(address, destination + 2, &global_sha3, chainConfig);
    destination[42] = '\0';
}

// TODO : rewrite as independant code
void starkware_print_amount(uint8_t *amountData, char *destination, bool forEscape) {
    uint256_t amount, amountPre, quantum;
    uint8_t decimals;
    char *ticker = (char *) PIC(chainConfig->coinName);

    if ((amountData == NULL) ||
        (forEscape && (dataContext.tokenContext.quantumIndex == MAX_TOKEN))) {
        decimals = WEI_TO_ETHER;
        if (!forEscape) {
            convertUint256BE(tmpContent.txContent.value.value,
                             tmpContent.txContent.value.length,
                             &amount);
        } else {
            readu256BE(amountData, &amountPre);
        }
    } else {
        tokenDefinition_t *token =
            &tmpCtx.transactionContext.tokens[dataContext.tokenContext.quantumIndex];
        decimals = token->decimals;
        ticker = (char *) token->ticker;
        readu256BE(amountData, &amountPre);
    }
    if (amountData != NULL) {
        readu256BE(dataContext.tokenContext.quantum, &quantum);
        mul256(&amountPre, &quantum, &amount);
    }
    tostring256(&amount, 10, (char *) (G_io_apdu_buffer + 100), 100);
    strcpy(destination, ticker);
    adjustDecimals((char *) (G_io_apdu_buffer + 100),
                   strlen((char *) (G_io_apdu_buffer + 100)),
                   destination + strlen(ticker),
                   50 - strlen(ticker),
                   decimals);
}

// TODO : rewrite as independant code
void starkware_print_ticker(char *destination) {
    char *ticker = (char *) PIC(chainConfig->coinName);

    if (dataContext.tokenContext.quantumIndex != MAX_TOKEN) {
        tokenDefinition_t *token =
            &tmpCtx.transactionContext.tokens[dataContext.tokenContext.quantumIndex];
        ticker = (char *) token->ticker;
    }
    strcpy(destination, ticker);
}

// TODO : rewrite as independant code
void starkware_print_asset_contract(char *destination) {
    // token has been validated to be present previously
    if (dataContext.tokenContext.quantumIndex != MAX_TOKEN) {
        tokenDefinition_t *token =
            &tmpCtx.transactionContext.tokens[dataContext.tokenContext.quantumIndex];
        starkware_print_eth_address(token->address, destination);
    } else {
        strcpy(destination, "UNKNOWN");
    }
}

// TODO : rewrite as independant code
void starkware_get_source_address(char *destination) {
    uint8_t privateKeyData[32];
    cx_ecfp_private_key_t privateKey;
    cx_ecfp_public_key_t publicKey;
    os_perso_derive_node_bip32(CX_CURVE_256K1,
                               tmpCtx.transactionContext.bip32Path,
                               tmpCtx.transactionContext.pathLength,
                               privateKeyData,
                               NULL);
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
    io_seproxyhal_io_heartbeat();
    cx_ecfp_generate_pair(CX_CURVE_256K1, &publicKey, &privateKey, 1);
    explicit_bzero(&privateKey, sizeof(privateKey));
    explicit_bzero(privateKeyData, sizeof(privateKeyData));
    io_seproxyhal_io_heartbeat();
    destination[0] = '0';
    destination[1] = 'x';
    getEthAddressStringFromKey(&publicKey, destination + 2, &global_sha3, chainConfig);
    destination[42] = '\0';
}

void starkware_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            uint8_t i;
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            starkware_parameters_t *context = (starkware_parameters_t *) msg->pluginContext;
            PRINTF("starkware plugin init\n");
            for (i = 0; i < NUM_STARKWARE_SELECTORS; i++) {
                if (memcmp(PIC(STARKWARE_SELECTORS[i]), msg->selector, SELECTOR_SIZE) == 0) {
                    context->selectorIndex = i;
                    break;
                }
            }
            if (i == NUM_STARKWARE_SELECTORS) {
                PRINTF("Unknown selector %.*H\n", SELECTOR_SIZE, msg->selector);
                break;
            }
            if (STARKWARE_EXPECTED_DATA_SIZE[context->selectorIndex] != 0) {
                if (msg->dataSize != STARKWARE_EXPECTED_DATA_SIZE[context->selectorIndex]) {
                    PRINTF("Unexpected data size for command %d expected %d got %d\n",
                           context->selectorIndex,
                           STARKWARE_EXPECTED_DATA_SIZE[context->selectorIndex],
                           msg->dataSize);
                    break;
                }
            }
            context->validToken = true;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
            starkware_parameters_t *context = (starkware_parameters_t *) msg->pluginContext;
            PRINTF("starkware plugin provide parameter %d %.*H\n",
                   msg->parameterOffset,
                   32,
                   msg->parameter);
            // Ignore for verify escape
            if (context->selectorIndex == STARKWARE_VERIFY_ESCAPE) {
                msg->result = ETH_PLUGIN_RESULT_OK;
                break;
            }
            switch (msg->parameterOffset) {
                case 4:
                    switch (context->selectorIndex) {
                        case STARKWARE_REGISTER:
                            memmove(context->amount, msg->parameter + 32 - 20, 20);
                            break;

                        case STARKWARE_DEPOSIT_TOKEN:
                        case STARKWARE_DEPOSIT_ETH:
                        case STARKWARE_DEPOSIT_CANCEL:
                        case STARKWARE_DEPOSIT_RECLAIM:
                        case STARKWARE_WITHDRAW:
                        case STARKWARE_FULL_WITHDRAW:
                        case STARKWARE_FREEZE:
                        case STARKWARE_ESCAPE:
                        case STARKWARE_WITHDRAW_TO:
                        case STARKWARE_DEPOSIT_NFT:
                        case STARKWARE_DEPOSIT_NFT_RECLAIM:
                        case STARKWARE_WITHDRAW_AND_MINT:
                        case STARKWARE_WITHDRAW_NFT:
                        case STARKWARE_WITHDRAW_NFT_TO:
                            memmove(context->starkKey, msg->parameter, 32);
                            break;

                        default:
                            break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 4 + 32:
                    switch (context->selectorIndex) {
                        case STARKWARE_REGISTER:
                            memmove(context->starkKey, msg->parameter, 32);
                            break;

                        case STARKWARE_ESCAPE:
                            memmove(context->vaultId, msg->parameter + 32 - 4, 4);
                            break;

                        case STARKWARE_DEPOSIT_CANCEL:
                        case STARKWARE_DEPOSIT_RECLAIM:
                            break;

                        case STARKWARE_FULL_WITHDRAW:
                        case STARKWARE_FREEZE:
                            memmove(context->vaultId, msg->parameter + 32 - 4, 4);
                            break;

                        case STARKWARE_DEPOSIT_ETH:
                        case STARKWARE_DEPOSIT_TOKEN:
                        case STARKWARE_WITHDRAW:
                        case STARKWARE_WITHDRAW_TO:
                        case STARKWARE_DEPOSIT_NFT:
                        case STARKWARE_DEPOSIT_NFT_RECLAIM:
                        case STARKWARE_WITHDRAW_AND_MINT:
                        case STARKWARE_WITHDRAW_NFT:
                        case STARKWARE_WITHDRAW_NFT_TO:
                            context->validToken =
                                starkware_verify_asset_id(context->amount, msg->parameter, true);
                            break;

                        default:
                            break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 4 + 32 + 32:
                    switch (context->selectorIndex) {
                        case STARKWARE_ESCAPE:
                            context->validToken =
                                starkware_verify_asset_id(context->amount, msg->parameter, true);
                            break;

                        case STARKWARE_DEPOSIT_CANCEL:
                        case STARKWARE_DEPOSIT_RECLAIM:
                        case STARKWARE_DEPOSIT_ETH:
                        case STARKWARE_DEPOSIT_TOKEN:
                        case STARKWARE_DEPOSIT_NFT:
                        case STARKWARE_DEPOSIT_NFT_RECLAIM:
                            memmove(context->vaultId, msg->parameter + 32 - 4, 4);
                            break;

                        case STARKWARE_WITHDRAW_TO:
                            memmove(context->amount, msg->parameter + 32 - 20, 20);
                            break;

                        case STARKWARE_WITHDRAW_NFT:
                        case STARKWARE_WITHDRAW_NFT_TO:
                            context->validToken = starkware_verify_nft_token_id(msg->parameter);
                            break;

                        default:
                            break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 4 + 32 + 32 + 32:
                    switch (context->selectorIndex) {
                        case STARKWARE_ESCAPE:
                        case STARKWARE_DEPOSIT_TOKEN:
                            memmove(context->amount, msg->parameter, 32);
                            break;

                        case STARKWARE_WITHDRAW_NFT_TO:
                            memmove(context->amount, msg->parameter + 32 - 20, 20);
                            break;

                        case STARKWARE_DEPOSIT_NFT:
                        case STARKWARE_DEPOSIT_NFT_RECLAIM:
                            context->validToken = starkware_verify_nft_token_id(msg->parameter);
                            break;

                        default:
                            break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                default:
                    switch (context->selectorIndex) {
                        case STARKWARE_REGISTER:
                        case STARKWARE_VERIFY_ESCAPE:
                            msg->result = ETH_PLUGIN_RESULT_OK;
                            break;

                        default:
                            PRINTF("Unhandled parameter offset\n");
                            break;
                    }
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            starkware_parameters_t *context = (starkware_parameters_t *) msg->pluginContext;
            PRINTF("starkware plugin finalize\n");
            if (!context->validToken) {
                msg->result = ETH_PLUGIN_RESULT_FALLBACK;
            } else {
                msg->uiType = ETH_UI_TYPE_GENERIC;
                msg->numScreens = STARKWARE_NUM_SCREENS[context->selectorIndex];
                msg->result = ETH_PLUGIN_RESULT_OK;
            }
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            starkware_parameters_t *context = (starkware_parameters_t *) msg->pluginContext;
            PRINTF("starkware query contract id\n");
            switch (context->selectorIndex) {
                case STARKWARE_REGISTER:
                    strcpy(msg->name, "Register");
                    break;
                case STARKWARE_DEPOSIT_TOKEN:
                case STARKWARE_DEPOSIT_ETH:
                case STARKWARE_DEPOSIT_NFT:
                    strcpy(msg->name, "Deposit");
                    break;
                case STARKWARE_DEPOSIT_CANCEL:
                    strcpy(msg->name, "Cancel Deposit");
                    break;
                case STARKWARE_DEPOSIT_RECLAIM:
                case STARKWARE_DEPOSIT_NFT_RECLAIM:
                    strcpy(msg->name, "Reclaim Deposit");
                    break;
                case STARKWARE_WITHDRAW:
                case STARKWARE_WITHDRAW_NFT:
                case STARKWARE_WITHDRAW_AND_MINT:
                    strcpy(msg->name, "Withdrawal");
                    break;
                case STARKWARE_FULL_WITHDRAW:
                    strcpy(msg->name, "Full Withdrawal");
                    break;
                case STARKWARE_FREEZE:
                    strcpy(msg->name, "Freeze");
                    break;
                case STARKWARE_ESCAPE:
                    strcpy(msg->name, "Escape");
                    break;
                case STARKWARE_VERIFY_ESCAPE:
                    strcpy(msg->name, "Verify Escape");
                    break;
                case STARKWARE_WITHDRAW_TO:
                case STARKWARE_WITHDRAW_NFT_TO:
                    strcpy(msg->name, "Withdrawal To");
                    break;

                default:
                    break;
            }
            strcpy(msg->version,
                   is_deversify_contract(tmpContent.txContent.destination) ? "DeversiFi"
                                                                           : "Starkware");
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            starkware_parameters_t *context = (starkware_parameters_t *) msg->pluginContext;
            switch (msg->screenIndex) {
                case 0:
                    strcpy(msg->title, "Contract Name");
                    if (is_deversify_contract(tmpContent.txContent.destination)) {
                        strcpy(msg->msg, "DeversiFi");
                    } else {
                        starkware_print_eth_address(tmpContent.txContent.destination, msg->msg);
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                case 1:
                    switch (context->selectorIndex) {
                        case STARKWARE_REGISTER:
                            strcpy(msg->title, "From ETH Address");
                            starkware_print_eth_address(context->amount, msg->msg);
                            break;
                        case STARKWARE_ESCAPE:
                            strcpy(msg->title, "Amount");
                            starkware_print_amount(context->amount, msg->msg, true);
                            break;
                        case STARKWARE_DEPOSIT_TOKEN:
                        case STARKWARE_DEPOSIT_ETH:
                        case STARKWARE_DEPOSIT_CANCEL:
                        case STARKWARE_DEPOSIT_RECLAIM:
                        case STARKWARE_WITHDRAW:
                        case STARKWARE_FULL_WITHDRAW:
                        case STARKWARE_FREEZE:
                        case STARKWARE_VERIFY_ESCAPE:
                        case STARKWARE_WITHDRAW_TO:
                        case STARKWARE_DEPOSIT_NFT:
                        case STARKWARE_DEPOSIT_NFT_RECLAIM:
                        case STARKWARE_WITHDRAW_AND_MINT:
                        case STARKWARE_WITHDRAW_NFT:
                        case STARKWARE_WITHDRAW_NFT_TO:
                            strcpy(msg->title, "Master Account");
                            starkware_print_stark_key(context->starkKey, msg->msg);
                            break;
                        default:
                            PRINTF("Unexpected screen %d for %d\n",
                                   msg->screenIndex,
                                   context->selectorIndex);
                            break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                case 2:
                    switch (context->selectorIndex) {
                        case STARKWARE_REGISTER:
                        case STARKWARE_ESCAPE:
                            strcpy(msg->title, "Master Account");
                            starkware_print_stark_key(context->starkKey, msg->msg);
                            break;

                        case STARKWARE_DEPOSIT_TOKEN:
                        case STARKWARE_DEPOSIT_ETH:
                        case STARKWARE_DEPOSIT_CANCEL:
                        case STARKWARE_DEPOSIT_RECLAIM:
                        case STARKWARE_FULL_WITHDRAW:
                        case STARKWARE_FREEZE:
                        case STARKWARE_DEPOSIT_NFT:
                        case STARKWARE_DEPOSIT_NFT_RECLAIM:
                            strcpy(msg->title, "Token Account");
                            starkware_print_vault_id(U4BE(context->vaultId, 0), msg->msg);
                            break;
                        case STARKWARE_WITHDRAW:
                        case STARKWARE_WITHDRAW_NFT:
                            strcpy(msg->title, "To ETH Address");
                            starkware_get_source_address(msg->msg);
                            break;
                        case STARKWARE_WITHDRAW_TO:
                        case STARKWARE_WITHDRAW_NFT_TO:
                            strcpy(msg->title, "To ETH Address");
                            starkware_print_eth_address(context->amount, msg->msg);
                            break;
                        case STARKWARE_WITHDRAW_AND_MINT:
                            strcpy(msg->title, "Asset Contract");
                            starkware_print_asset_contract(msg->msg);
                            break;

                        default:
                            PRINTF("Unexpected screen %d for %d\n",
                                   msg->screenIndex,
                                   context->selectorIndex);
                            break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 3:
                    switch (context->selectorIndex) {
                        case STARKWARE_ESCAPE:
                            strcpy(msg->title, "Token Account");
                            starkware_print_vault_id(U4BE(context->vaultId, 0), msg->msg);
                            break;
                        case STARKWARE_DEPOSIT_TOKEN:
                        case STARKWARE_DEPOSIT_ETH:
                            strcpy(msg->title, "Amount");
                            starkware_print_amount(
                                (context->selectorIndex == STARKWARE_DEPOSIT_ETH ? NULL
                                                                                 : context->amount),
                                msg->msg,
                                false);
                            break;
                        case STARKWARE_WITHDRAW:
                        case STARKWARE_WITHDRAW_TO:
                            strcpy(msg->title, "Token Symbol");
                            starkware_print_ticker(msg->msg);
                            break;

                        case STARKWARE_WITHDRAW_NFT:
                        case STARKWARE_WITHDRAW_NFT_TO:
                        case STARKWARE_DEPOSIT_NFT:
                        case STARKWARE_DEPOSIT_NFT_RECLAIM:
                            strcpy(msg->title, "NFT Contract");
                            starkware_print_asset_contract(msg->msg);
                            break;

                        default:
                            PRINTF("Unexpected screen %d for %d\n",
                                   msg->screenIndex,
                                   context->selectorIndex);
                            break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 4:
                    switch (context->selectorIndex) {
                        case STARKWARE_WITHDRAW_NFT:
                        case STARKWARE_WITHDRAW_NFT_TO:
                        case STARKWARE_DEPOSIT_NFT:
                        case STARKWARE_DEPOSIT_NFT_RECLAIM:
                            strcpy(msg->title, "TokenID");
                            starkware_print_stark_key(dataContext.tokenContext.quantum, msg->msg);
                            break;

                        default:
                            PRINTF("Unexpected screen %d for %d\n",
                                   msg->screenIndex,
                                   context->selectorIndex);
                            break;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                default:
                    PRINTF("Unexpected screen %d for %d\n",
                           msg->screenIndex,
                           context->selectorIndex);
                    break;
            }
        } break;

        default:
            PRINTF("Unhandled message %d\n", message);
    }
}

#endif
