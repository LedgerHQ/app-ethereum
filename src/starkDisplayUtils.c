#ifdef HAVE_STARKWARE

#include "shared_context.h"

void stark_sign_display_master_account() {
    snprintf(strings.tmp.tmp,
             sizeof(strings.tmp.tmp),
             "0x%.*H",
             32,
             dataContext.starkContext.transferDestination);
}

void stark_sign_display_condition_fact() {
    snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "0x%.*H", 32, dataContext.starkContext.fact);
}

#endif