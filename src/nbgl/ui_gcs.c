#include <string.h>
#include "os_utils.h"
#include "shared_context.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "operation_info.h"
#include "gcs_field_table.h"
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
#include "operation_ctx.h"
#include "token_info.h"
#include "nft_info.h"
#include "common_ui.h"
#include "common_712.h"
#ifdef HAVE_ADDRESS_BOOK
#include "handle_contacts.h"
#endif  // HAVE_ADDRESS_BOOK

static bool *index_allocated = NULL;

// The review of a typed message is on the chain of the message, which is not the one of a
// transaction, and is on none when its domain has none
static bool review_chain_id(uint64_t *chain_id) {
    if (operation_ctx_has_message()) {
        return get_current_operation_chain_id(chain_id);
    }
    *chain_id = get_tx_chain_id();
    return true;
}

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
    // Arrays may be missing when their allocation failed after nbInfos was set
    if (infolist->infoTypes != NULL) {
        APP_MEM_FREE((void *) infolist->infoTypes[idx]);
    }
    if (infolist->infoContents != NULL) {
        APP_MEM_FREE((void *) infolist->infoContents[idx]);
    }
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

/**
 * Fill a pair with copies of its item and value
 *
 * The pair is marked as allocated first, so that the cleanup frees a copy made before one that
 * fails.
 *
 * @param[in] idx index of the pair
 * @param[in] item the item to copy
 * @param[in] value the value to copy
 * @return whether both were copied
 */
static bool set_allocated_pair(size_t idx, const char *item, const char *value) {
    index_allocated[idx] = true;
    return ((g_pairs[idx].item = APP_MEM_STRDUP(item)) != NULL) &&
           ((g_pairs[idx].value = APP_MEM_STRDUP(value)) != NULL);
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
#define MAX_INFO_COUNT      3
#define INFO_CONTRACT_OWNER "Smart contract owner"
#define INFO_CONTRACT       "Smart contract"
#else
#define MAX_INFO_COUNT      4
#define INFO_CONTRACT_OWNER "Contract owner"
#define INFO_CONTRACT       "Contract"
#endif

static bool add_info(const char **keys,
                     const char **values,
                     uint8_t *count,
                     const char *key,
                     const char *value) {
    if (((keys[*count] = APP_MEM_STRDUP(key)) == NULL) ||
        ((values[*count] = APP_MEM_STRDUP(value)) == NULL)) {
        return false;
    }
    *count += 1;
    return true;
}

/**
 * Fill the contract information page of a review
 *
 * @param[out] infos the page
 * @param[in] info the operation info of what is reviewed
 * @param[in] contract_addr the address shown for the contract, NULL if there is none to show
 * @return whether the page was filled
 */
static bool prepare_infos(nbgl_contentInfoList_t *infos,
                          const s_operation_info *info,
                          const uint8_t *contract_addr) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    char addr_str[2 + ADDRESS_LENGTH_HEX + 1];
    uint8_t count = 0;
    const char **keys;
    const char **values;
    const char *value;
#ifdef SCREEN_SIZE_WALLET
    nbgl_contentValueExt_t *extensions;
    int contract_idx = -1;
    uint64_t chain_id;
#endif

    if ((contract_addr != NULL) && !getEthDisplayableAddress((uint8_t *) contract_addr,
                                                             addr_str,
                                                             sizeof(addr_str),
                                                             g_chain_config->chain_id)) {
        return false;
    }

    infos->nbInfos = MAX_INFO_COUNT;
    if (APP_MEM_CALLOC((void **) &keys, sizeof(*keys) * MAX_INFO_COUNT) == false) return false;
    infos->infoTypes = keys;

    if (APP_MEM_CALLOC((void **) &values, sizeof(*values) * MAX_INFO_COUNT) == false) return false;
    infos->infoContents = values;

    if ((value = get_creator_legal_name(info)) != NULL) {
        const char *url = get_creator_url(info);

        if (url != NULL) {
            snprintf(tmp_buf, tmp_buf_size, "%s\n%s", value, url);
            value = tmp_buf;
        }
        if (!add_info(keys, values, &count, INFO_CONTRACT_OWNER, value)) {
            return false;
        }
    }

    value = get_contract_name(info);
#ifdef SCREEN_SIZE_WALLET
    // The contract entry carries the address QR code, and shows the address when unnamed
    if ((value == NULL) && (contract_addr != NULL)) {
        value = addr_str;
    }
    if (value != NULL) {
        contract_idx = count;
        if (!add_info(keys, values, &count, INFO_CONTRACT, value)) {
            return false;
        }
    }
#else
    if (value != NULL) {
        if (!add_info(keys, values, &count, INFO_CONTRACT, value)) {
            return false;
        }
    }
    if ((contract_addr != NULL) && !add_info(keys, values, &count, "Contract address", addr_str)) {
        return false;
    }
#endif

    if ((value = get_deploy_date(info)) != NULL) {
        if (!add_info(keys, values, &count, "Deployed on", value)) {
            return false;
        }
    }

#ifdef SCREEN_SIZE_WALLET
    if ((contract_idx != -1) && (contract_addr != NULL)) {
        if (APP_MEM_CALLOC((void **) &extensions, sizeof(*extensions) * count) == false) {
            return false;
        }
        // The extension array is only `count` long: nbInfos must match before any
        // fallible operation, otherwise an error-path cleanup would index past it
        infos->nbInfos = count;
        infos->infoExtensions = extensions;
        infos->withExtensions = true;

        if ((extensions[contract_idx].title = APP_MEM_STRDUP(addr_str)) == NULL) {
            return false;
        }
        // Etherscan only for mainnet
        value = addr_str;
        if (review_chain_id(&chain_id) && (chain_id == ETHEREUM_MAINNET_CHAINID)) {
            if ((extensions[contract_idx].explanation =
                     APP_MEM_STRDUP("Scan to view on Etherscan")) == NULL) {
                return false;
            }
            snprintf(tmp_buf, tmp_buf_size, "https://etherscan.io/address/%s", addr_str);
            value = tmp_buf;
        }
        if ((extensions[contract_idx].fullValue = APP_MEM_STRDUP(value)) == NULL) {
            return false;
        }
        extensions[contract_idx].aliasType = QR_CODE_ALIAS;
    }
#endif

    infos->nbInfos = count;
    return true;
}

