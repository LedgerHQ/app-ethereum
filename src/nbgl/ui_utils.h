#pragma once

#include "nbgl_use_case.h"

// nbgl counts the pairs of a review on a byte, and the pages of a Nano review too: a page per pair,
// plus the title page and the approve and reject pages.
#ifdef SCREEN_SIZE_WALLET
#define MAX_REVIEW_ROWS UINT8_MAX
#else
#define MAX_REVIEW_ROWS (UINT8_MAX - 3)
#endif

extern nbgl_contentTagValue_t *g_pairs;
extern nbgl_contentTagValueList_t *g_pairsList;

extern char *g_titleMsg;
extern char *g_subTitleMsg;
extern char *g_finishMsg;

void ui_all_cleanup(void);

bool ui_pairs_init(size_t nbPairs);
void ui_pairs_cleanup(void);

bool ui_buffers_init(uint8_t title_len, uint8_t subtitle_len, uint8_t finish_len);
void ui_buffers_cleanup(void);
