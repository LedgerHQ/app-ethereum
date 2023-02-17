#ifdef HAVE_TRUSTED_NAME

#include "ux.h"
#include "trusted_name.h"

//////////////////////////////////////////////////////////////////////
// clang-format off
UX_STEP_NOCB(
    ux_trusted_name_step,
    bnnn_paging,
    {
      .title = "Domain",
      .text = trusted_name
    });
// clang-format on

#endif  // HAVE_TRUSTED_NAME
