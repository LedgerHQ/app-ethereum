#include <string.h>
#include "ui_nbgl.h"
#include "gtp_tx_info.h"
#include "gtp_field_table.h"
#include "mem.h"
#include "mem_utils.h"
#include "network.h"
#include "ui_callbacks.h"
#include "feature_signTx.h"
#include "apdu_constants.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"
#include "enum_value.h"
#include "proxy_info.h"
#include "trusted_name.h"
#include "tx_ctx.h"

#define ADDRESS_STRING_LENGTH (2 + (2 * ADDRESS_LENGTH) + 1)

static bool *index_allocated = NULL;

static void review_choice(bool confirm) {
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
    app_mem_free((void *) infolist->infoTypes[idx]);
    app_mem_free((void *) infolist->infoContents[idx]);
    if (infolist->infoExtensions != NULL) {
        app_mem_free((void *) infolist->infoExtensions[idx].title);
        app_mem_free((void *) infolist->infoExtensions[idx].explanation);
        app_mem_free((void *) infolist->infoExtensions[idx].fullValue);
    }
}

static void free_pair_extension(const nbgl_contentValueExt_t *ext) {
    app_mem_free((void *) ext->backText);
    app_mem_free((void *) ext->fullValue);
    if (ext->infolist != NULL) {
        for (int i = 0; i < ext->infolist->nbInfos; ++i) {
            free_pair_extension_infolist_elem(ext->infolist, i);
        }
        app_mem_free((void *) ext->infolist->infoTypes);
        app_mem_free((void *) ext->infolist->infoContents);
        app_mem_free((void *) ext->infolist->infoExtensions);
        app_mem_free((void *) ext->infolist);
    }
    app_mem_free((void *) ext);
}

