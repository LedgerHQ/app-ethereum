#include "call_description.h"
#include "shared_context.h"  // appState
#include "eip712_session.h"
#include "operation_ctx.h"

// A V1 message describes the calls it makes from its first filter on, which starts its review
static bool in_v1_message_review(void) {
    return (appState == APP_STATE_SIGNING_EIP712) && (eip712_session_get() == EIP712_SESSION_V1);
}

// Whether a typed message has what a command describing one of its calls needs
static bool message_allows(e_call_description part) {
    if ((eip712_session_get() != EIP712_SESSION_V2) || !operation_ctx_has_message()) {
        return false;
    }
    switch (part) {
        case CALL_DESCRIPTION_INFO:
            // its descriptors have to have named a call
            return get_operation_ctx_count() != 0;
        case CALL_DESCRIPTION_FIELD:
            // its calls are described one at a time, each once its info matched it
            return operation_ctx_is_describing_call();
        default:
            return false;
    }
}

bool call_description_is_allowed(e_call_description part) {
    switch (appState) {
        case APP_STATE_SIGNING_TX:
            return true;
        case APP_STATE_SIGNING_EIP712:
            // a V2 message is in this state once its review started, which leaves nothing to
            // describe
            return in_v1_message_review();
        case APP_STATE_PREPARING_EIP712:
            return message_allows(part);
        default:
            return false;
    }
}

bool call_description_is_nested(void) {
    if (!operation_ctx_is_root()) {
        return true;
    }
    // a V1 message has no operation of its own, so the first call it makes is its root
    return in_v1_message_review();
}
