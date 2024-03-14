#include "common_ui.h"
#include "shared_context.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "glyphs.h"
#include "caller_api.h"
#include "network.h"

char g_stax_shared_buffer[SHARED_BUFFER_SIZE] = {0};
nbgl_page_t *pageContext;

#define FORMAT_PLUGIN "This app enables clear\nsigning transactions for\nthe %s dApp."

void releaseContext(void) {
    if (pageContext != NULL) {
        nbgl_pageRelease(pageContext);
        pageContext = NULL;
    }
}

void app_quit(void) {
    // exit app here
    os_sched_exit(-1);
}

const nbgl_icon_details_t *get_app_icon(bool caller_icon) {
    const nbgl_icon_details_t *icon = NULL;

    if (caller_icon && caller_app) {
        if (caller_app->icon) {
            icon = caller_app->icon;
        }
    } else {
        icon = &ICONGLYPH;
    }
    if (icon == NULL) {
        PRINTF("%s(%s) returned NULL!\n", __func__, (caller_icon ? "true" : "false"));
    }
    return icon;
}

void ui_idle(void) {
    const char *app_name = NULL;
    const char *tagline = NULL;

    if (caller_app) {
        app_name = caller_app->name;

        if (caller_app->type == CALLER_TYPE_PLUGIN) {
            snprintf(g_stax_shared_buffer, sizeof(g_stax_shared_buffer), FORMAT_PLUGIN, app_name);
            tagline = g_stax_shared_buffer;
        }
    } else {  // Ethereum app
        uint64_t mainnet_chain_id = ETHEREUM_MAINNET_CHAINID;
        app_name = get_network_name_from_chain_id(&mainnet_chain_id);
    }

    nbgl_useCaseHome((char *) app_name,
                     get_app_icon(true),
                     tagline,
                     true,
                     ui_menu_settings,
                     app_quit);
}
