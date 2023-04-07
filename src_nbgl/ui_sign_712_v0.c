#include "common_ui.h"
#include "ui_nbgl.h"
#include "common_712.h"
#include "network.h"
#include "ethUtils.h"
#include "ui_712_common.h"

static nbgl_layoutTagValue_t pairs[2];

static void start_review(void);  // forward declaration

static char *format_hash(const uint8_t *hash, char *buffer, size_t buffer_size, size_t offset) {
    snprintf(buffer + offset, buffer_size - offset, "0x%.*H", KECCAK256_HASH_BYTESIZE, hash);
    return buffer + offset;
}

static bool display_review_page(uint8_t page, nbgl_pageContent_t *content) {
    if (page == 0) {
        pairs[0].item = "Domain hash";
        pairs[0].value = format_hash(tmpCtx.messageSigningContext712.domainHash,
                                     strings.tmp.tmp,
                                     sizeof(strings.tmp.tmp),
                                     0);
        pairs[1].item = "Message hash";
        pairs[1].value = format_hash(tmpCtx.messageSigningContext712.messageHash,
                                     strings.tmp.tmp,
                                     sizeof(strings.tmp.tmp),
                                     70);

        content->type = TAG_VALUE_LIST;
        content->tagValueList.nbPairs = 2;
        content->tagValueList.nbMaxLinesForValue = 0;
        content->tagValueList.pairs = (nbgl_layoutTagValue_t *) pairs;
    } else if (page == 1) {
        content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(true);
        content->infoLongPress.text = "Sign typed message";
        content->infoLongPress.longPressText = "Hold to sign";
    } else {
        return false;
    }
    // valid page so return true
    return true;
}

static void start_review(void) {
    nbgl_useCaseRegularReview(0, 2, "Reject", NULL, display_review_page, nbgl_712_review_choice);
}

void ui_sign_712_v0(void) {
    nbgl_712_start(&start_review, "Sign typed message");
}
