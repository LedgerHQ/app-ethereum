/*
 * Non-APDU coverage shims for the dispatcher.
 *
 * Production reaches some functions via the host pipeline rather than the
 * APDU dispatcher: swap library callbacks (check_address /
 * get_printable_amount / copy_transaction_parameters) and the plugin
 * helpers (find_selector / U2BE_from_parameter / copy_address). They have
 * no INS, so they never reach `fuzz_dispatch_apdu()`.
 *
 * - `fuzz_non_apdu_reset()` re-pins the swap callback globals to static
 *   harness-owned scratch. Called from `fuzz_reset_runtime_state()` and
 *   bound by the same ordering contract (see fuzz_runtime.c).
 * - `fuzz_dispatch_extension(cmd)` owns the four `INS_FUZZ_*` pseudo-INS
 *   (swap check-address / printable-amount / copy-transaction, plugin
 *   utils sweep) and is called from `fuzz_dispatch_command()` before the
 *   APDU adapter.
 */
#include "fuzz_non_apdu.h"

#include <stdbool.h>
#include <string.h>

#include "fuzz_command_registry.h"
#include "shared_context.h"
#include "handle_check_address.h"
#include "handle_get_printable_amount.h"
#include "eth_swap_utils.h"
#include "get_public_key.h"
#include "plugin_utils.h"
#include "mem_utils.h"
#include "write.h"
#include "swap_utils.h"
#include "app_mem_utils.h"

static volatile uint8_t k_swap_return_dummy;
static uint8_t k_swap_crosschain_hash_dummy[CX_SHA256_SIZE];

bool copy_transaction_parameters(create_transaction_parameters_t *sign_transaction_params,
                                 const chain_config_t *config);

void fuzz_non_apdu_reset(void) {
    G_swap_signing_return_value_address = (uint8_t *) &k_swap_return_dummy;
    G_swap_crosschain_hash = k_swap_crosschain_hash_dummy;
}

static void fuzz_dispatch_plugin_utils(const uint8_t *data, size_t size) {
    uint8_t parameter[PARAMETER_LENGTH] = {0};
    uint8_t copied_address[ADDRESS_LENGTH] = {0};
    uint8_t copied_parameter[PARAMETER_LENGTH] = {0};
    uint16_t value16 = 0;
    uint32_t value32 = 0;
    size_t selector_idx = 0;
    const uint32_t selectors[] = {0xAABBCCDDu, 0x11223344u, 0x55667788u};

    for (size_t i = 0; i < sizeof(parameter); ++i) {
        parameter[i] = (i < size) ? data[i] : (uint8_t) (0x51 + i);
    }

    copy_address(copied_address, parameter, sizeof(copied_address));
    copy_parameter(copied_parameter, parameter, sizeof(copied_parameter));

    memset(parameter, 0, sizeof(parameter));
    parameter[PARAMETER_LENGTH - 2] = (size > 0) ? data[0] : 0x12;
    parameter[PARAMETER_LENGTH - 1] = (size > 1) ? data[1] : 0x34;
    (void) U2BE_from_parameter(parameter, &value16);
    parameter[0] = 0xFF;
    (void) U2BE_from_parameter(parameter, &value16);

    memset(parameter, 0, sizeof(parameter));
    parameter[PARAMETER_LENGTH - 4] = (size > 2) ? data[2] : 0x11;
    parameter[PARAMETER_LENGTH - 3] = (size > 3) ? data[3] : 0x22;
    parameter[PARAMETER_LENGTH - 2] = (size > 4) ? data[4] : 0x33;
    parameter[PARAMETER_LENGTH - 1] = (size > 5) ? data[5] : 0x44;
    (void) U4BE_from_parameter(parameter, &value32);
    parameter[0] = 0xFF;
    (void) U4BE_from_parameter(parameter, &value32);

    (void) find_selector(selectors[(size > 6) ? (data[6] % 3) : 0], selectors, 3, &selector_idx);
    (void) find_selector(0xDEADBEEFu, selectors, 3, NULL);
}

static void fuzz_swap_fill_bytes(uint8_t *dst,
                                 size_t dst_len,
                                 const uint8_t *data,
                                 size_t data_len,
                                 uint8_t fallback_seed) {
    for (size_t i = 0; i < dst_len; ++i) {
        dst[i] = (i < data_len) ? data[i] : (uint8_t) (fallback_seed + i);
    }
}

