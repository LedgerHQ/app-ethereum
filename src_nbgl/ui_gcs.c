#ifdef HAVE_GENERIC_TX_PARSER

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

static nbgl_layoutTagValueList_t g_pair_list;
static size_t g_alloc_size;

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

    if ((dst = mem_alloc(length)) != NULL) {
        memmove(dst, src, length);
    }
    return dst;
}

static bool cleanup_on_error(const void *mem_before) {
    mem_dealloc(mem_alloc(0) - mem_before);
    return false;
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

    if (((keys = mem_alloc_and_align(sizeof(*keys) * MAX_INFO_COUNT, __alignof__(*keys))) ==
         NULL) ||
        ((values = mem_alloc_and_align(sizeof(*values) * MAX_INFO_COUNT, __alignof__(*values))) ==
         NULL)) {
        return false;
    }
    if ((value = get_creator_legal_name()) != NULL) {
        snprintf(tmp_buf, tmp_buf_size, "Contract owner");
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
        snprintf(tmp_buf, tmp_buf_size, "Contract");
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

    if ((extensions = mem_alloc_and_align(sizeof(*extensions) * count, __alignof__(*extensions))) ==
        NULL) {
        return false;
    }
    explicit_bzero(extensions, sizeof(*extensions) * count);

    if (contract_idx != -1) {
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
    infos->infoTypes = keys;
    infos->infoContents = values;
    infos->infoExtensions = extensions;
    infos->withExtensions = true;
    return true;
}

bool ui_gcs(void) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    size_t off;
    const char *review_title;
    const char *sign_title;
    nbgl_contentTagValue_t *pairs;
    s_field_table_entry entry;
    bool show_network;
    nbgl_tipBox_t tip_box;
    const void *mem_before = mem_alloc(0);

    snprintf(tmp_buf, tmp_buf_size, "Review transaction to %s", get_operation_type());
    if ((review_title = _strdup(tmp_buf)) == NULL) {
        return cleanup_on_error(mem_before);
    }
    snprintf(tmp_buf, tmp_buf_size, "Sign transaction to %s?", get_operation_type());
    if ((sign_title = _strdup(tmp_buf)) == NULL) {
        return cleanup_on_error(mem_before);
    }
    explicit_bzero(&tip_box, sizeof(tip_box));
    tip_box.icon = &C_review_info_button;
    tip_box.text = NULL;
    tip_box.modalTitle = "Contract information";
    tip_box.type = INFOS_LIST;
    if (!prepare_infos(&tip_box.infos)) {
        return cleanup_on_error(mem_before);
    }
    snprintf(tmp_buf, tmp_buf_size, "Interaction with a\nsmart contract");
    off = strlen(tmp_buf);
    if (get_creator_name()) {
        snprintf(tmp_buf + off, tmp_buf_size - off, " from:\n%s", get_creator_name());
    }
    if ((tip_box.text = _strdup(tmp_buf)) == NULL) {
        return cleanup_on_error(mem_before);
    }

    g_pair_list.nbPairs = field_table_size() + 1;
    show_network = get_tx_chain_id() != chainConfig->chainId;
    if (show_network) {
        g_pair_list.nbPairs += 1;
    }
    if ((pairs = mem_alloc_and_align(sizeof(*pairs) * g_pair_list.nbPairs, __alignof__(*pairs))) ==
        NULL) {
        return cleanup_on_error(mem_before);
    }
    explicit_bzero(pairs, sizeof(*pairs) * g_pair_list.nbPairs);

    for (int i = 0; i < (int) field_table_size(); ++i) {
        if (!get_from_field_table(i, &entry)) {
            return cleanup_on_error(mem_before);
        }
        pairs[i].item = entry.key;
        pairs[i].value = entry.value;
    }

    if (show_network) {
        pairs[g_pair_list.nbPairs - 2].item = _strdup("Network");
        if (get_network_as_string(tmp_buf, tmp_buf_size) != APDU_RESPONSE_OK) {
            return cleanup_on_error(mem_before);
        }
        pairs[g_pair_list.nbPairs - 2].value = _strdup(tmp_buf);
    }

    pairs[g_pair_list.nbPairs - 1].item = _strdup("Max fees");
    if (max_transaction_fee_to_string(&tmpContent.txContent.gasprice,
                                      &tmpContent.txContent.startgas,
                                      tmp_buf,
                                      tmp_buf_size) == false) {
        PRINTF("Error: Could not format the max fees!\n");
    }
    pairs[g_pair_list.nbPairs - 1].value = _strdup(tmp_buf);

    g_pair_list.pairs = pairs;
    nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                               &g_pair_list,
                               get_tx_icon(),
                               review_title,
                               NULL,
                               sign_title,
                               &tip_box,
                               review_choice);
    g_alloc_size = mem_alloc(0) - mem_before;
    return true;
}

void ui_gcs_cleanup(void) {
    mem_dealloc(g_alloc_size);
}

#endif  // HAVE_GENERIC_TX_PARSER
