#pragma once

#include <stdbool.h>

// What a command brings to the description of a call
typedef enum {
    CALL_DESCRIPTION_INFO,   // its TRANSACTION_INFO
    CALL_DESCRIPTION_FIELD,  // one of its FIELD descriptors
} e_call_description;

/**
 * Whether the app is in a state to be told about a call (a PROVIDE OPERATION INFO or FIELD
 * DESCRIPTION command with P2 = 00)
 *
 * A call is described while signing its transaction, or the message that makes it. A typed
 * message has its calls described once its own descriptors have named them, and the descriptors
 * of each once its info matched it.
 *
 * @param[in] part what the command brings
 * @return whether the command may go on
 */
bool call_description_is_allowed(e_call_description part);

/**
 * @return whether the call being described is nested in another operation, which is every call
 *         but the root of a transaction: the description of such a call ends with it, and is
 *         followed by the rest of the operation that made it
 */
bool call_description_is_nested(void);