static size_t fuzz_swap_build_bip32_path(uint8_t *out,
                                         size_t out_len,
                                         const uint8_t *data,
                                         size_t data_len) {
    uint32_t path[5] = {
        0x8000002CU,
        0x8000003CU,
        0x80000000U,
        0,
        0,
    };

    if (out_len < (1 + sizeof(path))) {
        return 0;
    }

    if (data_len > 0) {
        path[3] = data[0];
    }
    if (data_len > 2) {
        path[4] = ((uint32_t) data[1] << 8) | data[2];
    }

    out[0] = 5;
    for (size_t i = 0; i < 5; ++i) {
        write_u32_be(out + 1 + (i * sizeof(uint32_t)), 0, path[i]);
    }
    return 1 + sizeof(path);
}

static size_t fuzz_swap_build_config(uint8_t *out,
                                     size_t out_len,
                                     const uint8_t *data,
                                     size_t data_len) {
    const char *asset_ticker = ((data_len > 0) && ((data[0] & 0x01) != 0)) ? "USDC" : "ETH";
    const uint8_t asset_decimals = ((data_len > 1) && ((data[1] & 0x01) != 0)) ? 6 : 18;
    const uint64_t chain_id = ((data_len > 2) && ((data[2] & 0x01) != 0)) ? 137 : APP_CHAIN_ID;
    const char *fees_ticker = "ETH";
    const uint8_t fees_decimals = WEI_TO_ETHER;
    const size_t asset_len = strlen(asset_ticker);
    const size_t fees_len = strlen(fees_ticker);
    size_t offset = 0;

    if ((1 + asset_len + 1 + sizeof(uint64_t) + 1 + fees_len + 1) > out_len) {
        return 0;
    }

    out[offset++] = (uint8_t) asset_len;
    memcpy(out + offset, asset_ticker, asset_len);
    offset += asset_len;
    out[offset++] = asset_decimals;
    write_u64_be(out, offset, chain_id);
    offset += sizeof(uint64_t);
    out[offset++] = (uint8_t) fees_len;
    memcpy(out + offset, fees_ticker, fees_len);
    offset += fees_len;
    out[offset++] = fees_decimals;
    return offset;
}

static bool fuzz_swap_build_expected_address(char *out,
                                             size_t out_len,
                                             const uint8_t *path_blob,
                                             size_t path_blob_len,
                                             bool with_0x,
                                             bool mutate_last_nibble) {
    bip32_path_t bip32 = {0};
    uint8_t raw_pubkey[CX_SECP256_PUB_KEY_SIZE];
    char derived[ADDRESS_LENGTH_STR] = {0};

    if ((path_blob == NULL) || (path_blob_len < 2)) {
        return false;
    }

    bip32.length = path_blob[0];
    if ((bip32.length == 0) || (bip32.length > MAX_BIP32_PATH)) {
        return false;
    }
    if (!bip32_path_read(path_blob + 1, path_blob_len - 1, bip32.path, bip32.length)) {
        return false;
    }
    if (get_public_key_string(&bip32, raw_pubkey, derived, NULL, g_chain_config->chain_id) !=
        CX_OK) {
        return false;
    }

    if (with_0x) {
        if (out_len < ADDRESS_LENGTH_HEX_STR) {
            return false;
        }
        out[0] = '0';
        out[1] = 'x';
        strlcpy(out + 2, derived, out_len - 2);
    } else {
        strlcpy(out, derived, out_len);
    }

    if (mutate_last_nibble) {
        size_t last = strlen(out);
        if (last > 0) {
            out[last - 1] = (out[last - 1] == '0') ? '1' : '0';
        }
    }
    return true;
}

static void fuzz_dispatch_swap_check_address(const uint8_t *data, size_t size) {
    uint8_t path_blob[1 + (5 * sizeof(uint32_t))] = {0};
    char address_to_check[ADDRESS_LENGTH_HEX_STR] = {0};
    check_address_parameters_t params = {0};
    const size_t path_len = fuzz_swap_build_bip32_path(path_blob, sizeof(path_blob), data, size);
    const bool with_0x = (size <= 3) || ((data[3] & 0x01) != 0);
    const bool mutate_last = (size > 4) && ((data[4] & 0x01) != 0);

    if (path_len == 0) {
        return;
    }

    if (!fuzz_swap_build_expected_address(address_to_check,
                                          sizeof(address_to_check),
                                          path_blob,
                                          path_len,
                                          with_0x,
                                          mutate_last)) {
        strlcpy(address_to_check,
                "0x0000000000000000000000000000000000000000",
                sizeof(address_to_check));
    }

    params.address_parameters = path_blob;
    params.address_parameters_length =
        (uint8_t) (((size > 5) && ((data[5] & 0x01) != 0)) ? 4 : path_len);
    params.address_to_check = address_to_check;
    (void) handle_check_address(&params, (chain_config_t *) g_chain_config);
}

