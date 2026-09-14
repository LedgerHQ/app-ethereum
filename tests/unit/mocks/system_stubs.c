/**
 * @file system_stubs.c
 * @brief Host-side stubs for app-layer and platform symbols.
 *
 * This file provides two categories of stubs:
 *  1. Weak default stubs for app functions that are not linked in most test
 *     binaries. The defaults return a sensible success value. Any test that
 *     needs to control the behaviour or inspect arguments installs a strong
 *     local definition that overrides the weak one at link time.
 *  2. Noreturn stubs (app_exit, send_swap_error_simple) with the longjmp
 *     handshake used by the EXPECT_NORETURN() macro in wraps.h.
 *
 * SDK/platform stubs (pic, crypto primitives, mem_utils, etc.) live in
 * sdk_stubs.c.
 */

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "buffer.h"
#include "cx_errors.h"
#include "tlv_apdu.h"

// Forward-declared SDK opaque types (avoid pulling lcx_*.h here).
typedef struct cx_hash_header_s cx_hash_t;

// =============================================================================
// Noreturn handshake — declared in mocks/wraps.h
// =============================================================================

// Tests arm the jump, run the statement inside EXPECT_NORETURN() (wraps.h),
// and inspect g_noreturn_calls afterward.
jmp_buf g_noreturn_jmp;
bool g_noreturn_armed = false;
int g_noreturn_calls = 0;

// =============================================================================
// app/src/ledger_pki.c (via public_keys.h)
// =============================================================================

__attribute__((weak)) bool check_signature_with_pubkey(uint8_t *buffer,
                                                       const uint8_t bufLen,
                                                       const uint8_t *PubKey,
                                                       const uint8_t keyLen,
                                                       const uint8_t keyUsageExp,
                                                       const uint8_t *signature,
                                                       const uint8_t sigLen) {
    (void) buffer;
    (void) bufLen;
    (void) PubKey;
    (void) keyLen;
    (void) keyUsageExp;
    (void) signature;
    (void) sigLen;
    return true;
}

// =============================================================================
// app/src/hash_bytes.c
// =============================================================================

__attribute__((weak)) bool finalize_hash(cx_hash_t *hash_ctx, uint8_t *out, size_t out_len) {
    (void) hash_ctx;
    memset(out, 0, out_len);
    return true;
}

__attribute__((weak)) void hash_nbytes(const uint8_t *bytes, size_t n, cx_hash_t *hash_ctx) {
    (void) bytes;
    (void) n;
    (void) hash_ctx;
}

// =============================================================================
// app/src/tlv_apdu.c
// =============================================================================

__attribute__((weak)) e_tlv_apdu_ret tlv_from_apdu(bool first_chunk,
                                                   uint8_t lc,
                                                   const uint8_t *payload,
                                                   f_tlv_payload_handler handler) {
    (void) first_chunk;
    (void) lc;
    (void) payload;
    (void) handler;
    return TLV_APDU_SUCCESS;
}

// =============================================================================
// app/src/network.c
// =============================================================================

__attribute__((weak)) uint64_t __wrap_get_tx_chain_id(void) {
    return 1;
}

// chain_config_t is type-erased to const void * to avoid pulling
// shared_context.h.
__attribute__((weak)) const char *__wrap_get_displayable_ticker(const uint64_t *chain_id,
                                                                const void *config,
                                                                bool fallback) {
    (void) chain_id;
    (void) config;
    (void) fallback;
    return "ETH";
}

__attribute__((weak)) const char *get_displayable_ticker(const uint64_t *chain_id,
                                                         const void *config,
                                                         bool fallback) {
    (void) chain_id;
    (void) config;
    (void) fallback;
    return "ETH";
}

__attribute__((weak)) uint64_t get_tx_chain_id(void) {
    return 1;
}

__attribute__((weak)) const char g_unknown_ticker[] = "???";

// =============================================================================
// BOLOS_SDK -- lib_tlv (and app/src/tlv_utils.c)
// =============================================================================

bool tlv_parse(const uint8_t *payload, uint16_t size, void *handler, void *context) {
    (void) payload;
    (void) size;
    (void) handler;
    (void) context;
    return true;
}

__attribute__((weak)) bool check_challenge(uint32_t received_challenge) {
    (void) received_challenge;
    return true;
}

__attribute__((weak)) uint64_t u64_from_BE(const uint8_t *in, uint8_t size) {
    uint8_t i = 0;
    uint64_t res = 0;
    while (i < size && i < sizeof(res)) {
        res <<= 8;
        res |= in[i];
        i++;
    }
    return res;
}

// =============================================================================
// app/src/features/generic_tx_parser/gtp_field_table.c
// =============================================================================

