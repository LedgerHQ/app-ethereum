#pragma once

#include <stdbool.h>

/**
 * @return whether the call being described is nested in another operation, which is every call
 *         but the root of a transaction: the description of such a call ends with it, and is
 *         followed by the rest of the operation that made it
 */
bool call_description_is_nested(void);