static void fuzz_dispatch_swap_get_printable_amount(const uint8_t *data, size_t size) {
    uint8_t config[32] = {0};
    uint8_t amount[33] = {0};
    get_printable_amount_parameters_t params = {0};
    const size_t config_len = fuzz_swap_build_config(config, sizeof(config), data, size);

    if (config_len == 0) {
        return;
    }

    fuzz_swap_fill_bytes(amount, sizeof(amount), data, size, 0x11);

    params.coin_configuration = config;
    params.coin_configuration_length = (uint8_t) config_len;
    params.amount = amount;
    params.amount_length =
        (uint8_t) (((size > 6) && ((data[6] & 0x01) != 0)) ? 33
                                                           : (((size > 7) ? data[7] : 0) % 16) + 1);
    params.is_fee = (size > 8) && ((data[8] & 0x01) != 0);
    (void) handle_get_printable_amount(&params, (chain_config_t *) g_chain_config);
}

static void fuzz_dispatch_swap_copy_transaction(const uint8_t *data, size_t size) {
    uint8_t config[32] = {0};
    uint8_t amount[17] = {0};
    uint8_t fee_amount[9] = {0};
    uint8_t extra_id[1 + CX_SHA256_SIZE] = {0};
    char destination_address[ADDRESS_LENGTH_HEX_STR] = "0x1111111111111111111111111111111111111111";
    create_transaction_parameters_t params = {0};
    const size_t config_len = fuzz_swap_build_config(config, sizeof(config), data, size);

    if (config_len == 0) {
        return;
    }

    fuzz_swap_fill_bytes(amount, sizeof(amount), data, size, 0x21);
    fuzz_swap_fill_bytes(fee_amount, sizeof(fee_amount), data, size, 0x31);
    fuzz_swap_fill_bytes(extra_id + 1, CX_SHA256_SIZE, data, size, 0x41);
    extra_id[0] = (size > 0) ? (data[0] % 3) : 0;
    if (extra_id[0] == 2) {
        extra_id[0] = 0xFF;
    }
    if ((size > 9) && ((data[9] & 0x01) != 0)) {
        strlcpy(destination_address,
                "0x2222222222222222222222222222222222222222",
                sizeof(destination_address));
    }

    params.coin_configuration = config;
    params.coin_configuration_length = (uint8_t) config_len;
    params.amount = amount;
    params.amount_length = (uint8_t) (((size > 10) && ((data[10] & 0x01) != 0))
                                          ? 17
                                          : (((size > 11) ? data[11] : 0) % 16) + 1);
    params.fee_amount = fee_amount;
    params.fee_amount_length = (uint8_t) (((size > 12) && ((data[12] & 0x01) != 0))
                                              ? 9
                                              : (((size > 13) ? data[13] : 0) % 8) + 1);
    params.destination_address = destination_address;
    params.destination_address_extra_id =
        ((size > 14) && ((data[14] & 0x01) != 0)) ? NULL : (char *) extra_id;

    (void) copy_transaction_parameters(&params, g_chain_config);

    if ((G_swap_crosschain_hash != NULL) &&
        (G_swap_crosschain_hash != k_swap_crosschain_hash_dummy)) {
        APP_MEM_FREE(G_swap_crosschain_hash);
        G_swap_crosschain_hash = k_swap_crosschain_hash_dummy;
    }
    G_swap_signing_return_value_address = (uint8_t *) &k_swap_return_dummy;
}

bool fuzz_dispatch_extension(const command_t *cmd) {
    if (cmd == NULL) {
        return false;
    }

    switch (cmd->ins) {
        case INS_FUZZ_SWAP_CHECK_ADDRESS:
            fuzz_dispatch_swap_check_address(cmd->data, cmd->lc);
            return true;

        case INS_FUZZ_SWAP_PRINTABLE_AMOUNT:
            fuzz_dispatch_swap_get_printable_amount(cmd->data, cmd->lc);
            return true;

        case INS_FUZZ_SWAP_COPY_TRANSACTION:
            fuzz_dispatch_swap_copy_transaction(cmd->data, cmd->lc);
            return true;

        case INS_FUZZ_PLUGIN_UTILS:
            fuzz_dispatch_plugin_utils(cmd->data, cmd->lc);
            return true;

        default:
            return false;
    }
}
