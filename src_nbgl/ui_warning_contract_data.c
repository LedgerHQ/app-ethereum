#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"

enum { OK_TOKEN };

static void pageTouchCallback(int token, uint8_t index) {
    (void) index;
    releaseContext();

    if (token == OK_TOKEN) {
      ui_idle();
    }
}

void ui_warning_contract_data(void) {
  releaseContext();
  nbgl_pageInfoDescription_t info = {
    .centeredInfo = {
      .text1 = "Error",
      .text2 = "Blind signing must be\nenabled in Settings",
      .text3 = NULL,
      .style = LARGE_CASE_INFO,
      .offsetY = 32
    },
    .footerText = NULL,
    .bottomButtonStyle = QUIT_ICON,
    .bottomButtonsToken = OK_TOKEN,
    .tapActionText = NULL,
    .topRightStyle = NO_BUTTON_STYLE,
    .topRightToken = 0,
    .tuneId = TUNE_TAP_CASUAL
  };

  pageContext = nbgl_pageDrawInfo(pageTouchCallback, NULL, &info);
  nbgl_refresh();
}
