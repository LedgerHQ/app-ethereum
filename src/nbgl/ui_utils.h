#pragma once

#include "nbgl_use_case.h"

extern nbgl_contentTagValue_t *g_pairs;
extern nbgl_contentTagValueList_t *g_pairsList;

extern char *g_titleMsg;
extern char *g_subTitleMsg;
extern char *g_finishMsg;

void ui_all_cleanup(void);

bool ui_pairs_init(uint8_t nbPairs);
void ui_pairs_cleanup(void);

bool ui_buffers_init(uint8_t title_len, uint8_t subtitle_len, uint8_t finish_len);
void ui_buffers_cleanup(void);
