#include <string.h>
#include "address_name_lookup.h"
#ifdef HAVE_ADDRESS_BOOK
#include "handle_contacts.h"
#endif
#include "common_utils.h"
#include "os_print.h"
#include "os_utils.h"  // is_displayable_string

/**
 * @brief Resolve a human-readable display name for an Ethereum address.
 *
 * Tries name sources in priority order:
 *  1. Address Book (if compiled in and @p types contains TN_TYPE_ACCOUNT)
 *  2. Trusted Name matching the given @p types / @p sources constraints
 *  3. Raw checksummed hex address as fallback
 *
 * @param[in]  addr            Raw 20-byte Ethereum address to resolve
 * @param[in]  chain_id        Chain ID used for both Address Book / Trusted Name lookups and hex
 * formatting
 * @param[in]  type_count      Number of entries in @p types
 * @param[in]  types           Accepted name types (e.g. TN_TYPE_ACCOUNT)
 * @param[in]  source_count    Number of entries in @p sources
 * @param[in]  sources         Accepted name sources (e.g. TN_SOURCE_ENS)
 * @param[out] buf             Output buffer receiving the display string
 * @param[in]  buf_size        Size of @p buf
 * @param[out] name_source_out Which source produced the name; NULL to discard
 * @param[out] extra_data_out  Opaque pointer to the matched record
 *                             (s_ab_contact * or s_trusted_name *), NULL to discard
 * @return true on success, false if address hex formatting failed
 */
bool get_address_display_name(const uint8_t *addr,
                              uint64_t chain_id,
                              uint8_t type_count,
                              const e_name_type *types,
                              uint8_t source_count,
                              const e_name_source *sources,
                              char *buf,
                              size_t buf_size,
                              e_addr_name_source *name_source_out,
                              const void **extra_data_out) {
    const void *extra_data = NULL;
    e_addr_name_source name_source = ADDR_NAME_FROM_RAW;
    bool found = false;
    const s_trusted_name *tname =
        get_trusted_name(type_count, types, source_count, sources, &chain_id, addr);

#ifdef HAVE_ADDRESS_BOOK
    // Address Book contacts are only relevant for EOA / account addresses (TN_TYPE_ACCOUNT).
    // If that type is requested, check the local contact list first; it takes priority over
    // any Trusted Name for the same address.
    for (uint8_t i = 0; i < type_count; i++) {
        if (types[i] == TN_TYPE_ACCOUNT) {
            const s_ab_contact *ab_contact = get_address_book_contact(chain_id, addr);
            // A nameless contact has nothing to put in place of the address, so it is
            // not a match: leave `found` clear and let the fallbacks below run.
            if ((ab_contact != NULL) &&
                is_displayable_string(
                    ab_contact->contact_name,
                    strnlen(ab_contact->contact_name, sizeof(ab_contact->contact_name)))) {
                strlcpy(buf, ab_contact->contact_name, buf_size);
                extra_data = ab_contact;
                name_source = ADDR_NAME_FROM_ADDRESS_BOOK;
                found = true;
            }
            break;
        }
    }
#endif  // HAVE_ADDRESS_BOOK
    // Fall back to a Trusted Name if no Address Book contact matched.
    if (!found) {
        if ((tname != NULL) &&
            is_displayable_string(tname->name, strnlen(tname->name, sizeof(tname->name)))) {
            strlcpy(buf, tname->name, buf_size);
            extra_data = tname;
            name_source = ADDR_NAME_FROM_TRUSTED_NAME;
            found = true;
        }
    }
    // Last resort: format the raw address as a checksummed hex string.
    if (!found) {
        if (!getEthDisplayableAddress(addr, buf, buf_size, chain_id)) {
            return false;
        }
    }
    if (name_source_out != NULL) *name_source_out = name_source;
    if (extra_data_out != NULL) *extra_data_out = extra_data;
    return true;
}
