#include <string.h>
#include "tlv_library.h"
#include "read.h"
#include "os_print.h"
#include "common_utils.h"
#include "tlv_apdu.h"
#include "mem.h"
#include "mem_utils.h"
#include "ui_utils.h"
#include "buffer.h"
#include "utils.h"
#include "challenge.h"

static uint8_t *g_tlv_payload = NULL;
static uint16_t g_tlv_size = 0;
static uint16_t g_tlv_pos = 0;

static void reset_state(void) {
    mem_buffer_cleanup((void **) &g_tlv_payload);
    g_tlv_size = 0;
    g_tlv_pos = 0;
}

bool tlv_from_apdu(bool first_chunk,
                   uint8_t lc,
                   const uint8_t *payload,
                   f_tlv_payload_handler handler) {
    bool ret = true;
    uint8_t offset = 0;
    uint8_t chunk_length;

    if (first_chunk) {
        if ((offset + sizeof(g_tlv_size)) > lc) {
            return false;
        }
        g_tlv_size = read_u16_be(payload, offset);
        offset += sizeof(g_tlv_size);
        g_tlv_pos = 0;
        if (g_tlv_payload != NULL) {
            PRINTF("Error: remnants from an incomplete TLV payload!\n");
            reset_state();
            return false;
        }

        if (g_tlv_size > (lc - offset)) {
            if ((g_tlv_payload = app_mem_alloc(g_tlv_size)) == NULL) {
                reset_state();
                return false;
            }
        }
    }
    chunk_length = lc - offset;
    if ((g_tlv_pos + chunk_length) > g_tlv_size) {
        PRINTF("TLV payload bigger than expected!\n");
        reset_state();
        return false;
    }

    if (g_tlv_payload != NULL) {
        memcpy(g_tlv_payload + g_tlv_pos, payload + offset, chunk_length);
    }

    g_tlv_pos += chunk_length;

    if (g_tlv_pos == g_tlv_size) {
        // Create buffer_t for the complete payload
        buffer_t buf;
        if (g_tlv_payload != NULL) {
            // Multi-chunk case: use allocated buffer
            buf = (buffer_t){.ptr = g_tlv_payload, .size = g_tlv_size, .offset = 0};
        } else {
            // Single-chunk case: use APDU data directly
            buf = (buffer_t){.ptr = (uint8_t *) &payload[offset], .size = g_tlv_size, .offset = 0};
        }
        ret = (*handler)(&buf);
        reset_state();
    }
    return ret;
}

/**
 * @brief Parse and check the STRUCTURE_TYPE value.
 *
 * @param[in] data to handle
 * @param[in] expected value
 * @return whether the handling was successful
 */
bool tlv_check_struct_type(const tlv_data_t *data, uint8_t expected) {
    uint8_t value = 0;
    if (!get_uint8_t_from_tlv_data(data, &value)) {
        PRINTF("STRUCTURE_TYPE: failed to extract\n");
        return false;
    }
    CHECK_FIELD_VALUE("STRUCTURE_TYPE", value, expected);
    return true;
}

/**
 * @brief Parse and check the STRUCTURE_VERSION value.
 *
 * @param[in] data to handle
 * @param[in] expected value
 * @return whether the handling was successful
 */
bool tlv_check_struct_version(const tlv_data_t *data, uint8_t expected) {
    uint8_t value = 0;
    if (!get_uint8_t_from_tlv_data(data, &value)) {
        PRINTF("STRUCTURE_VERSION: failed to extract\n");
        return false;
    }
    CHECK_FIELD_VALUE("STRUCTURE_VERSION", value, expected);
    return true;
}

/**
 * @brief Parse and check the STRUCTURE_TYPE value.
 *
 * @param[in] data to handle
 * @return whether the handling was successful
 */
bool tlv_check_challenge(const tlv_data_t *data) {
    uint32_t challenge = 0;
    if (!get_uint32_t_from_tlv_data(data, &challenge)) {
        PRINTF("CHALLENGE: failed to extract\n");
        return false;
    }
    return check_challenge(challenge);
}

/**
 * @brief Parse and get the CHAIN_ID value.
 *
 * @param[in] data to handle
 * @param[out] chain_id extracted chain ID
 * @return whether the handling was successful
 *
 * @note The chain ID is expected to be in the range of valid Ethereum chain IDs
 * (1 to 2^48-1, excluding some reserved values).
 * See https://github.com/ethereum/EIPs/blob/master/EIPS/eip-2294.md
 */
bool tlv_get_chain_id(const tlv_data_t *data, uint64_t *chain_id) {
    uint64_t max_range = 0x7FFFFFFFFFFFFFDB;
    if (!chain_id) {
        PRINTF("CHAIN_ID: null pointer provided\n");
        return false;
    }
    if (!get_uint64_t_from_tlv_data(data, chain_id)) {
        PRINTF("CHAIN_ID: failed to extract\n");
        return false;
    }
    // Check if the chain ID is supported
    if ((*chain_id > max_range) || (*chain_id == 0)) {
        PRINTF("Unsupported chain ID: %llu\n", *chain_id);
        return false;
    }
    return true;
}

/**
 * @brief Parse and get a HASH value.
 *
 * @param[in] data to handle
 * @param[out] out buffer to store the hash
 * @return whether the handling was successful
 */
