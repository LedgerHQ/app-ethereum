#include <string.h>
#include "os_utils.h"
#include "shared_context.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "gtp_tx_info.h"
#include "gtp_field_table.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "network.h"
#include "common_ui.h"
#include "feature_sign_tx.h"
#include "apdu_constants.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"
#include "enum_value.h"
#include "proxy_info.h"
#include "trusted_name.h"
#include "tx_ctx.h"
#include "token_info.h"
#include "nft_info.h"
#include "common_ui.h"
#ifdef HAVE_ADDRESS_BOOK
#include "handle_contacts.h"
#endif  // HAVE_ADDRESS_BOOK

static bool *index_allocated = NULL;

static void review_choice(bool confirm) {
    ui_gcs_cleanup();
    if (confirm) {
        io_seproxyhal_touch_tx_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_tx_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_idle);
    }
}

static void free_pair_extension_infolist_elem(const struct nbgl_contentInfoList_s *infolist,
                                              int idx) {
    APP_MEM_FREE((void *) infolist->infoTypes[idx]);
    APP_MEM_FREE((void *) infolist->infoContents[idx]);
    if (infolist->infoExtensions != NULL) {
        APP_MEM_FREE((void *) infolist->infoExtensions[idx].title);
        APP_MEM_FREE((void *) infolist->infoExtensions[idx].explanation);
        APP_MEM_FREE((void *) infolist->infoExtensions[idx].fullValue);
    }
}

static void free_pair_extension(const nbgl_contentValueExt_t *ext) {
    APP_MEM_FREE((void *) ext->backText);
    APP_MEM_FREE((void *) ext->fullValue);
    if (ext->infolist != NULL) {
        for (int i = 0; i < ext->infolist->nbInfos; ++i) {
            free_pair_extension_infolist_elem(ext->infolist, i);
        }
        APP_MEM_FREE((void *) ext->infolist->infoTypes);
        APP_MEM_FREE((void *) ext->infolist->infoContents);
        APP_MEM_FREE((void *) ext->infolist->infoExtensions);
        APP_MEM_FREE((void *) ext->infolist);
    }
    APP_MEM_FREE((void *) ext);
}

static void free_pair(const nbgl_contentTagValueList_t *pair_list, int idx) {
    // Only a few pairs are created from the UI, and need to be freed :
    // - the first one, that leads to the contract infos
    // - the second to last one, that shows the Network (optional)
    // - the last one, that shows the TX fees
    if ((index_allocated != NULL) && (index_allocated[idx] == true)) {
        APP_MEM_FREE((void *) pair_list->pairs[idx].item);
        APP_MEM_FREE((void *) pair_list->pairs[idx].value);
    }
    if (pair_list->pairs[idx].extension != NULL) {
        free_pair_extension(pair_list->pairs[idx].extension);
    }
}

#ifdef SCREEN_SIZE_WALLET
#define MAX_INFO_COUNT 3
#else
#define MAX_INFO_COUNT 4
#endif

static bool prepare_infos(nbgl_contentInfoList_t *infos) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    size_t off;
    uint8_t count = 0;
    const char **keys;
    const char **values;
    const char *value;
#ifdef SCREEN_SIZE_WALLET
    nbgl_contentValueExt_t *extensions;
    int contract_idx = -1;
