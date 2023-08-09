#ifdef HAVE_DOMAIN_NAME

#include "ui_domain_name.h"
#include "domain_name.h"

//////////////////////////////////////////////////////////////////////
// clang-format off
UX_STEP_NOCB(
    ux_domain_name_step,
    bnnn_paging,
    {
      .title = "Domain",
      .text = g_domain_name
    });
// clang-format on

#endif  // HAVE_DOMAIN_NAME
