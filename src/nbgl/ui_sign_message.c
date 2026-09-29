#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"

#define FINISH_MSG_LEN 32

#ifdef SCREEN_SIZE_WALLET
#define MSG_PAGE_FONT     SMALL_BOLD_FONT
#define MSG_LINES_IN_PAGE NB_MAX_LINES_IN_DETAILS
#else
#define MSG_PAGE_FONT     BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp
// first line of each step is taken by the "Message" title
#define MSG_LINES_IN_PAGE (NB_MAX_LINES - 1)
#endif

/**
 * Walk the message pages the way the SDK splits them for display
 *
 * @return the number of pages, or a value above UINT8_MAX if they cannot all be addressed
 */
static uint16_t ui_191_walk_pages(const char *message) {
    uint16_t nb_pages = 0;
    uint16_t len;

    do {
        if (++nb_pages > UINT8_MAX) {
            break;
        }
        len = 0;
        nbgl_getTextMaxLenInNbLines(MSG_PAGE_FONT,
                                    message,
                                    AVAILABLE_WIDTH,
                                    MSG_LINES_IN_PAGE,
                                    &len,
                                    true);
        if ((len == 0) && (message[0] != '\0')) {
            return UINT16_MAX;
        }
#ifdef SCREEN_SIZE_WALLET
        // a details page never starts on a '\n'
        if (message[len] == '\n') {
            len++;
        }
#endif
        message += len;
    } while (message[0] != '\0');
    return nb_pages;
}

#ifdef SCREEN_SIZE_WALLET
/**
 * Number of pages the SDK "More" details view allots to the message, on 16 bits
 */
static uint16_t ui_191_details_nb_pages(const char *message) {
    uint16_t nb_lines =
        nbgl_getTextNbLinesInWidth(SMALL_REGULAR_FONT, message, AVAILABLE_WIDTH, true);
    uint16_t nb_pages = (nb_lines + NB_MAX_LINES_IN_DETAILS - 1) / NB_MAX_LINES_IN_DETAILS;

    if (nb_pages > 1) {
        uint16_t nb_lost_chars = (nb_pages - 1) * 3;
        uint16_t nb_lost_lines =
            (nb_lost_chars + (AVAILABLE_WIDTH / 16) - 1) / (AVAILABLE_WIDTH / 16);
        uint16_t nb_lines_in_last_page = nb_lines - ((nb_pages - 1) * NB_MAX_LINES_IN_DETAILS);

        nb_pages += nb_lost_lines / NB_MAX_LINES_IN_DETAILS;
        if ((nb_lines_in_last_page + (nb_lost_lines % NB_MAX_LINES_IN_DETAILS)) >
            NB_MAX_LINES_IN_DETAILS) {
            nb_pages++;
        }
    }
    return nb_pages;
}
#endif

bool ui_191_message_fits(const char *message) {
    // The UI counts pages on 8 bits: every page the message needs must be reachable
    uint16_t nb_pages = ui_191_walk_pages(message);

    if (nb_pages > UINT8_MAX) {
        return false;
    }
#ifdef SCREEN_SIZE_WALLET
    // the details view only navigates through the pages it allotted
    uint16_t nb_allotted = ui_191_details_nb_pages(message);
    if ((nb_allotted > UINT8_MAX) || (nb_pages > nb_allotted)) {
        return false;
    }
#endif
    return true;
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
