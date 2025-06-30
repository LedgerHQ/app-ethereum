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
    if (infolist->infoTypes[idx] != NULL) {
        app_mem_free((void *) infolist->infoTypes[idx]);
    }
    if (infolist->infoContents[idx] != NULL) {
        app_mem_free((void *) infolist->infoContents[idx]);
    }
    if (infolist->infoExtensions != NULL) {
        if (infolist->infoExtensions[idx].title != NULL) {
            app_mem_free((void *) infolist->infoExtensions[idx].title);
        }
        if (infolist->infoExtensions[idx].explanation != NULL) {
            app_mem_free((void *) infolist->infoExtensions[idx].explanation);
        }
        if (infolist->infoExtensions[idx].fullValue != NULL) {
            app_mem_free((void *) infolist->infoExtensions[idx].fullValue);
        }
    }
}

static void free_pair_extension(const nbgl_contentValueExt_t *ext) {
    if (ext->backText != NULL) {
        app_mem_free((void *) ext->backText);
    }
    if (ext->infolist != NULL) {
        for (int i = 0; i < ext->infolist->nbInfos; ++i) {
            free_pair_extension_infolist_elem(ext->infolist, i);
        }
        if (ext->infolist->infoTypes != NULL) {
            app_mem_free((void *) ext->infolist->infoTypes);
        }
        if (ext->infolist->infoContents != NULL) {
            app_mem_free((void *) ext->infolist->infoContents);
        }

        if (ext->infolist->infoExtensions != NULL) {
            app_mem_free((void *) ext->infolist->infoExtensions);
        }
        app_mem_free((void *) ext->infolist);
    }
    app_mem_free((void *) ext);
}

