#include "common_ui.h"
#include "common_712.h"
#include "ui_nbgl.h"
#include "ui_logic.h"
#include "ui_message_signing.h"
#include "cmd_get_tx_simulation.h"
#include "utils.h"
#include "ui_utils.h"

#ifdef SCREEN_SIZE_WALLET
#define PAIR_COUNT 7
#else
#define PAIR_COUNT 2
#endif

static uint8_t pair_idx;
static bool review_skipped;
static bool hash_displayed;

static nbgl_callback_t skip_callback = NULL;

static void message_progress(bool confirm) {
    if (!review_skipped) {
#ifdef SCREEN_SIZE_WALLET
        if (g_pairsList->nbPairs < pair_idx) {
            ui_712_delete_pairs(pair_idx - g_pairsList->nbPairs);
            const s_ui_712_pair *tmp = ui_712_get_pairs();
            if (tmp != NULL) {
                g_pairs[0].item = tmp->key;
                g_pairs[0].value = tmp->value;
            }
            pair_idx = pair_idx - g_pairsList->nbPairs;
        }
#else
        ui_712_delete_pairs(0);
        pair_idx = 0;
#endif
    }
    if (confirm) {
        if (ui_712_next_field() == EIP712_NO_MORE_FIELD) {
            ui_712_switch_to_sign();
        }
    } else {
        ui_typed_message_review_choice(false);
    }
}

#ifdef SCREEN_SIZE_WALLET
static void review_skip(void) {
    review_skipped = true;
    message_progress(true);
}
#endif  // SCREEN_SIZE_WALLET

static void message_update(bool confirm) {
    bool flag;
    bool skippable;

    if (confirm) {
        if (!review_skipped) {
            LEDGER_ASSERT(ui_712_push_new_pair(strings.tmp.tmp2, strings.tmp.tmp), "Out of memory");
            const s_ui_712_pair *tmp;
            for (tmp = ui_712_get_pairs(); (s_ui_712_pair *) ((s_flist_node *) tmp)->next != NULL;
                 tmp = (s_ui_712_pair *) ((s_flist_node *) tmp)->next)
                ;
            if (tmp != NULL) {
                g_pairs[pair_idx].item = tmp->key;
                g_pairs[pair_idx].value = tmp->value;
                pair_idx += 1;
            }
            skippable = warning.predefinedSet & SET_BIT(BLIND_SIGNING_WARN);
            g_pairsList->nbPairs =
                nbgl_useCaseGetNbTagValuesInPageExt(pair_idx, g_pairsList, 0, skippable, &flag);
        }
#ifdef SCREEN_SIZE_WALLET
        if (!review_skipped && ((pair_idx == PAIR_COUNT) || (g_pairsList->nbPairs < pair_idx))) {
#else
        if (!review_skipped) {
#endif
            nbgl_useCaseReviewStreamingContinueExt(g_pairsList, message_progress, skip_callback);
        } else {
            message_progress(true);
        }
    } else {
        ui_typed_message_review_choice(false);
    }
}

static void ui_712_start_common(void) {
    ui_pairs_init(PAIR_COUNT);
    pair_idx = 0;
    review_skipped = false;
    hash_displayed = false;
#ifdef SCREEN_SIZE_WALLET
    skip_callback = review_skip;
#endif  // SCREEN_SIZE_WALLET
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    appState = APP_STATE_SIGNING_EIP712;
    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning(&warning, false, false);
#endif
}

void ui_712_start_unfiltered(void) {
    ui_712_start_common();
    warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);
    nbgl_useCaseAdvancedReviewStreamingStart(TYPE_MESSAGE | SKIPPABLE_OPERATION,
                                             &ICON_APP_REVIEW,
                                             "Review typed message",
                                             NULL,
                                             &warning,
                                             message_update);
}

void ui_712_start(void) {
    ui_712_start_common();
    if (warning.predefinedSet == 0) {
        nbgl_useCaseReviewStreamingStart(TYPE_MESSAGE,
                                         &ICON_APP_REVIEW,
                                         "Review typed message",
                                         NULL,
                                         message_update);
    } else {
        nbgl_useCaseAdvancedReviewStreamingStart(TYPE_MESSAGE,
                                                 &ICON_APP_REVIEW,
                                                 "Review typed message",
                                                 NULL,
                                                 &warning,
                                                 message_update);
    }
}

void ui_712_switch_to_message(void) {
    message_update(true);
}

#ifdef HAVE_TRANSACTION_CHECKS
static void ui_712_tx_check_cb(bool confirm) {
    const char *tx_check_str = NULL;
#ifdef SCREEN_SIZE_WALLET
    const char *title_suffix = " typed message?";
#else
    const char *title_suffix = " message";
#endif
    uint8_t finish_len = 1;  // Initialize lengths to 1 for '\0' character

    if (confirm) {
        // User has clicked on "Reject transaction"
        ui_typed_message_review_choice(false);
    } else {
        tx_check_str = ui_tx_simulation_finish_str();
        finish_len += strlen(tx_check_str);
        finish_len += strlen(title_suffix);
        ui_buffers_init(0, 0, finish_len);
        // User has clicked on "Sign anyway"
        snprintf(g_finishMsg, finish_len, "%s%s", tx_check_str, title_suffix);
        nbgl_useCaseReviewStreamingFinish(g_finishMsg, ui_typed_message_review_choice);
    }
}
#endif

void ui_712_switch_to_sign(void) {
#ifdef SCREEN_SIZE_WALLET
    const char *tx_check_str = ui_tx_simulation_finish_str();
    const char *title_suffix = " typed message?";
#else
    const char *tx_check_str = "Sign";
    const char *title_suffix = " message";
#endif
    uint8_t finish_len = 1;  // Initialize lengths to 1 for '\0' character

    if (!review_skipped && (pair_idx > 0)) {
        g_pairsList->nbPairs = pair_idx;
        pair_idx = 0;
        nbgl_useCaseReviewStreamingContinueExt(g_pairsList, message_progress, skip_callback);
    } else {
        if (N_storage.displayHash && !hash_displayed) {
            // If the hash is not displayed yet, we need to format it and display it
            // Prepare the pairs list with the hashes
            eip712_format_hash(g_pairs, PAIR_COUNT, g_pairsList);
            g_pairs[0].forcePageStart = 1;
            hash_displayed = true;
            pair_idx = 0;
            // Continue with the pairs list but no more skip needed here
            nbgl_useCaseReviewStreamingContinue(g_pairsList, message_progress);
            return;
        }
#ifdef HAVE_TRANSACTION_CHECKS
        if ((TX_SIMULATION.risk != RISK_UNKNOWN) && ((check_tx_simulation_hash() == false) ||
                                                     check_tx_simulation_from_address() == false)) {
            ui_tx_simulation_error(ui_712_tx_check_cb);
            return;
        }
#endif
        finish_len += strlen(tx_check_str);
        finish_len += strlen(title_suffix);
        ui_buffers_init(0, 0, finish_len);
        snprintf(g_finishMsg, finish_len, "%s%s", tx_check_str, title_suffix);
        nbgl_useCaseReviewStreamingFinish(g_finishMsg, ui_typed_message_review_choice);
    }
}
