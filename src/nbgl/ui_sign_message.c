#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"

#define FINISH_MSG_LEN 32

bool ui_191_message_fits(const char *message) {
#ifdef SCREEN_SIZE_WALLET
    // Everything past a short excerpt is reached through the "More" details view,
    // which the SDK paginates with an 8-bit page counter. Past what that counter can
    // address, the tail of the message becomes unreachable while approval stays
    // available, so the message must be refused rather than signed unseen.
    return nbgl_getTextNbLinesInWidth(SMALL_BOLD_FONT, message, AVAILABLE_WIDTH, true) <=
           (UINT8_MAX * NB_MAX_LINES_IN_DETAILS);
#else
    // Nano walks the message as text steps, with no page counter to overflow
    UNUSED(message);
    return true;
#endif
}

static void ui_191_finish_cb(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_signMessage_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_signMessage_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
    ui_all_cleanup();
}

void ui_191_start(const char *message) {
    // Honor the "Always display the transaction or message hash" setting for
    // EIP-191 personal messages so that the user can verify the exact bytes
    // being signed even when rendering is ambiguous (whitespace, encoding,
    // long content, hex fallback). The tx hash flow does this already; the
    // message review used to silently ignore the toggle (CWE-451).
    //
    // Format the hash first and only count it as a pair if formatting
    // succeeded — otherwise we would advertise a 2-pair list with an
    // uninitialized g_pairs[1] and the device would render garbage.
    bool show_hash = false;
    if (N_storage.displayHash) {
        strlcpy(strings.common.tx_hash, "0x", 3);
        if (bytes_to_lowercase_hex(strings.common.tx_hash + 2,
                                   sizeof(strings.common.tx_hash) - 2,
                                   tmpCtx.messageSigningContext.hash,
                                   INT256_LENGTH) >= 0) {
            show_hash = true;
        }
    }

    if (!ui_pairs_init(show_hash ? 2 : 1)) {
        // Initialization failed, cleanup and return
        return;
    }
    // Initialize the buffers
    if (!ui_buffers_init(0, 0, FINISH_MSG_LEN)) {
        // Initialization failed, cleanup and return
        return;
    }

    explicit_bzero(&warning, sizeof(nbgl_warning_t));

    snprintf(g_finishMsg,
             FINISH_MSG_LEN,
#ifdef SCREEN_SIZE_WALLET
             "%s message?",
#else
             "%s message",
#endif
             ui_tx_simulation_finish_str());

    g_pairsList->wrapping = true;
    g_pairs[0].item = "Message";
    g_pairs[0].value = message;

    if (show_hash) {
        g_pairs[1].item = "Message hash";
        g_pairs[1].value = strings.common.tx_hash;
    }

    nbgl_useCaseAdvancedReview(TYPE_MESSAGE,
                               g_pairsList,
                               &LARGE_REVIEW_ICON,
                               "Review message",
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               ui_191_finish_cb);
}