void ui_gcs_cleanup(void) {
    // The pairs are the ones of a GCS review only when it allocated index_allocated: another
    // review fills the same list with pointers it owns, some of them to static strings
    if ((index_allocated != NULL) && (g_pairsList != NULL) && (g_pairsList->pairs != NULL)) {
        for (int i = 0; i < g_pairsList->nbPairs; ++i) {
            free_pair(g_pairsList, i);
        }
    }
    APP_MEM_FREE_AND_NULL((void *) &index_allocated);
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
    // The transaction address, not the trusted name one which can be a proxy implementation
    if (!getEthDisplayableAddress(field->has_addr ? field->addr : tname->addr,
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
        uint64_t chain_id;
        e_name_type types[] = {TN_TYPE_ACCOUNT};
        e_name_source sources[] = {TN_SOURCE_ENS, TN_SOURCE_LAB, TN_SOURCE_MAB};
        const s_trusted_name *tname = NULL;

        // a trusted name is for a chain, which a message whose domain has none does not give
        if (review_chain_id(&chain_id)) {
            tname = get_trusted_name(ARRAYLEN(types),
                                     types,
                                     ARRAYLEN(sources),
                                     sources,
                                     &chain_id,
                                     ab_contact->identifier);
        }
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
#ifdef HAVE_ADDRESS_BOOK
            // displayed as a contact, so cut as one
            if (pair->extension->aliasType == ADDRESS_BOOK_ALIAS) {
                pair->nbMaxLinesForValue = ADDRESS_BOOK_ALIAS_MAX_LINES;
            }
#endif  // HAVE_ADDRESS_BOOK
            break;
#ifdef HAVE_ADDRESS_BOOK
        case PARAM_TYPE_ADDRESS_BOOK:
            if ((pair->extension = handle_extra_data_address_book(field)) == NULL) {
                return false;
            }
            pair->nbMaxLinesForValue = ADDRESS_BOOK_ALIAS_MAX_LINES;
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

/**
 * Whether the page of the information about the contract has anything to list
 *
 * @param[in] info the operation info of what is reviewed
 * @param[in] contract_addr the address shown for the contract, NULL if there is none to show
 */
static bool has_contract_infos(const s_operation_info *info, const uint8_t *contract_addr) {
    return (contract_addr != NULL) || (get_creator_legal_name(info) != NULL) ||
           (get_contract_name(info) != NULL) || (get_deploy_date(info) != NULL);
}

/**
 * Get the name shown on the pair that leads to the information about the contract
 *
 * @param[in] info the operation info of what is reviewed
 * @param[in] contract_addr the address shown for the contract, NULL if there is none to show
 * @return the creator, a generic name when other things are known about the contract, NULL if
 * there is nothing to tell about it
 */
static const char *get_interaction_name(const s_operation_info *info,
                                        const uint8_t *contract_addr) {
    const char *name = get_creator_name(info);

    if ((name == NULL) && has_contract_infos(info, contract_addr)) {
        name = "a smart contract";
    }
    return name;
}

/**
 * Add the pair that leads to the information about the contract
 *
 * Nothing is added when there is nothing to tell about the contract, and the pair does not lead
 * anywhere when the creator is all there is to tell.
 *
 * @param[in,out] pair index of the next pair
 * @param[in] info the operation info of what is reviewed
 * @param[in] contract_addr the address shown for the contract, NULL if there is none to show
 * @return whether the pair was added
 */
static bool add_interaction_pair(size_t *pair,
                                 const s_operation_info *info,
                                 const uint8_t *contract_addr) {
    const char *name = get_interaction_name(info, contract_addr);
    nbgl_contentValueExt_t *ext = NULL;
    nbgl_contentInfoList_t *infolist = NULL;

    if (name == NULL) {
        return true;
    }
    if (!set_allocated_pair(*pair, "Interaction with", name)) {
        return false;
    }
    if (!has_contract_infos(info, contract_addr)) {
        *pair += 1;
        return true;
    }
    if (APP_MEM_CALLOC((void **) &ext, sizeof(*ext)) == false) {
        return false;
    }
    g_pairs[*pair].extension = ext;

    if (APP_MEM_CALLOC((void **) &infolist, sizeof(*infolist)) == false) {
        return false;
    }
    ext->infolist = infolist;

    if (!prepare_infos(infolist, info, contract_addr)) {
        return false;
    }
    ext->aliasType = INFO_LIST_ALIAS;
    if ((ext->backText = get_creator_name(info)) == NULL) {
        ext->backText = APP_MEM_STRDUP("Smart contract information");
    } else {
        ext->backText = APP_MEM_STRDUP(ext->backText);
    }
    if (ext->backText == NULL) {
        return false;
    }
    g_pairs[*pair].aliasValue = true;
    *pair += 1;
    return true;
}

/**
 * Add the pair that shows who signs
 *
 * The BIP32 path comes from the host, so the user must be able to verify which account signs.
 * It is formatted here from the address, since strings.common.fromAddress shares its memory with
 * strings.tmp.tmp, already clobbered by the title.
 *
 * @param[in,out] pair index of the next pair
 * @param[in] from the address of the signer
 * @return whether the pair was added
 */
static bool add_from_pair(size_t *pair, const uint8_t *from) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);

    if ((from == NULL) ||
        !getEthDisplayableAddress(from, tmp_buf, tmp_buf_size, g_chain_config->chain_id)) {
        PRINTF("Error: no sender address!\n");
        return false;
    }
    if (!set_allocated_pair(*pair, "From", tmp_buf)) {
        return false;
    }
    *pair += 1;
    return true;
}

/**
 * Add the pairs of the fields the descriptors gave
 *
 * @param[in,out] pair index of the next pair
 * @param[out] had_separator whether one of the fields was a separator
 * @return whether the pairs were added
 */
static bool add_field_pairs(size_t *pair, bool *had_separator) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    size_t table_size = field_table_size();
    const s_field_table_entry *field;
    uint8_t tx_idx = 0;

    for (size_t i = 0; i < table_size; ++i) {
        if ((field = get_from_field_table(i)) == NULL) {
            return false;
        }
        bool add_intermediate = (field->start_intent) && (txContext.batch_nb_tx > 1);
        size_t needed = add_intermediate ? 2 : 1;
        if (*pair + needed > g_pairsList->nbPairs) {
            return false;
        }
        if (add_intermediate) {
            // Batch intermediate page
            tx_idx++;
            snprintf(tmp_buf, tmp_buf_size, "%d of %d", tx_idx, txContext.batch_nb_tx);
            if (!set_allocated_pair(*pair, "Review transaction", tmp_buf)) {
                return false;
            }
            g_pairs[*pair].centeredInfo = true;
            *pair += 1;
        }
        if (field->is_separator) {
            *had_separator = true;
            g_pairs[*pair].item = field->key;
            g_pairs[*pair].value = field->value;
            g_pairs[*pair].centeredInfo = true;
            *pair += 1;
            continue;
        }
        g_pairs[*pair].item = field->key;
        g_pairs[*pair].value = field->value;

        if (field->extra_data != NULL) {
            if (!handle_extra_data(field, &g_pairs[*pair])) {
                return false;
            }
        }
        // Switch to the next pair
        *pair += 1;
        if ((field->end_intent) && (txContext.batch_nb_tx > 1) && (*pair < g_pairsList->nbPairs)) {
            // End of batch transaction : start next info on full page
            g_pairs[*pair].forcePageStart = true;
        }
    }
    return true;
}

