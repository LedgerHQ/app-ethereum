#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "feature_signTx.h"

static void ui_warning_contract_data_choice2(bool confirm) {
    if (confirm) {
        start_signature_flow();
    } else {
        report_finalize_error();
    }
}

static void ui_warning_contract_data_choice1(bool confirm) {
    if (confirm) {
        report_finalize_error();
    } else {
        nbgl_useCaseChoice(
            NULL,
            "The transaction cannot be trusted",
            "Your Ledger cannot decode this transaction. If you sign it, you could be authorizing "
            "malicious actions that can drain your wallet.\n\nLearn more: ledger.com/e8",
            "I accept the risk",
            "Reject transaction",
            ui_warning_contract_data_choice2);
    }
}

void ui_warning_contract_data(void) {
    nbgl_useCaseChoice(
        &C_Warning_64px,
        "Security risk detected",
        "It may not be safe to sign this transaction. To continue, you'll need to review the risk.",
        "Back to safety",
        "Review risk",
        ui_warning_contract_data_choice1);
}
