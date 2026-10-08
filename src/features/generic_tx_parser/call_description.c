#include "call_description.h"
#include "shared_context.h"  // appState
#include "eip712_session.h"
#include "operation_ctx.h"

// A V1 message describes the calls it makes from its first filter on, which starts its review
static bool in_v1_message_review(void) {
    return (appState == APP_STATE_SIGNING_EIP712) && (eip712_session_get() == EIP712_SESSION_V1);
}

bool call_description_is_nested(void) {
    if (!operation_ctx_is_root()) {
        return true;
    }
    // a V1 message has no operation of its own, so the first call it makes is its root
    return in_v1_message_review();
}
