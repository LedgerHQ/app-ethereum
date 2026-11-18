#pragma once

#include <stdbool.h>

/**
 * The version of the EIP-712 protocol a typed message signing flow speaks
 *
 * V1 and V2 share the typed data tree and the app state, but never a flow: the first command of
 * one opens its session, and no command of the other is accepted until the app is reset. A session
 * is opened from the idle state, which it leaves for \ref APP_STATE_PREPARING_EIP712, and closing
 * it returns the app to the idle state.
 */
typedef enum {
    EIP712_SESSION_NONE,
    EIP712_SESSION_V1,
    EIP712_SESSION_V2,
} e_eip712_session;

e_eip712_session eip712_session_get(void);
bool eip712_session_open(e_eip712_session session);
bool eip712_session_is_preparing(e_eip712_session session);
void eip712_session_close(void);
