/*
 * APDU adapter for the dispatcher harness.
 *
 * Sits between the SDK boundary (fuzz_dispatcher.c) and the production
 * routing logic (src/apdu_dispatch.c, called by fuzz_apdu_dispatch.c):
 *
 * 1. Skips the fuzz-only extension INS (handled by
 *    `fuzz_dispatch_extension` for the non-APDU surfaces).
 * 2. Looks up the picked INS in the canonical fuzz command registry and
 *    drops iterations whose `cmd->data == NULL` / `lc > 0` combination
 *    cannot be a well-formed APDU.
 * 3. Runs the harness-side state bootstraps for handlers that depend on
 *    prior-APDU state (GTP, EIP-712 STRUCT_IMPL/FILTERING/SIGN, signer
 *    descriptor). The bootstraps live in fuzz_runtime.c and allocate from
 *    the freshly-reset heap (see the reset/init ordering contract there).
 * 4. Re-encodes the picked command as a real APDU byte buffer and runs it
 *    through `apdu_parser()` before forwarding to `fuzz_dispatch_apdu()`,
 *    so the fuzzer exercises the exact parser production uses.
 *
 * `appState` lives in the Absolution prefix (domain-overridden to valid
 * enum values) and survives `fuzz_app_reset()`, so the bootstrap selector
 * sees the value the mutator wrote into the prefix.
 */
#include "fuzz_apdu_adapter.h"

#include <string.h>

#include "fuzz_apdu_dispatch.h"
#include "fuzz_command_registry.h"
#include "fuzz_non_apdu.h"
#include "fuzz_runtime.h"
#include "shared_context.h"
#include "apdu_constants.h"

static void fuzz_pre_dispatch_bootstrap(const command_t *cmd, const fuzz_command_meta_t *meta) {
    if (meta == NULL) return;

    switch (meta->tlv_kind) {
        case FUZZ_TLV_GTP_TX_INFO:
        case FUZZ_TLV_GTP_FIELD:
            if (appState == APP_STATE_SIGNING_TX) {
                fuzz_bootstrap_gtp_ctx();
                fuzz_bootstrap_enum_entries();
            }
            break;
        case FUZZ_TLV_SAFE_DESCRIPTOR:
            if (cmd->p2 == 0x01) {
                fuzz_bootstrap_safe_ctx();
            }
            break;
        default:
            break;
    }

    if (cmd->ins == INS_EIP712_STRUCT_IMPL || cmd->ins == INS_EIP712_FILTERING ||
        cmd->ins == INS_SIGN_EIP_712_MESSAGE) {
        if (appState == APP_STATE_SIGNING_EIP712) {
            fuzz_bootstrap_eip712_ctx();
        }
    }
}

void fuzz_dispatch_command(command_t *cmd) {
    const fuzz_command_meta_t *meta;
    command_t parsed = {0};
    uint8_t apdu[5 + UINT8_MAX] = {0};

    if (cmd == NULL) {
        return;
    }

    if (fuzz_dispatch_extension(cmd)) {
        return;
    }

    meta = fuzz_command_meta_by_ins(cmd->ins);
    if ((meta == NULL) || (meta->group != FUZZ_COMMAND_GROUP_APDU)) {
        return;
    }
    if ((cmd->data == NULL) && !meta->allow_empty_data) {
        return;
    }
    if ((cmd->lc > 0) && (cmd->data == NULL)) {
        return;
    }

    fuzz_pre_dispatch_bootstrap(cmd, meta);

    apdu[0] = cmd->cla;
    apdu[1] = cmd->ins;
    apdu[2] = cmd->p1;
    apdu[3] = cmd->p2;
    apdu[4] = cmd->lc;
    if (cmd->lc > 0) {
        memcpy(apdu + 5, cmd->data, cmd->lc);
    }

    if (!apdu_parser(&parsed, apdu, 5 + cmd->lc)) {
        return;
    }
    fuzz_dispatch_apdu(&parsed);
}
