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

static nbgl_contentTagValueList_t *g_pair_list = NULL;
static char *g_review_title = NULL;
static char *g_sign_title = NULL;

static void review_choice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_tx_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_tx_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_idle);
    }
}

static char *_strdup(const char *src) {
    char *dst;
    size_t length = strlen(src) + 1;

    if ((dst = app_mem_alloc(length)) != NULL) {
        memmove(dst, src, length);
    }
    return dst;
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

#define MAX_INFO_COUNT 3

static bool prepare_infos(nbgl_contentInfoList_t *infos) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    size_t off;
    uint8_t count = 0;
    const char **keys;
    const char **values;
    nbgl_contentValueExt_t *extensions;
    const char *value;
    int contract_idx = -1;

    infos->nbInfos = MAX_INFO_COUNT;
    if ((keys = app_mem_alloc(sizeof(*keys) * MAX_INFO_COUNT)) == NULL) return false;
    explicit_bzero(keys, sizeof(*keys) * MAX_INFO_COUNT);
    infos->infoTypes = keys;

    if ((values = app_mem_alloc(sizeof(*values) * MAX_INFO_COUNT)) == NULL) return false;
    explicit_bzero(keys, sizeof(*values) * MAX_INFO_COUNT);
    infos->infoContents = values;

    if ((value = get_creator_legal_name()) != NULL) {
        snprintf(tmp_buf, tmp_buf_size, "Smart contract owner");
        if ((keys[count] = _strdup(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((value = get_creator_url()) != NULL) {
            off = strlen(tmp_buf);
            snprintf(tmp_buf + off, tmp_buf_size - off, "\n%s", value);
        }
        if ((values[count] = _strdup(tmp_buf)) == NULL) {
            return false;
        }
        count += 1;
    }

    if ((value = get_contract_name()) != NULL) {
        snprintf(tmp_buf, tmp_buf_size, "Smart contract");
        if ((keys[count] = _strdup(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((values[count] = _strdup(tmp_buf)) == NULL) {
            return false;
        }
        contract_idx = count;
        count += 1;
    }

    if ((value = get_deploy_date()) != NULL) {
        snprintf(tmp_buf, tmp_buf_size, "Deployed on");
        if ((keys[count] = _strdup(tmp_buf)) == NULL) {
            return false;
        }
        snprintf(tmp_buf, tmp_buf_size, "%s", value);
        if ((values[count] = _strdup(tmp_buf)) == NULL) {
            return false;
        }
        count += 1;
    }

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
        if ((extensions[contract_idx].title = _strdup(tmp_buf)) == NULL) {
            return false;
        }
        // Etherscan only for mainnet
        if (get_tx_chain_id() == ETHEREUM_MAINNET_CHAINID) {
            if ((extensions[contract_idx].explanation = _strdup("Scan to view on Etherscan")) ==
                NULL) {
                return false;
            }
            snprintf(tmp_buf,
                     tmp_buf_size,
                     "https://etherscan.io/address/%s",
                     extensions[contract_idx].title);
            if ((extensions[contract_idx].fullValue = _strdup(tmp_buf)) == NULL) {
                return false;
            }
        } else {
            extensions[contract_idx].fullValue = extensions[contract_idx].title;
        }
        extensions[contract_idx].aliasType = QR_CODE_ALIAS;
    }

    infos->nbInfos = count;
    return true;
}

void ui_gcs_cleanup(void) {
    if (g_review_title != NULL) {
        app_mem_free(g_review_title);
        g_review_title = NULL;
    }
    if (g_sign_title != NULL) {
        app_mem_free(g_sign_title);
        g_sign_title = NULL;
    }
    if (g_pair_list != NULL) {
        if (g_pair_list->pairs != NULL) {
            for (int i = 0; i < g_pair_list->nbPairs; ++i) {
                free_pair(g_pair_list, i);
            }
            app_mem_free((void *) g_pair_list->pairs);
        }
        app_mem_free(g_pair_list);
        g_pair_list = NULL;
    }
}

bool ui_gcs(void) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    nbgl_contentTagValue_t *pairs = NULL;
    const s_field_table_entry *field;
    bool show_network;
    nbgl_contentValueExt_t *ext = NULL;
    nbgl_contentInfoList_t *infolist = NULL;

    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_WEB3_CHECKS
    set_tx_simulation_warning(&warning, true, true);
#endif

    snprintf(tmp_buf, tmp_buf_size, "Review transaction to %s", get_operation_type());
    if ((g_review_title = _strdup(tmp_buf)) == NULL) {
        ui_gcs_cleanup();
        return false;
    }
    snprintf(tmp_buf,
             tmp_buf_size,
             "%s transaction to %s?",
             ui_tx_simulation_finish_str(),
             get_operation_type());
    if ((g_sign_title = _strdup(tmp_buf)) == NULL) {
        ui_gcs_cleanup();
        return false;
    }

    if ((g_pair_list = app_mem_alloc(sizeof(*g_pair_list))) == NULL) {
        ui_gcs_cleanup();
        return false;
    }
    explicit_bzero(g_pair_list, sizeof(*g_pair_list));

    // Contract info
    g_pair_list->nbPairs += 1;
    // TX fields
    g_pair_list->nbPairs += field_table_size();
    show_network = get_tx_chain_id() != chainConfig->chainId;
    if (show_network) {
        g_pair_list->nbPairs += 1;
    }
    // Fees
    g_pair_list->nbPairs += 1;

    if ((pairs = app_mem_alloc(sizeof(*pairs) * g_pair_list->nbPairs)) == NULL) {
        ui_gcs_cleanup();
        return false;
    }
    explicit_bzero(pairs, sizeof(*pairs) * g_pair_list->nbPairs);
    g_pair_list->pairs = pairs;

    pairs[0].item = _strdup("Interaction with");
    pairs[0].value = get_creator_name();
    if (pairs[0].value == NULL) {
        // not great, but this cannot be NULL
        pairs[0].value = _strdup("a smart contract");
    } else {
        pairs[0].value = _strdup(pairs[0].value);
    }
    if ((ext = app_mem_alloc(sizeof(*ext))) == NULL) {
        ui_gcs_cleanup();
        return false;
    }
    explicit_bzero(ext, sizeof(*ext));
    pairs[0].extension = ext;

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
        ext->backText = _strdup("Smart contract information");
    } else {
        ext->backText = _strdup(ext->backText);
    }
    pairs[0].aliasValue = 1;

    for (int i = 0; i < (int) field_table_size(); ++i) {
        if ((field = get_from_field_table(i)) == NULL) {
            ui_gcs_cleanup();
            return false;
        }
        pairs[1 + i].item = field->key;
        pairs[1 + i].value = field->value;
    }

    if (show_network) {
        pairs[g_pair_list->nbPairs - 2].item = _strdup("Network");
        if (get_network_as_string(tmp_buf, tmp_buf_size) != APDU_RESPONSE_OK) {
            ui_gcs_cleanup();
            return false;
        }
        pairs[g_pair_list->nbPairs - 2].value = _strdup(tmp_buf);
    }

    pairs[g_pair_list->nbPairs - 1].item = _strdup("Max fees");
    if (max_transaction_fee_to_string(&tmpContent.txContent.gasprice,
                                      &tmpContent.txContent.startgas,
                                      tmp_buf,
                                      tmp_buf_size) == false) {
        PRINTF("Error: Could not format the max fees!\n");
    }
    pairs[g_pair_list->nbPairs - 1].value = _strdup(tmp_buf);

    nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                               g_pair_list,
                               get_tx_icon(),
                               g_review_title,
                               NULL,
                               g_sign_title,
                               NULL,
                               &warning,
                               review_choice);
    return true;
}