/**
 * Add the pair that shows the network
 *
 * @param[in,out] pair index of the next pair
 * @param[in] chain_id the chain
 * @param[in] reserved number of pairs that have to remain after it
 * @return whether the pair was added
 */
static bool add_network_pair(size_t *pair, uint64_t chain_id, size_t reserved) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);

    if ((*pair + reserved) >= g_pairsList->nbPairs) {
        PRINTF("Error: No more pairs available for network!\n");
        return false;
    }
    if (get_network_as_string_from_chain_id(tmp_buf, tmp_buf_size, chain_id) != true) {
        return false;
    }
    if (!set_allocated_pair(*pair, "Network", tmp_buf)) {
        return false;
    }
    *pair += 1;
    return true;
}

/**
 * Set the title of the review, and the message that ends it
 *
 * @param[in] info the operation info of what is reviewed
 * @param[in] type TYPE_MESSAGE for a typed message, TYPE_TRANSACTION for a transaction
 * @return whether the messages were set
 */
static bool set_review_messages(const s_operation_info *info, nbgl_operationType_t type) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    bool is_message = (type == TYPE_MESSAGE);
    const char *what = is_message ? "typed message" : "transaction";

    snprintf(tmp_buf, tmp_buf_size, "Review %s to %s", what, get_operation_type(info));
    if ((g_titleMsg = APP_MEM_STRDUP(tmp_buf)) == NULL) {
        return false;
    }
