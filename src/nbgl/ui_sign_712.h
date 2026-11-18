#pragma once

#include <stdbool.h>
#include "nbgl_use_case.h"

/**
 * Start the review of a typed message, whose rows are the pairs of the pair list
 *
 * A review that warns about blind signing shows the values raw, and can be skipped on a touch
 * screen. In verbose mode, every review can be.
 *
 * @param[in] operationType the type of operation to review
 * @param[in] choiceCallback called when the user makes a choice
 * @return whether the review could be started
 */
bool ui_712_start_review(nbgl_operationType_t operationType, nbgl_choiceCallback_t choiceCallback);