static void free_pair(const nbgl_contentTagValueList_t *pair_list, int idx) {
    // Only a few pairs are created from the UI, and need to be freed :
    // - the first one, that leads to the contract infos
    // - the second to last one, that shows the Network (optional)
    // - the last one, that shows the TX fees
    if ((index_allocated != NULL) && (index_allocated[idx] == true)) {
        app_mem_free((void *) pair_list->pairs[idx].item);
        app_mem_free((void *) pair_list->pairs[idx].value);
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
    if ((keys = app_mem_alloc(sizeof(*keys) * MAX_INFO_COUNT)) == NULL) return false;
    explicit_bzero(keys, sizeof(*keys) * MAX_INFO_COUNT);
    infos->infoTypes = keys;

    if ((values = app_mem_alloc(sizeof(*values) * MAX_INFO_COUNT)) == NULL) return false;
    explicit_bzero(keys, sizeof(*values) * MAX_INFO_COUNT);
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
        if ((keys[count] = app_mem_strdup(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((value = get_creator_url(get_current_tx_info())) != NULL) {
            off = strlen(tmp_buf);
            snprintf(tmp_buf + off, tmp_buf_size - off, "\n%s", value);
        }
        if ((values[count] = app_mem_strdup(tmp_buf)) == NULL) {
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
        if ((keys[count] = app_mem_strdup(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((values[count] = app_mem_strdup(tmp_buf)) == NULL) {
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
                                  chainConfig->chainId)) {
        return false;
    }
    if ((keys[count] = app_mem_strdup("Contract address")) == NULL) {
        return false;
    }
    if ((values[count] = app_mem_strdup(tmp_buf)) == NULL) {
        return false;
    }
    count += 1;
#endif

    if ((value = get_deploy_date(get_current_tx_info())) != NULL) {
        snprintf(tmp_buf, tmp_buf_size, "Deployed on");
        if ((keys[count] = app_mem_strdup(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((values[count] = app_mem_strdup(tmp_buf)) == NULL) {
            return false;
        }
        count += 1;
    }

#ifdef SCREEN_SIZE_WALLET
    if (contract_idx != -1) {
        if ((extensions = app_mem_alloc(sizeof(*extensions) * count)) == NULL) {
            return false;
        }
        explicit_bzero(extensions, sizeof(*extensions) * count);
        infos->infoExtensions = extensions;
        infos->withExtensions = true;

        if (!getEthDisplayableAddress((uint8_t *) get_contract_addr(get_current_tx_info()),
                                      tmp_buf,
                                      tmp_buf_size,
                                      chainConfig->chainId)) {
            return false;
        }
        if ((extensions[contract_idx].title = app_mem_strdup(tmp_buf)) == NULL) {
            return false;
        }
        // Etherscan only for mainnet
        if (get_tx_chain_id() == ETHEREUM_MAINNET_CHAINID) {
            if ((extensions[contract_idx].explanation =
                     app_mem_strdup("Scan to view on Etherscan")) == NULL) {
                return false;
            }
            snprintf(tmp_buf,
                     tmp_buf_size,
                     "https://etherscan.io/address/%s",
                     extensions[contract_idx].title);
        } else {
            snprintf(tmp_buf, tmp_buf_size, "%s", extensions[contract_idx].title);
        }
        if ((extensions[contract_idx].fullValue = app_mem_strdup(tmp_buf)) == NULL) {
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
        app_mem_free((void *) index_allocated);
        index_allocated = NULL;
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

    if ((ext = app_mem_alloc(sizeof(*ext))) == NULL) {
        return NULL;
    }
    explicit_bzero(ext, sizeof(*ext));

    if ((ext->backText = app_mem_strdup(title)) == NULL) {
        free_pair_extension(ext);
        return NULL;
    }
    ext->aliasType = INFO_LIST_ALIAS;

    if ((list = app_mem_alloc(sizeof(*list))) == NULL) {
        free_pair_extension(ext);
        return NULL;
    }
    explicit_bzero(list, sizeof(*list));
    ext->infolist = list;
    list->nbInfos = count;

    if ((tmp = app_mem_alloc(sizeof(*tmp) * count)) == NULL) {
        free_pair_extension(ext);
        return NULL;
    }
    explicit_bzero(tmp, sizeof(*tmp) * count);
    list->infoTypes = (const char **) tmp;

    for (int idx = 0; (size_t) idx < count; ++idx) {
        if ((tmp[idx] = app_mem_strdup(keys[idx])) == NULL) {
            free_pair_extension(ext);
            return NULL;
        }
    }

    if ((tmp = app_mem_alloc(sizeof(*tmp) * count)) == NULL) {
        free_pair_extension(ext);
        return NULL;
    }
    explicit_bzero(tmp, sizeof(*tmp) * count);
    list->infoContents = (const char **) tmp;

    for (int idx = 0; (size_t) idx < count; ++idx) {
        if ((tmp[idx] = app_mem_strdup(PIC(values[idx]))) == NULL) {
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
    char formatted_addr[ADDRESS_STRING_LENGTH];

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
                                  chainConfig->chainId)) {
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
        if ((extension = app_mem_alloc(sizeof(*extension))) == NULL) {
            return NULL;
        }
        explicit_bzero(extension, sizeof(*extension));
        if ((extension->fullValue = app_mem_strdup(formatted_addr)) == NULL) {
            app_mem_free(extension);
            return NULL;
        }
        extension->title = tname->name;
        extension->aliasType = alias_type;
    }
    return extension;
}

static const nbgl_contentValueExt_t *handle_extra_data_token(const s_field_table_entry *field) {
    const tokenDefinition_t *token_def = (tokenDefinition_t *) field->extra_data;
    char formatted_addr[ADDRESS_STRING_LENGTH];
    const char *keys[] = {"Contract address"};
    const char *values[] = {formatted_addr};

    if (!getEthDisplayableAddress(token_def->address,
                                  formatted_addr,
                                  sizeof(formatted_addr),
                                  chainConfig->chainId)) {
        return NULL;
    }
    return get_infolist_extension(token_def->ticker, ARRAYLEN(keys), keys, values);
}

static const nbgl_contentValueExt_t *handle_extra_data_nft(const s_field_table_entry *field) {
    const nftInfo_t *nft_def = (nftInfo_t *) field->extra_data;
    char formatted_addr[ADDRESS_STRING_LENGTH];
    const char *keys[] = {"Contract address"};
    const char *values[] = {formatted_addr};

    if (!getEthDisplayableAddress(nft_def->contractAddress,
                                  formatted_addr,
                                  sizeof(formatted_addr),
                                  chainConfig->chainId)) {
        return NULL;
    }
    return get_infolist_extension(nft_def->collectionName, ARRAYLEN(keys), keys, values);
}

static const nbgl_contentValueExt_t *handle_extra_data_enum(const s_field_table_entry *field) {
    const s_enum_value_entry *enum_value = (s_enum_value_entry *) field->extra_data;
    char formatted_value[4];  // max value : 255 + '\0'
    const char *keys[] = {"Raw value"};
    const char *values[] = {formatted_value};

    if (snprintf(formatted_value, sizeof(formatted_value), "%u", enum_value->value) <= 0) {
        return false;
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
    nbgl_contentValueExt_t *ext = NULL;
    nbgl_contentInfoList_t *infolist = NULL;
    uint8_t nbPairs = 0;
    uint8_t pair = 0;
    uint8_t tx_idx = 0;
    const s_tx_info *info_tx = get_current_tx_info();

    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning();
#endif

    snprintf(tmp_buf, tmp_buf_size, "Review transaction to %s", get_operation_type(info_tx));
    if ((g_titleMsg = app_mem_strdup(tmp_buf)) == NULL) {
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
    if ((g_finishMsg = app_mem_strdup(tmp_buf)) == NULL) {
        return false;
    }

    // Contract info
    nbPairs += 1;
    // Batch transactions
    if (txContext.batch_nb_tx > 1) {
        nbPairs += txContext.batch_nb_tx;  // one page per sub-tx
    }
    // TX fields
    nbPairs += field_table_size();
    show_network = get_tx_chain_id() != chainConfig->chainId;
    if (show_network) {
        nbPairs += 1;
    }
    // Fees
    nbPairs += 1;

    if (!ui_pairs_init(nbPairs)) {
        return false;
    }

    // Allocate a table to hold all pairs that will be allocated for UI, and need to be freed later
    if (mem_buffer_allocate((void **) &index_allocated, nbPairs) == false) {
        return false;
    }

    // First pair: contract info
    index_allocated[pair] = true;
    g_pairs[pair].item = app_mem_strdup("Interaction with");
    g_pairs[pair].value = get_creator_name(info_tx);
    if (g_pairs[pair].value == NULL) {
        // not great, but this cannot be NULL
        g_pairs[pair].value = app_mem_strdup("a smart contract");
    } else {
        g_pairs[pair].value = app_mem_strdup(g_pairs[pair].value);
    }
    if ((ext = app_mem_alloc(sizeof(*ext))) == NULL) {
        return false;
    }
    explicit_bzero(ext, sizeof(*ext));
    g_pairs[pair].extension = ext;

    if ((infolist = app_mem_alloc(sizeof(*infolist))) == NULL) {
        return false;
    }
    explicit_bzero(infolist, sizeof(*infolist));
    ext->infolist = infolist;

    if (!prepare_infos(infolist)) {
        return false;
    }
    ext->aliasType = INFO_LIST_ALIAS;
    if ((ext->backText = get_creator_name(info_tx)) == NULL) {
        ext->backText = app_mem_strdup("Smart contract information");
    } else {
        ext->backText = app_mem_strdup(ext->backText);
    }
    g_pairs[pair].aliasValue = true;
    pair++;

    // TX fields
    for (int i = 0; i < (int) field_table_size(); ++i) {
        if ((field = get_from_field_table(i)) == NULL) {
            return false;
        }
        if ((field->start_intent) && (txContext.batch_nb_tx > 1)) {
            // Batch intermediate page
            tx_idx++;
            snprintf(tmp_buf, tmp_buf_size, "%d of %d", tx_idx, txContext.batch_nb_tx);
            g_pairs[pair].item = app_mem_strdup("Review transaction");
            g_pairs[pair].value = app_mem_strdup(tmp_buf);
            index_allocated[pair] = true;
            g_pairs[pair].centeredInfo = true;
            pair++;
        }
        g_pairs[pair].item = field->key;
        g_pairs[pair].value = field->value;

        if (field->extra_data != NULL) {
            if (!handle_extra_data(field, &g_pairs[pair])) {
                return false;
            }
        }

        pair++;
        if ((field->end_intent) && (txContext.batch_nb_tx > 1)) {
            // End of batch transaction : start next info on full page
            g_pairs[pair].forcePageStart = true;
        }
    }

    if (show_network) {
        if (pair >= g_pairsList->nbPairs - 1) {
            PRINTF("Error: No more pairs available for network!\n");
            return false;
        }
        g_pairs[pair].item = app_mem_strdup("Network");
        if (get_network_as_string(tmp_buf, tmp_buf_size) != SWO_SUCCESS) {
            return false;
        }
        g_pairs[pair].value = app_mem_strdup(tmp_buf);
        index_allocated[pair] = true;
        pair++;
    }

    // Last pair : fees
    if (pair >= g_pairsList->nbPairs) {
        PRINTF("Error: No more pairs available for fees!\n");
        return false;
    }
    g_pairs[pair].item = app_mem_strdup("Max fees");
    if (max_transaction_fee_to_string(&tmpContent.txContent.gasprice,
                                      &tmpContent.txContent.startgas,
                                      tmp_buf,
                                      tmp_buf_size) == false) {
        PRINTF("Error: Could not format the max fees!\n");
    }
    g_pairs[pair].value = app_mem_strdup(tmp_buf);
    index_allocated[pair] = true;

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