static void free_pair(const nbgl_contentTagValueList_t *pair_list, int idx) {
    // Only a few pairs are created from the UI, and need to be freed :
    // - the first one, that leads to the contract infos
    // - the second to last one, that shows the Network (optional)
    // - the last one, that shows the TX fees
    if ((idx == 0) || (idx == (pair_list->nbPairs - 1)) ||
        ((get_tx_chain_id() != chainConfig->chainId) && (idx == (pair_list->nbPairs - 2)))) {
        if (pair_list->pairs[idx].item != NULL) app_mem_free((void *) pair_list->pairs[idx].item);
        if (pair_list->pairs[idx].value != NULL) app_mem_free((void *) pair_list->pairs[idx].value);
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

    if ((value = get_creator_legal_name()) != NULL) {
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
        if ((value = get_creator_url()) != NULL) {
            off = strlen(tmp_buf);
            snprintf(tmp_buf + off, tmp_buf_size - off, "\n%s", value);
        }
        if ((values[count] = app_mem_strdup(tmp_buf)) == NULL) {
            return false;
        }
        count += 1;
    }

    if ((value = get_contract_name()) != NULL) {
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
    if (!getEthDisplayableAddress((uint8_t *) get_contract_addr(),
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

    if ((value = get_deploy_date()) != NULL) {
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

        if (!getEthDisplayableAddress((uint8_t *) get_contract_addr(),
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
            if ((extensions[contract_idx].fullValue = app_mem_strdup(tmp_buf)) == NULL) {
                return false;
            }
        } else {
            extensions[contract_idx].fullValue = extensions[contract_idx].title;
        }
        extensions[contract_idx].aliasType = QR_CODE_ALIAS;
    }
#endif

    infos->nbInfos = count;
    return true;
}

void ui_gcs_cleanup(void) {
    mem_buffer_cleanup((void **) &g_trusted_name);
    mem_buffer_cleanup((void **) &g_trusted_name_info);
    if ((g_pairsList != NULL) && (g_pairsList->pairs != NULL)) {
        for (int i = 0; i < g_pairsList->nbPairs; ++i) {
            free_pair(g_pairsList, i);
        }
    }
    ui_all_cleanup();
    enum_value_cleanup();
    proxy_cleanup();
}

bool ui_gcs(void) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    const s_field_table_entry *field;
    bool show_network;
    nbgl_contentValueExt_t *ext = NULL;
    nbgl_contentInfoList_t *infolist = NULL;
    uint8_t nbPairs = 0;

    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning(&warning, true, true);
#endif

    snprintf(tmp_buf, tmp_buf_size, "Review transaction to %s", get_operation_type());
    if ((g_titleMsg = app_mem_strdup(tmp_buf)) == NULL) {
        ui_gcs_cleanup();
        return false;
    }
#ifdef SCREEN_SIZE_WALLET
    snprintf(tmp_buf,
             tmp_buf_size,
             "%s transaction to %s?",
             ui_tx_simulation_finish_str(),
             get_operation_type());
#else
    snprintf(tmp_buf, tmp_buf_size, "%s transaction", ui_tx_simulation_finish_str());
#endif
    if ((g_finishMsg = app_mem_strdup(tmp_buf)) == NULL) {
        ui_gcs_cleanup();
        return false;
    }

    // Contract info
    nbPairs += 1;
    // TX fields
    nbPairs += field_table_size();
    show_network = get_tx_chain_id() != chainConfig->chainId;
    if (show_network) {
        nbPairs += 1;
    }
    // Fees
    nbPairs += 1;

    if (!ui_pairs_init(nbPairs)) {
        ui_gcs_cleanup();
        return false;
    }

    g_pairs[0].item = app_mem_strdup("Interaction with");
    g_pairs[0].value = get_creator_name();
    if (g_pairs[0].value == NULL) {
        // not great, but this cannot be NULL
        g_pairs[0].value = app_mem_strdup("a smart contract");
    } else {
        g_pairs[0].value = app_mem_strdup(g_pairs[0].value);
    }
    if ((ext = app_mem_alloc(sizeof(*ext))) == NULL) {
        ui_gcs_cleanup();
        return false;
    }
    explicit_bzero(ext, sizeof(*ext));
    g_pairs[0].extension = ext;

    if ((infolist = app_mem_alloc(sizeof(*infolist))) == NULL) {
        ui_gcs_cleanup();
        return false;
    }
    explicit_bzero(infolist, sizeof(*infolist));
    ext->infolist = infolist;

    if (!prepare_infos(infolist)) {
        ui_gcs_cleanup();
        return false;
    }
    ext->aliasType = INFO_LIST_ALIAS;
    if ((ext->backText = get_creator_name()) == NULL) {
        ext->backText = app_mem_strdup("Smart contract information");
    } else {
        ext->backText = app_mem_strdup(ext->backText);
    }
    g_pairs[0].aliasValue = 1;

    for (int i = 0; i < (int) field_table_size(); ++i) {
        if ((field = get_from_field_table(i)) == NULL) {
            ui_gcs_cleanup();
            return false;
        }
        g_pairs[1 + i].item = field->key;
        g_pairs[1 + i].value = field->value;
    }

    if (show_network) {
        g_pairs[g_pairsList->nbPairs - 2].item = app_mem_strdup("Network");
        if (get_network_as_string(tmp_buf, tmp_buf_size) != APDU_RESPONSE_OK) {
            ui_gcs_cleanup();
            return false;
        }
        g_pairs[g_pairsList->nbPairs - 2].value = app_mem_strdup(tmp_buf);
    }

    g_pairs[g_pairsList->nbPairs - 1].item = app_mem_strdup("Max fees");
    if (max_transaction_fee_to_string(&tmpContent.txContent.gasprice,
                                      &tmpContent.txContent.startgas,
                                      tmp_buf,
                                      tmp_buf_size) == false) {
        PRINTF("Error: Could not format the max fees!\n");
    }
    g_pairs[g_pairsList->nbPairs - 1].value = app_mem_strdup(tmp_buf);

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