#ifdef SCREEN_SIZE_WALLET
    snprintf(tmp_buf,
             tmp_buf_size,
             "%s %s to %s?",
             ui_tx_simulation_finish_str(),
             what,
             get_operation_type(info));
#else
    snprintf(tmp_buf,
             tmp_buf_size,
             "%s %s",
             ui_tx_simulation_finish_str(),
             is_message ? "message" : what);
#endif
    return (g_finishMsg = APP_MEM_STRDUP(tmp_buf)) != NULL;
}

/**
 * Allocate the pairs of a review, and the table of the ones created here, which have to be freed
 * later
 *
 * @param[in] nbPairs the number of pairs
 * @return whether the pairs were allocated
 */
static bool init_pairs(size_t nbPairs) {
    return ui_pairs_init(nbPairs) && APP_MEM_CALLOC((void **) &index_allocated, nbPairs);
}

/**
 * Count the pairs the contract, the signer and the fields give, which every review starts with
 *
 * @param[in] info the operation info of what is reviewed
 * @param[in] contract_addr the address shown for the contract, NULL if there is none to show
 * @param[in] show_from whether the signer is shown, which a descriptor can do already
 * @return the number of pairs
 */
static size_t count_leading_pairs(const s_operation_info *info,
                                  const uint8_t *contract_addr,
                                  bool show_from) {
    size_t count = field_table_size();

    if (get_interaction_name(info, contract_addr) != NULL) {
        count += 1;
    }
    if (show_from) {
        count += 1;
    }
    // Batch transactions: one page per sub-tx
    if (txContext.batch_nb_tx > 1) {
        count += txContext.batch_nb_tx;
    }
    return count;
}

