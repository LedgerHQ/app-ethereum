#pragma once

#include <stdint.h>
#include "ux.h"
#include "nbgl_use_case.h"

void eip712_format_hash(nbgl_contentTagValue_t *pairs,
                        uint8_t nbPairs,
                        nbgl_contentTagValueList_t *pairs_list);

unsigned int ui_712_approve_cb();
unsigned int ui_712_reject_cb();