bool tlv_get_hash(const tlv_data_t *data, char *out) {
    buffer_t hash = {0};
    if (!out) {
        PRINTF("HASH: null pointer provided\n");
        return false;
    }
    if (!get_buffer_from_tlv_data(data, &hash, CX_SHA256_SIZE, CX_SHA256_SIZE)) {
        PRINTF("HASH: failed to extract\n");
        return false;
    }
    CHECK_NOT_EMPTY_BUFFER("HASH", hash);
    memmove((void *) out, hash.ptr, hash.size);
    return true;
}

/**
 * @brief Parse and get a valid ADDRESS value.
 *
 * @param[in] data to handle
 * @param[out] out buffer to store the hash
 * @param[in] not_empty whether the buffer must not be empty (0 values)
 * @return whether the handling was successful
 */
bool tlv_get_address(const tlv_data_t *data, uint8_t *out, bool not_empty) {
    buffer_t address = {0};
    if (!out) {
        PRINTF("ADDRESS: null pointer provided\n");
        return false;
    }
    if (!get_buffer_from_tlv_data(data, &address, ADDRESS_LENGTH, ADDRESS_LENGTH)) {
        PRINTF("ADDRESS: failed to extract\n");
        return false;
    }
    if (not_empty) {
        CHECK_NOT_EMPTY_BUFFER("ADDRESS", address);
    }
    buf_shrink_expand(address.ptr, address.size, out, ADDRESS_LENGTH);
    return true;
}

/**
 * @brief Parse and get a SELECTOR value.
 *
 * @param[in] data to handle
 * @param[out] out buffer to store the selector
 * @param[in] max_size of the selector

 * @return whether the handling was successful
 */
bool tlv_get_selector(const tlv_data_t *data, uint8_t *out, uint16_t max_size) {
    buffer_t selector = {0};
    if (!out) {
        PRINTF("SELECTOR: null pointer provided\n");
        return false;
    }
    if (!get_buffer_from_tlv_data(data, &selector, 0, max_size)) {
        PRINTF("SELECTOR: failed to extract\n");
        return false;
    }
    CHECK_NOT_EMPTY_BUFFER("SELECTOR", selector);
    buf_shrink_expand(selector.ptr, selector.size, out, max_size);
    return true;
}

/**
 * @brief Parse and get an string.
 *
 * @param[in] data to handle
 * @param[out] out extracted string
 * @param[in] min_len minimum valid length
 * @param[in] max_len maximum valid length
 * @return whether the handling was successful
 */
bool tlv_get_printable_string(const tlv_data_t *data,
                              char *out,
                              uint32_t min_len,
                              uint32_t max_len) {
    if (!out) {
        PRINTF("STRING: null pointer provided\n");
        return false;
    }
    if (min_len > max_len) {
        PRINTF("STRING: Invalid limits provided\n");
        return false;
    }
    // Extract the string (with null terminator added by get_string_from_tlv_data)
    if (!get_string_from_tlv_data(data, out, min_len, max_len - 1)) {
        PRINTF("STRING: failed to extract\n");
        return false;
    }
    // Check if the name is printable
    // ('\0' is set by get_string_from_tlv_data, so we can use strlen safely)
    if (!is_printable((const char *) out, strlen((const char *) out))) {
        PRINTF("STRING is not printable!\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse and get an uint16_t value.
 *
 * @param[in] data to handle
 * @param[out] out extracted value
 * @param[in] min_val minimum valid value (inclusive)
 * @param[in] max_val maximum valid value (inclusive)
 * @return whether the handling was successful
 */
bool tlv_get_uint16(const tlv_data_t *data, uint16_t *out, uint16_t min_val, uint16_t max_val) {
    uint16_t value = 0;
    if (!out) {
        PRINTF("UINT16: null pointer provided\n");
        return false;
    }
    if (min_val > max_val) {
        PRINTF("UINT16: Invalid limits provided\n");
        return false;
    }
    if (!get_uint16_t_from_tlv_data(data, &value)) {
        PRINTF("UINT16: failed to extract\n");
        return false;
    }
    if ((value < min_val) || (value > max_val)) {
        PRINTF("UINT16: not in range\n");
        return false;
    }
    *out = value;
    return true;
}

/**
 * @brief Parse and get an uint8_t value.
 *
 * @param[in] data to handle
 * @param[out] out extracted value
 * @param[in] min_val minimum valid value (inclusive)
 * @param[in] max_val maximum valid value (inclusive)
 * @return whether the handling was successful
 */
bool tlv_get_uint8(const tlv_data_t *data, uint8_t *out, uint8_t min_val, uint8_t max_val) {
    uint8_t value = 0;
    if (!out) {
        PRINTF("UINT8: null pointer provided\n");
        return false;
    }
    if (min_val > max_val) {
        PRINTF("UINT8: Invalid limits provided\n");
        return false;
    }
    if (!get_uint8_t_from_tlv_data(data, &value)) {
        PRINTF("UINT8: failed to extract\n");
        return false;
    }
    if ((value < min_val) || (value > max_val)) {
        PRINTF("UINT8: not in range\n");
        return false;
    }
    *out = value;
    return true;
}

/**
 * @brief Parse and check an uint8_t value.
 *
 * @param[in] data to handle
 * @param[out] out extracted value
 * @param[in] expected expected value
 * @return whether the handling was successful
 */
bool tlv_check_uint8(const tlv_data_t *data, uint8_t expected) {
    uint8_t value = 0;
    if (!get_uint8_t_from_tlv_data(data, &value)) {
        PRINTF("UINT8: failed to extract\n");
        return false;
    }
    if (value != expected) {
        PRINTF("UINT8: Value mismatch (%d /%d)!\n", value, expected);
        return false;
    }
    return true;
}