#endif

    infos->nbInfos = MAX_INFO_COUNT;
    if (APP_MEM_CALLOC((void **) &keys, sizeof(*keys) * MAX_INFO_COUNT) == false) return false;
    infos->infoTypes = keys;

    if (APP_MEM_CALLOC((void **) &values, sizeof(*values) * MAX_INFO_COUNT) == false) return false;
    infos->infoContents = values;

    if ((value = get_creator_legal_name(get_current_tx_info())) != NULL) {
        snprintf(tmp_buf,
                 tmp_buf_size,
#ifdef SCREEN_SIZE_WALLET
                 "Smart contract owner"
#else
                 "Contract owner"
#endif
        );
        if ((keys[count] = APP_MEM_STRDUP(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((value = get_creator_url(get_current_tx_info())) != NULL) {
            off = strlen(tmp_buf);
            snprintf(tmp_buf + off, tmp_buf_size - off, "\n%s", value);
        }
        if ((values[count] = APP_MEM_STRDUP(tmp_buf)) == NULL) {
            return false;
        }
        count += 1;
    }

    if ((value = get_contract_name(get_current_tx_info())) != NULL) {
        snprintf(tmp_buf,
                 tmp_buf_size,
#ifdef SCREEN_SIZE_WALLET
                 "Smart contract"
#else
                 "Contract"
#endif
        );
        if ((keys[count] = APP_MEM_STRDUP(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((values[count] = APP_MEM_STRDUP(tmp_buf)) == NULL) {
            return false;
        }
#ifdef SCREEN_SIZE_WALLET
        contract_idx = count;
#endif
        count += 1;
    }

#ifndef SCREEN_SIZE_WALLET
    if (!getEthDisplayableAddress((uint8_t *) get_contract_addr(get_current_tx_info()),
                                  tmp_buf,
                                  tmp_buf_size,
                                  g_chain_config->chain_id)) {
        return false;
    }
    if ((keys[count] = APP_MEM_STRDUP("Contract address")) == NULL) {
        return false;
    }
    if ((values[count] = APP_MEM_STRDUP(tmp_buf)) == NULL) {
        return false;
    }
    count += 1;
#endif

    if ((value = get_deploy_date(get_current_tx_info())) != NULL) {
        snprintf(tmp_buf, tmp_buf_size, "Deployed on");
        if ((keys[count] = APP_MEM_STRDUP(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((values[count] = APP_MEM_STRDUP(tmp_buf)) == NULL) {
            return false;
        }
        count += 1;
    }

#ifdef SCREEN_SIZE_WALLET
    if (contract_idx != -1) {
        if (APP_MEM_CALLOC((void **) &extensions, sizeof(*extensions) * count) == false) {
            return false;
        }
        // The extension array is only `count` long: nbInfos must match before any
        // fallible operation, otherwise an error-path cleanup would index past it
        infos->nbInfos = count;
        infos->infoExtensions = extensions;
        infos->withExtensions = true;

        if (!getEthDisplayableAddress((uint8_t *) get_contract_addr(get_current_tx_info()),
                                      tmp_buf,
                                      tmp_buf_size,
                                      g_chain_config->chain_id)) {
            return false;
        }
        if ((extensions[contract_idx].title = APP_MEM_STRDUP(tmp_buf)) == NULL) {
            return false;
        }
        // Etherscan only for mainnet
        if (get_tx_chain_id() == ETHEREUM_MAINNET_CHAINID) {
            if ((extensions[contract_idx].explanation =
                     APP_MEM_STRDUP("Scan to view on Etherscan")) == NULL) {
                return false;
            }
            snprintf(tmp_buf,
                     tmp_buf_size,
                     "https://etherscan.io/address/%s",
                     extensions[contract_idx].title);
        } else {
            snprintf(tmp_buf, tmp_buf_size, "%s", extensions[contract_idx].title);
        }
        if ((extensions[contract_idx].fullValue = APP_MEM_STRDUP(tmp_buf)) == NULL) {
            return false;
        }
        extensions[contract_idx].aliasType = QR_CODE_ALIAS;
    }
#endif

    infos->nbInfos = count;
    return true;
}

void ui_gcs_cleanup(void) {
    if ((g_pairsList != NULL) && (g_pairsList->pairs != NULL)) {
        for (int i = 0; i < g_pairsList->nbPairs; ++i) {
            free_pair(g_pairsList, i);
        }
        APP_MEM_FREE_AND_NULL((void *) &index_allocated);
    }
    ui_all_cleanup();
    proxy_cleanup();
}

static nbgl_contentValueExt_t *get_infolist_extension(const char *title,
                                                      size_t count,
                                                      const char **keys,
                                                      const char **values) {
    nbgl_contentValueExt_t *ext;
    nbgl_contentInfoList_t *list;
    char **tmp;

    if (APP_MEM_CALLOC((void **) &ext, sizeof(*ext)) == false) {
        return NULL;
    }

    if ((ext->backText = APP_MEM_STRDUP(title)) == NULL) {
        free_pair_extension(ext);
        return NULL;
    }
    ext->aliasType = INFO_LIST_ALIAS;

    if (APP_MEM_CALLOC((void **) &list, sizeof(*list)) == false) {
        free_pair_extension(ext);
        return NULL;
    }
    ext->infolist = list;
    list->nbInfos = count;

    if (APP_MEM_CALLOC((void **) &tmp, sizeof(*tmp) * count) == false) {
        free_pair_extension(ext);
        return NULL;
    }
    list->infoTypes = (const char **) tmp;

    for (int idx = 0; (size_t) idx < count; ++idx) {
        if ((tmp[idx] = APP_MEM_STRDUP(keys[idx])) == NULL) {
            free_pair_extension(ext);
            return NULL;
        }
    }

    if (APP_MEM_CALLOC((void **) &tmp, sizeof(*tmp) * count) == false) {
        free_pair_extension(ext);
        return NULL;
    }
    list->infoContents = (const char **) tmp;

    for (int idx = 0; (size_t) idx < count; ++idx) {
        if ((tmp[idx] = APP_MEM_STRDUP(PIC(values[idx]))) == NULL) {
            free_pair_extension(ext);
            return NULL;
        }
    }
    return ext;
}

static const nbgl_contentValueExt_t *handle_extra_data_trusted_name(
    const s_field_table_entry *field) {
    nbgl_contentValueAliasType_t alias_type;
    nbgl_contentValueExt_t *extension;
    const s_trusted_name *tname = (s_trusted_name *) field->extra_data;
    char formatted_addr[ADDRESS_LENGTH_HEX_STR];

    switch (tname->name_source) {
        case TN_SOURCE_ENS:
            alias_type = ENS_ALIAS;
            break;
        case TN_SOURCE_LAB:
            alias_type = ADDRESS_BOOK_ALIAS;
            break;
        default:
            alias_type = INFO_LIST_ALIAS;
            break;
    }
    if (!getEthDisplayableAddress(tname->addr,
                                  formatted_addr,
                                  sizeof(formatted_addr),
                                  g_chain_config->chain_id)) {
        return NULL;
    }
    if (alias_type == INFO_LIST_ALIAS) {
        const char *keys[] = {"Contract address"};
        const char *values[] = {formatted_addr};
        if ((extension = get_infolist_extension(tname->name, ARRAYLEN(keys), keys, values)) ==
            NULL) {
            return NULL;
        }
    } else {
        if (APP_MEM_CALLOC((void **) &extension, sizeof(*extension)) == false) {
            return NULL;
        }
        if ((extension->fullValue = APP_MEM_STRDUP(formatted_addr)) == NULL) {
            APP_MEM_FREE(extension);
            return NULL;
        }
        extension->title = tname->name;
        extension->aliasType = alias_type;
    }
    return extension;
}

#ifdef HAVE_ADDRESS_BOOK
/**
 * @brief Build the alias extension for an Address Book field.
 *
 * Populates an ADDRESS_BOOK_ALIAS extension so that tapping the field opens a
 * detail view showing the contact name, optional scope, and the full hex address.
 * If a Trusted Name also exists for the same address, it is stored in
 * @c explanation so the detail view can display both names simultaneously.
 *
 * @param[in] field  Field table entry whose extra_data points to an s_ab_contact
 * @return Allocated extension, or NULL on allocation / formatting failure
 */
static const nbgl_contentValueExt_t *handle_extra_data_address_book(
    const s_field_table_entry *field) {
    nbgl_contentValueExt_t *extension;
    const s_ab_contact *ab_contact = (const s_ab_contact *) field->extra_data;
    char formatted_addr[ADDRESS_LENGTH_HEX_STR];

    if (!getEthDisplayableAddress((uint8_t *) ab_contact->identifier,
                                  formatted_addr,
                                  sizeof(formatted_addr),
                                  g_chain_config->chain_id)) {
        return NULL;
    }
    if (APP_MEM_CALLOC((void **) &extension, sizeof(*extension)) == false) {
        return NULL;
    }
    if ((extension->fullValue = APP_MEM_STRDUP(formatted_addr)) == NULL) {
        APP_MEM_FREE(extension);
        return NULL;
    }
    extension->title = ab_contact->contact_name;
    extension->aliasType = ADDRESS_BOOK_ALIAS;
    // NULL suppresses the scope line in the detail view; an empty string would still render it
    extension->aliasSubName = (ab_contact->scope[0] != '\0') ? ab_contact->scope : NULL;

    // If a Trusted Name also exists for this address, store it in explanation so the
    // detail view can show both the Address Book name and the Trusted Name together.
    {
        uint64_t chain_id = get_tx_chain_id();
        e_name_type types[] = {TN_TYPE_ACCOUNT};
        e_name_source sources[] = {TN_SOURCE_ENS, TN_SOURCE_LAB, TN_SOURCE_MAB};
        const s_trusted_name *tname = get_trusted_name(ARRAYLEN(types),
                                                       types,
                                                       ARRAYLEN(sources),
                                                       sources,
                                                       &chain_id,
                                                       ab_contact->identifier);
        if (tname != NULL) {
            extension->explanation = tname->name;
        }
    }
    return extension;
}
#endif  // HAVE_ADDRESS_BOOK

static const nbgl_contentValueExt_t *handle_extra_data_token(const s_field_table_entry *field) {
    const s_token_info *token_info = field->extra_data;
    char formatted_addr[ADDRESS_LENGTH_HEX_STR];
    const char *keys[] = {"Contract address"};
    const char *values[] = {formatted_addr};

    if (!getEthDisplayableAddress(token_info->address,
                                  formatted_addr,
                                  sizeof(formatted_addr),
                                  token_info->chain_id)) {
        return NULL;
    }
    return get_infolist_extension(token_info->ticker, ARRAYLEN(keys), keys, values);
}

static const nbgl_contentValueExt_t *handle_extra_data_nft(const s_field_table_entry *field) {
    const s_nft_info *nft_info = (s_nft_info *) field->extra_data;
    char formatted_addr[ADDRESS_LENGTH_HEX_STR];
    const char *keys[] = {"Contract address"};
    const char *values[] = {formatted_addr};

    if (!getEthDisplayableAddress(nft_info->address,
                                  formatted_addr,
                                  sizeof(formatted_addr),
                                  nft_info->chain_id)) {
        return NULL;
    }
    return get_infolist_extension(nft_info->collection_name, ARRAYLEN(keys), keys, values);
}

static const nbgl_contentValueExt_t *handle_extra_data_enum(const s_field_table_entry *field) {
    const s_enum_value_entry *enum_value = (s_enum_value_entry *) field->extra_data;
    char formatted_value[4];  // max value : 255 + '\0'
    const char *keys[] = {"Raw value"};
    const char *values[] = {formatted_value};

    if (snprintf(formatted_value, sizeof(formatted_value), "%u", enum_value->value) <= 0) {
        return NULL;
    }
    return get_infolist_extension(enum_value->name, ARRAYLEN(keys), keys, values);
}

static bool handle_extra_data(const s_field_table_entry *field, nbgl_contentTagValue_t *pair) {
    pair->aliasValue = true;
    switch (field->type) {
        case PARAM_TYPE_TRUSTED_NAME:
            if ((pair->extension = handle_extra_data_trusted_name(field)) == NULL) {
                return false;
            }
            break;
#ifdef HAVE_ADDRESS_BOOK
        case PARAM_TYPE_ADDRESS_BOOK:
            if ((pair->extension = handle_extra_data_address_book(field)) == NULL) {
                return false;
            }
            break;
#endif  // HAVE_ADDRESS_BOOK
        case PARAM_TYPE_TOKEN_AMOUNT:
        case PARAM_TYPE_TOKEN:
            if ((pair->extension = handle_extra_data_token(field)) == NULL) {
                return false;
            }
            break;
        case PARAM_TYPE_NFT:
            if ((pair->extension = handle_extra_data_nft(field)) == NULL) {
                return false;
            }
            break;
        case PARAM_TYPE_ENUM:
            if ((pair->extension = handle_extra_data_enum(field)) == NULL) {
                return false;
            }
            break;
        default:
            PRINTF("Warning: Unsupported extra data for field of type %u\n", field->type);
            pair->aliasValue = false;
            break;
    }
    return true;
}

bool ui_gcs(void) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    const s_field_table_entry *field;
    bool show_network;
    bool show_root_value;
    bool show_root_from;
    bool show_nonce;
    bool show_hash;
    nbgl_contentValueExt_t *ext = NULL;
    nbgl_contentInfoList_t *infolist = NULL;
    size_t nbPairs = 0;
    size_t pair = 0;
    size_t table_size = 0;
    uint8_t tx_idx = 0;
    bool had_separator = false;
    const s_tx_info *info_tx = get_current_tx_info();

    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning();
#endif

    snprintf(tmp_buf, tmp_buf_size, "Review transaction to %s", get_operation_type(info_tx));
    if ((g_titleMsg = APP_MEM_STRDUP(tmp_buf)) == NULL) {
        return false;
    }
#ifdef SCREEN_SIZE_WALLET
    snprintf(tmp_buf,
             tmp_buf_size,
             "%s transaction to %s?",
             ui_tx_simulation_finish_str(),
             get_operation_type(info_tx));
#else
    snprintf(tmp_buf, tmp_buf_size, "%s transaction", ui_tx_simulation_finish_str());
#endif
    if ((g_finishMsg = APP_MEM_STRDUP(tmp_buf)) == NULL) {
        return false;
    }

    // Get the number of TX fields to display
    table_size = field_table_size();
    // Sender account, unless a descriptor field already shows it (CP_FROM)
    show_root_from = !gcs_is_root_from_shown();
    if (show_root_from) {
        nbPairs += 1;
    }
    // Contract info
    nbPairs += 1;
    // Root transaction native value, unless a descriptor field already shows it (CP_VALUE)
    show_root_value = !gcs_is_root_value_shown() && !allzeroes(tmpContent.txContent.value.value,
                                                               tmpContent.txContent.value.length);
    if (show_root_value) {
        nbPairs += 1;
    }
    // Nonce and transaction hash: both are covered by the signature, so they follow
    // the same settings as the standard review rather than being unconditionally
    // dropped from the clear-signing one.
    show_nonce = N_storage.displayNonce;
    if (show_nonce) {
        nbPairs += 1;
    }
    show_hash = N_storage.displayHash;
    if (show_hash) {
        nbPairs += 1;
    }
    // Batch transactions
    if (txContext.batch_nb_tx > 1) {
        nbPairs += txContext.batch_nb_tx;  // one page per sub-tx
    }
    // TX fields
    nbPairs += table_size;
    show_network = get_tx_chain_id() != g_chain_config->chain_id;
    if (show_network) {
        nbPairs += 1;
    }
    // Fees
    nbPairs += 1;

    if (!ui_pairs_init(nbPairs)) {
        return false;
    }

    // Allocate a table to hold all pairs that will be allocated for UI, and need to be freed later
    if (APP_MEM_CALLOC((void **) &index_allocated, nbPairs) == false) {
        return false;
    }

    // First pair: contract info
    if ((g_pairs[pair].item = APP_MEM_STRDUP("Interaction with")) == NULL) {
        return false;
    }
    g_pairs[pair].value = get_creator_name(info_tx);
    if (g_pairs[pair].value == NULL) {
        // not great, but this cannot be NULL
        g_pairs[pair].value = APP_MEM_STRDUP("a smart contract");
    } else {
        g_pairs[pair].value = APP_MEM_STRDUP(g_pairs[pair].value);
    }
    if (g_pairs[pair].value == NULL) {
        return false;
    }
    index_allocated[pair] = true;
    if (APP_MEM_CALLOC((void **) &ext, sizeof(*ext)) == false) {
        return false;
    }
    g_pairs[pair].extension = ext;

    if (APP_MEM_CALLOC((void **) &infolist, sizeof(*infolist)) == false) {
        return false;
    }
    ext->infolist = infolist;

    if (!prepare_infos(infolist)) {
        return false;
    }
    ext->aliasType = INFO_LIST_ALIAS;
    if ((ext->backText = get_creator_name(info_tx)) == NULL) {
        ext->backText = APP_MEM_STRDUP("Smart contract information");
    } else {
        ext->backText = APP_MEM_STRDUP(ext->backText);
    }
    if (ext->backText == NULL) {
        return false;
    }
    g_pairs[pair].aliasValue = true;
    pair++;

    // Sender account, right after the clickable contract info — the BIP32 path
    // comes from the host, so the user must be able to verify which account signs.
    // Format from the tx context (strings.common.fromAddress shares its memory
    // with strings.tmp.tmp, already clobbered by the title above).
    if (show_root_from) {
        const uint8_t *from = get_current_tx_from();

        if ((from == NULL) ||
            !getEthDisplayableAddress(from, tmp_buf, tmp_buf_size, g_chain_config->chain_id)) {
            PRINTF("Error: no sender address!\n");
            return false;
        }
        if (((g_pairs[pair].item = APP_MEM_STRDUP("From")) == NULL) ||
            ((g_pairs[pair].value = APP_MEM_STRDUP(tmp_buf)) == NULL)) {
            return false;
        }
        index_allocated[pair] = true;
        pair++;
    }

    // TX fields
    for (size_t i = 0; i < table_size; ++i) {
        if ((field = get_from_field_table(i)) == NULL) {
            return false;
        }
        bool add_intermediate = (field->start_intent) && (txContext.batch_nb_tx > 1);
        size_t needed = add_intermediate ? 2 : 1;
        if (pair + needed > g_pairsList->nbPairs) {
            return false;
        }
        if (add_intermediate) {
            // Batch intermediate page
            tx_idx++;
            snprintf(tmp_buf, tmp_buf_size, "%d of %d", tx_idx, txContext.batch_nb_tx);
            if (((g_pairs[pair].item = APP_MEM_STRDUP("Review transaction")) == NULL) ||
                ((g_pairs[pair].value = APP_MEM_STRDUP(tmp_buf)) == NULL)) {
                return false;
            }
            index_allocated[pair] = true;
            g_pairs[pair].centeredInfo = true;
            pair++;
        }
        if (field->is_separator) {
            had_separator = true;
            g_pairs[pair].item = field->key;
            g_pairs[pair].value = field->value;
            g_pairs[pair].centeredInfo = true;
            pair++;
            continue;
        }
        g_pairs[pair].item = field->key;
        g_pairs[pair].value = field->value;

        if (field->extra_data != NULL) {
            if (!handle_extra_data(field, &g_pairs[pair])) {
                return false;
            }
        }
        // Switch to the next pair
        pair++;
        if ((field->end_intent) && (txContext.batch_nb_tx > 1) && (pair < g_pairsList->nbPairs)) {
            // End of batch transaction : start next info on full page
            g_pairs[pair].forcePageStart = true;
        }
    }

    // Native value of the root transaction, taken from the RLP content (device-side
    // source of truth) so the host cannot hide it by omitting it from the metadata.
    if (show_root_value) {
        uint64_t chain_id = get_tx_chain_id();
        const char *ticker = get_displayable_ticker(&chain_id, g_chain_config, true);

        if (!amountToString(tmpContent.txContent.value.value,
                            tmpContent.txContent.value.length,
                            WEI_TO_ETHER,
                            ticker,
                            tmp_buf,
                            tmp_buf_size)) {
            return false;
        }
        if (((g_pairs[pair].item = APP_MEM_STRDUP("Amount")) == NULL) ||
            ((g_pairs[pair].value = APP_MEM_STRDUP(tmp_buf)) == NULL)) {
            return false;
        }
        index_allocated[pair] = true;
        pair++;
    }

    // Format from the tx context: strings.common.nonce shares its memory with
    // strings.tmp.tmp, already clobbered by the title above.
    if (show_nonce) {
        if (pair >= g_pairsList->nbPairs) {
            PRINTF("Error: No more pairs available for nonce!\n");
            return false;
        }
        if (!nonce_to_string(&tmpContent.txContent.nonce, tmp_buf, tmp_buf_size)) {
            PRINTF("Error: Could not format the nonce!\n");
            return false;
        }
        if (((g_pairs[pair].item = APP_MEM_STRDUP("Nonce")) == NULL) ||
            ((g_pairs[pair].value = APP_MEM_STRDUP(tmp_buf)) == NULL)) {
            return false;
        }
        index_allocated[pair] = true;
        pair++;
    }

    if (had_separator) {
        g_pairs[pair].forcePageStart = true;
    }
    if (show_network) {
        if (pair >= g_pairsList->nbPairs - 1) {
            PRINTF("Error: No more pairs available for network!\n");
            return false;
        }
        if (get_network_as_string(tmp_buf, tmp_buf_size) != true) {
            return false;
        }
        if (((g_pairs[pair].item = APP_MEM_STRDUP("Network")) == NULL) ||
            ((g_pairs[pair].value = APP_MEM_STRDUP(tmp_buf)) == NULL)) {
            return false;
        }
        index_allocated[pair] = true;
        pair++;
    }

    // Last pair : fees
    if (pair >= g_pairsList->nbPairs) {
        PRINTF("Error: No more pairs available for fees!\n");
        return false;
    }
    if (max_transaction_fee_to_string(&tmpContent.txContent.gasprice,
                                      &tmpContent.txContent.startgas,
                                      tmp_buf,
                                      tmp_buf_size) == false) {
        PRINTF("Error: Could not format the max fees!\n");
        return false;
    }
    if (((g_pairs[pair].item = APP_MEM_STRDUP("Max fees")) == NULL) ||
        ((g_pairs[pair].value = APP_MEM_STRDUP(tmp_buf)) == NULL)) {
        return false;
    }
    index_allocated[pair] = true;
    pair++;

    if (show_hash) {
        if (pair >= g_pairsList->nbPairs) {
            PRINTF("Error: No more pairs available for tx hash!\n");
            return false;
        }
        strlcpy(tmp_buf, "0x", tmp_buf_size);
        if (bytes_to_lowercase_hex(tmp_buf + 2,
                                   tmp_buf_size - 2,
                                   tmpCtx.transactionContext.hash,
                                   INT256_LENGTH) < 0) {
            PRINTF("Error: Could not format the tx hash!\n");
            return false;
        }
#ifdef SCREEN_SIZE_WALLET
        g_pairs[pair].item = APP_MEM_STRDUP("Transaction hash");
#else
        g_pairs[pair].item = APP_MEM_STRDUP("Tx hash");
#endif
        if ((g_pairs[pair].item == NULL) ||
            ((g_pairs[pair].value = APP_MEM_STRDUP(tmp_buf)) == NULL)) {
            return false;
        }
        index_allocated[pair] = true;
        pair++;
    }

    // Correct the pair count: empty separators are in the field table but
    // do not consume a pair slot, so nbPairs may over-estimate.
    g_pairsList->nbPairs = pair;

    nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                               g_pairsList,
                               get_tx_icon(false),
                               g_titleMsg,
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               review_choice);
    return true;
}