__attribute__((weak)) bool add_to_field_table(int type,
                                              const char *key,
                                              const char *value,
                                              const void *extra) {
    (void) type;
    (void) key;
    (void) value;
    (void) extra;
    return true;
}

__attribute__((weak)) bool set_intent_field(const char *value) {
    (void) value;
    return true;
}

// =============================================================================
// app/src/features/provide_trusted_name/trusted_name.c
// =============================================================================

__attribute__((weak)) const void *get_trusted_name(uint8_t type_count,
                                                   const void *types,
                                                   uint8_t source_count,
                                                   const void *sources,
                                                   const uint64_t *chain_id,
                                                   const uint8_t *addr) {
    (void) type_count;
    (void) types;
    (void) source_count;
    (void) sources;
    (void) chain_id;
    (void) addr;
    return NULL;
}

// =============================================================================
// ethereum-plugin-sdk/src/common_utils.c
// =============================================================================

__attribute__((weak)) bool amountToString(const uint8_t *amount,
                                          uint8_t amount_len,
                                          uint8_t decimals,
                                          const char *ticker,
                                          char *out_buffer,
                                          size_t out_buffer_size) {
    (void) amount;
    (void) amount_len;
    (void) decimals;
    (void) ticker;
    if (out_buffer != NULL && out_buffer_size > 0) {
        strncpy(out_buffer, "1.5", out_buffer_size);
        out_buffer[out_buffer_size - 1] = '\0';
    }
    return true;
}

__attribute__((weak)) bool __wrap_amountToString(const uint8_t *amount,
                                                 uint8_t amount_len,
                                                 uint8_t decimals,
                                                 const char *ticker,
                                                 char *out_buffer,
                                                 size_t out_buffer_size) {
    (void) amount;
    (void) amount_len;
    (void) decimals;
    (void) ticker;
    if (out_buffer != NULL && out_buffer_size > 0) {
        strncpy(out_buffer, "1.5", out_buffer_size);
        out_buffer[out_buffer_size - 1] = '\0';
    }
    return true;
}

__attribute__((weak)) bool getEthDisplayableAddress(const uint8_t *in,
                                                    char *out,
                                                    size_t out_size,
                                                    uint64_t chain_id) {
    (void) in;
    (void) chain_id;
    if (out != NULL && out_size > 0) {
        strncpy(out, "0xdeadbeef", out_size);
        out[out_size - 1] = '\0';
    }
    return true;
}

__attribute__((weak)) bool __wrap_getEthDisplayableAddress(const uint8_t *in,
                                                           char *out,
                                                           size_t out_size,
                                                           uint64_t chain_id) {
    (void) in;
    (void) chain_id;
    if (out != NULL && out_size > 0) {
        strncpy(out, "0xdeadbeef", out_size);
        out[out_size - 1] = '\0';
    }
    return true;
}

__attribute__((weak)) uint16_t get_public_key(uint8_t *out, uint8_t out_size) {
    if (out != NULL && out_size >= 20) {
        memset(out, 0xAB, 20);
    }
    return 0x9000;  // SWO_SUCCESS
}

__attribute__((weak)) bool get_network_as_string(char *out, size_t out_len) {
    if (out != NULL && out_len > 0) {
        strncpy(out, "Ethereum", out_len);
        out[out_len - 1] = '\0';
    }
    return true;
}

// =============================================================================
// app/src/features/get_challenge/cmd_get_challenge.c
// =============================================================================

__attribute__((weak)) void roll_challenge(void) {
}

// =============================================================================
// app/src/features/generic_tx_parser/gtp_data_path.c
// =============================================================================

