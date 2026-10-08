#include "eip712_session.h"
#include "shared_context.h"  // appState

static e_eip712_session s_session = EIP712_SESSION_NONE;

/**
 * Get the session of the flow in progress
 *
 * @return the session, \ref EIP712_SESSION_NONE if there is no typed message flow
 */
e_eip712_session eip712_session_get(void) {
    return s_session;
}

/**
 * Start a session, which starts the preparation of its typed message
 *
 * A session lasts until \ref eip712_session_close, so there can only be one at a time. Opening one
 * is the only way into \ref APP_STATE_PREPARING_EIP712.
 *
 * @param[in] session the version of the flow, not \ref EIP712_SESSION_NONE
 * @return whether the session was opened, which it is not if the app is not idle
 */
bool eip712_session_open(e_eip712_session session) {
    if ((session == EIP712_SESSION_NONE) || (s_session != EIP712_SESSION_NONE) ||
        (appState != APP_STATE_IDLE)) {
        return false;
    }
    s_session = session;
    appState = APP_STATE_PREPARING_EIP712;
    return true;
}

/**
 * Tell whether a flow is receiving its typed message, its review not started
 *
 * @param[in] session the version of the flow
 * @return whether it is
 */
bool eip712_session_is_preparing(e_eip712_session session) {
    return (s_session == session) && (appState == APP_STATE_PREPARING_EIP712);
}

/**
 * End the session, if any, which ends its flow: the app is idle again
 *
 * Without a session, the state is another flow's, and is left as it is.
 */
void eip712_session_close(void) {
    if (s_session != EIP712_SESSION_NONE) {
        appState = APP_STATE_IDLE;
    }
    s_session = EIP712_SESSION_NONE;
}