/**
 * Add the pairs that every review starts with: the contract, with its information behind it, the
 * signer and the fields
 *
 * @param[in,out] pair index of the next pair
 * @param[in] info the operation info of what is reviewed
 * @param[in] contract_addr the address shown for the contract, NULL if there is none to show
 * @param[in] show_from whether the signer is shown
 * @param[out] had_separator whether one of the fields was a separator
 * @return whether the pairs were added
 */
static bool add_leading_pairs(size_t *pair,
                              const s_operation_info *info,
                              const uint8_t *contract_addr,
                              bool show_from,
                              bool *had_separator) {
    if (!add_interaction_pair(pair, info, contract_addr)) {
        return false;
    }
    // Sender account, right after the clickable contract info
    if (show_from && !add_from_pair(pair, get_current_operation_from())) {
        return false;
    }
    return add_field_pairs(pair, had_separator);
}

bool ui_gcs(void) {
    char *tmp_buf = strings.tmp.tmp;
    size_t tmp_buf_size = sizeof(strings.tmp.tmp);
    bool show_network;
    bool show_root_value;
    bool show_root_from;
    bool show_nonce;
    bool show_hash;
    size_t nbPairs = 0;
    size_t pair = 0;
    bool had_separator = false;
    const s_operation_info *info_tx = get_current_operation_info();
    // The destination of the transaction rather than the contract of the info, which can be a
    // proxy implementation
    const uint8_t *to = get_current_operation_to();

    if (to == NULL) {
        return false;
    }
    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning();
#endif

    if (!set_review_messages(info_tx, TYPE_TRANSACTION)) {
        return false;
    }

    // Sender account, unless a descriptor field already shows it (CP_FROM)
    show_root_from = !operation_ctx_is_root_from_shown();
    nbPairs += count_leading_pairs(info_tx, to, show_root_from);
    // Root transaction native value, unless a descriptor field already shows it (CP_VALUE)
    show_root_value =
        !operation_ctx_is_root_value_shown() &&
        !allzeroes(tmpContent.txContent.value.value, tmpContent.txContent.value.length);
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
    show_network = get_tx_chain_id() != g_chain_config->chain_id;
    if (show_network) {
        nbPairs += 1;
    }
    // Fees
    nbPairs += 1;

    if (!init_pairs(nbPairs)) {
        return false;
    }

    if (!add_leading_pairs(&pair, info_tx, to, show_root_from, &had_separator)) {
        return false;
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
        if (!set_allocated_pair(pair, "Amount", tmp_buf)) {
            return false;
        }
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
        if (!set_allocated_pair(pair, "Nonce", tmp_buf)) {
            return false;
        }
        pair++;
    }

    if (had_separator) {
        g_pairs[pair].forcePageStart = true;
    }
    if (show_network && !add_network_pair(&pair, get_tx_chain_id(), 1)) {
        return false;
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
    if (!set_allocated_pair(pair, "Max fees", tmp_buf)) {
        return false;
    }
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
        const char *hash_label = "Transaction hash";
#else
        const char *hash_label = "Tx hash";
#endif
        if (!set_allocated_pair(pair, hash_label, tmp_buf)) {
            return false;
        }
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

/**
 * Add a pair that shows a hash of the typed message
 *
 * @param[in,out] pair index of the next pair
 * @param[in] label the item of the pair
 * @param[in] hash_str the formatted hash
 * @return whether the pair was added
 */
static bool add_hash_pair(size_t *pair, const char *label, const char *hash_str) {
    if (!set_allocated_pair(*pair, label, hash_str)) {
        return false;
    }
    *pair += 1;
    return true;
}

/**
 * Add the pairs that show the hashes of the typed message
 *
 * @param[in,out] pair index of the next pair
 * @return whether the pairs were added
 */
static bool add_message_hash_pairs(size_t *pair) {
    // both strings are formatted at once into the temporary buffer
    _Static_assert(EIP712_HASH_ROWS * EIP712_HASH_STR_SIZE <= sizeof(strings.tmp.tmp),
                   "the hashes must fit in the temporary buffer");
    char *domain_str = strings.tmp.tmp;
    char *message_str = domain_str + EIP712_HASH_STR_SIZE;

    if ((*pair + EIP712_HASH_ROWS) > g_pairsList->nbPairs) {
        PRINTF("Error: No more pairs available for the hashes!\n");
        return false;
    }
    eip712_format_hash_strs(domain_str, message_str);
    return add_hash_pair(pair, EIP712_DOMAIN_HASH_LABEL, domain_str) &&
           add_hash_pair(pair, EIP712_MESSAGE_HASH_LABEL, message_str);
}

bool ui_gcs_message(void) {
    const s_operation_info *info = get_current_operation_info();
    const uint8_t *contract_addr;
    uint64_t chain_id = 0;
    bool show_from;
    bool show_network;
    bool show_hash;
    size_t nbPairs;
    size_t pair = 0;
    bool had_separator = false;

    if ((info == NULL) || (info->context != OPERATION_INFO_CTX_EIP712)) {
        return false;
    }
    if (!ui_712_prepare_review()) {
        return false;
    }
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning();
#endif

    if (!set_review_messages(info, TYPE_MESSAGE)) {
        return false;
    }

    // The contract of the domain, when it is an address
    contract_addr = get_current_operation_to();

    // Signer, unless a descriptor field already shows it (CP_FROM)
    show_from = !operation_ctx_is_root_from_shown();
    nbPairs = count_leading_pairs(info, contract_addr, show_from);
    // A message with no chain is not tied to one, so there is no network to show
    show_network =
        get_current_operation_chain_id(&chain_id) && (chain_id != g_chain_config->chain_id);
    if (show_network) {
        nbPairs += 1;
    }
    // The hashes are covered by the signature, so they follow the same setting as the standard
    // review
    show_hash = N_storage.displayHash;
    if (show_hash) {
        nbPairs += EIP712_HASH_ROWS;
    }

    if (!init_pairs(nbPairs)) {
        return false;
    }

    if (!add_leading_pairs(&pair, info, contract_addr, show_from, &had_separator)) {
        return false;
    }
    if (had_separator && (show_network || show_hash) && (pair < g_pairsList->nbPairs)) {
        g_pairs[pair].forcePageStart = true;
    }
    if (show_network && !add_network_pair(&pair, chain_id, show_hash ? EIP712_HASH_ROWS : 0)) {
        return false;
    }
    if (show_hash && !add_message_hash_pairs(&pair)) {
        return false;
    }

    // A review with nothing to show must not be signed, and one with more rows than nbgl can
    // count is refused before it is shown
    if ((pair == 0) || (pair > MAX_REVIEW_ROWS)) {
        return false;
    }
    g_pairsList->nbPairs = pair;

    nbgl_useCaseAdvancedReview(TYPE_MESSAGE,
                               g_pairsList,
                               &LARGE_REVIEW_ICON,
                               g_titleMsg,
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               ui_712_review_choice);
    return true;
}
