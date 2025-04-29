#include "ui_nbgl.h"
#include "common_712.h"
#include "ui_message_signing.h"
#include "cmd_get_tx_simulation.h"
#include "utils.h"

static nbgl_contentTagValue_t pairs[2];
static nbgl_contentTagValueList_t pairs_list;

static char *format_hash(const uint8_t *hash, char *buffer, size_t buffer_size, size_t offset) {
    array_bytes_string(buffer + offset, buffer_size - offset, hash, KECCAK256_HASH_BYTESIZE);
    return buffer + offset;
}

void ui_sign_712_v0(void) {
    explicit_bzero(pairs, sizeof(pairs));
    explicit_bzero(&pairs_list, sizeof(pairs_list));

    pairs[0].item = "Domain hash";
    pairs[0].value = format_hash(tmpCtx.messageSigningContext712.domainHash,
                                 strings.tmp.tmp,
                                 sizeof(strings.tmp.tmp),
                                 0);
    pairs[1].item = "Message hash";
    pairs[1].value = format_hash(tmpCtx.messageSigningContext712.messageHash,
                                 strings.tmp.tmp,
                                 sizeof(strings.tmp.tmp),
                                 70);

    pairs_list.nbPairs = ARRAYLEN(pairs);
    pairs_list.pairs = pairs;
    pairs_list.nbMaxLinesForValue = 0;

    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    appState = APP_STATE_SIGNING_EIP712;
    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_WEB3_CHECKS
    set_tx_simulation_warning(&warning, true, true);
#endif
    warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);

    snprintf(g_stax_shared_buffer,
             sizeof(g_stax_shared_buffer),
             "%s typed message?",
             ui_tx_simulation_finish_str());
    nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                               &pairs_list,
                               &ICON_APP_REVIEW,
                               "Review typed message",
                               NULL,
                               g_stax_shared_buffer,
                               NULL,
                               &warning,
                               ui_typed_message_review_choice_v0);
}