__attribute__((weak)) bool handle_data_path_struct(const void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

__attribute__((weak)) void data_path_cleanup(const void *collection) {
    (void) collection;
}

__attribute__((weak)) bool data_path_get(const void *data_path, void *collection) {
    (void) data_path;
    (void) collection;
    return true;
}

// =============================================================================
// app/src/features/generic_tx_parser/tx_ctx.c
// =============================================================================

__attribute__((weak)) const uint8_t *get_current_tx_to(void) {
    return NULL;
}

__attribute__((weak)) const uint8_t *get_current_tx_from(void) {
    return NULL;
}

__attribute__((weak)) const uint8_t *get_current_tx_info(void) {
    return NULL;
}

__attribute__((weak)) const uint8_t *get_current_tx_amount(void) {
    return NULL;
}

// =============================================================================
// app/src/features/generic_tx_parser/gtp_value.c
// =============================================================================

struct s_value;
struct s_parsed_value_collection;

__attribute__((weak)) void value_cleanup(const struct s_value *value,
                                         const struct s_parsed_value_collection *collection) {
    (void) value;
    (void) collection;
}

// =============================================================================
// app/src/features/provide_map_entry/map_entry.c
// =============================================================================

__attribute__((weak)) const void *get_matching_map_entry(uint8_t id,
                                                         const uint8_t *key,
                                                         uint8_t key_size) {
    (void) id;
    (void) key;
    (void) key_size;
    return NULL;
}

// =============================================================================
// app/src/features/sign_tx/logic_sign_tx.c link-fillers
// =============================================================================

struct txContext_t;
__attribute__((weak)) bool copy_tx_data(struct txContext_t *context,
                                        uint8_t *out,
                                        uint32_t length) {
    (void) context;
    (void) out;
    (void) length;
    return true;
}

__attribute__((weak)) void eth_plugin_prepare_init(void *msg,
                                                   const uint8_t *pluginName,
                                                   uint8_t pluginNameLength) {
    (void) msg;
    (void) pluginName;
    (void) pluginNameLength;
}

__attribute__((weak)) bool eth_plugin_perform_init(uint8_t *contractAddress, void *msg) {
    (void) contractAddress;
    (void) msg;
    return true;
}

__attribute__((weak)) void eth_plugin_prepare_finalize(void *msg) {
    (void) msg;
}

__attribute__((weak)) void eth_plugin_prepare_provide_info(void *msg) {
    (void) msg;
}

__attribute__((weak)) void eth_plugin_prepare_provide_parameter(void *msg,
                                                                const uint8_t *param,
                                                                uint32_t paramOffset) {
    (void) msg;
    (void) param;
    (void) paramOffset;
}

__attribute__((weak)) void *get_matching_asset_info(const uint64_t *chain_id,
                                                    const uint8_t *address) {
    (void) chain_id;
    (void) address;
    return NULL;
}

__attribute__((weak)) void ui_confirm_parameter(void) {
}

__attribute__((weak)) void ui_confirm_selector(void) {
}

struct s_calldata;
__attribute__((weak)) struct s_calldata *get_root_calldata(void) {
    return NULL;
}

__attribute__((weak)) const uint8_t *calldata_get_selector(const struct s_calldata *node) {
    (void) node;
    return NULL;
}

// =============================================================================
// app/src/main.c -- BIP-32 path parsing
// =============================================================================

struct s_bip32_path {
    unsigned int length;
    unsigned int path[10];  // BIP32_PATH_MAX_LENGTH -- enough for tests
};

__attribute__((weak)) const uint8_t *parseBip32(const uint8_t *dataBuffer,
                                                uint8_t *dataLength,
                                                struct s_bip32_path *bip32) {
    (void) bip32;
    if (*dataLength < 1) return NULL;
    uint8_t count = *dataBuffer;
    if ((size_t) *dataLength < 1 + (size_t) count * 4) return NULL;
    dataBuffer += 1 + count * 4;
    *dataLength -= 1 + count * 4;
    return dataBuffer;
}

// =============================================================================
// BOLOS_SDK -- lib_standard_app/main.c (app entry point)
// =============================================================================

__attribute__((weak)) __attribute__((noreturn)) void app_exit(void) {
    g_noreturn_calls++;
    if (g_noreturn_armed) longjmp(g_noreturn_jmp, 1);
    while (1) {
    }
}

__attribute__((weak)) __attribute__((noreturn)) void os_sched_exit(unsigned int exit_code) {
    (void) exit_code;
    g_noreturn_calls++;
    if (g_noreturn_armed) longjmp(g_noreturn_jmp, 1);
    while (1) {
    }
}

__attribute__((weak)) void app_quit(void) {
    g_noreturn_calls++;
}

__attribute__((weak)) __attribute__((noreturn)) void send_swap_error_simple(
    uint16_t status_word,
    uint8_t common_error_code,
    uint8_t application_specific_error_code) {
    (void) status_word;
    (void) common_error_code;
    (void) application_specific_error_code;
    g_noreturn_calls++;
    if (g_noreturn_armed) longjmp(g_noreturn_jmp, 1);
    while (1) {
    }
}

// Default: not at the root context, so value_get_rlp() never marks the root
// FROM/VALUE rows as shown. Tests that exercise the dedup hooks install their
// own strong definitions.
__attribute__((weak)) bool tx_ctx_is_root(void) {
    return false;
}

__attribute__((weak)) void gcs_set_root_from_shown(void) {
}

__attribute__((weak)) void gcs_set_root_value_shown(void) {
}

// Default: every chain is signable. Tests covering the rejection install a
// strong local definition.
__attribute__((weak)) bool chain_id_is_signable(uint64_t chain_id) {
    (void) chain_id;
    return true;
}
