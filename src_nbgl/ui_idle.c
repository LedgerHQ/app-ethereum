#include "common_ui.h"
#include "shared_context.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "glyphs.h"
#include "network.h"

uint8_t staxSharedBuffer[SHARED_BUFFER_SIZE] = {0};

nbgl_page_t* pageContext;

void releaseContext(void) {
    if (pageContext != NULL) {
        nbgl_pageRelease(pageContext);
        pageContext = NULL;
    }
}
enum { BACK_TOKEN = 0, INFO_TOKEN, NEXT_TOKEN, CANCEL_TOKEN, QUIT_INFO_TOKEN, QUIT_APP_TOKEN };

void app_quit(void) {
    // exit app here
    os_sched_exit(-1);
}

void ui_idle(void) {
    if (plugin_name != NULL) {  // plugin
        nbgl_useCasePlugInHome((char*) plugin_name,
                               APPNAME,
                               &ICONGLYPH_SMALL,
                               NULL,
                               NULL,
                               true,
                               ui_menu_settings,
                               app_quit);
    } else {
        char* app_name = (char*) get_app_network_name();

        switch (get_app_chain_id()) {
            // Standalone apps
            case 1:  // Mainnet
            case 3:  // Ropsten
            case 5:  // Goerli
                nbgl_useCaseHome(app_name,
                                 get_app_chain_icon(),
                                 NULL,
                                 true,
                                 ui_menu_settings,
                                 app_quit);
                break;
            // Clones
            default:
                nbgl_useCaseHome(app_name,
                                 get_app_chain_icon(),
                                 NULL,
                                 true,
                                 ui_menu_settings,
                                 app_quit);
                break;
        }
    }
}
