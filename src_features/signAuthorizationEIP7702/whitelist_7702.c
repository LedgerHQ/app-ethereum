#ifdef HAVE_EIP7702_WHITELIST

#include <string.h>
#include "os_utils.h"
#include "os_pic.h"
#include "shared_context.h"
#include "common_utils.h"
#include "whitelist_7702.h"
#include "shared_7702.h"

// EIP 7702 whitelist
// Chain ID 0 is valid for all chain IDs
static const eip7702_whitelist_t EIP7702_WHITELIST[] = {
#ifdef HAVE_EIP7702_WHITELIST_TEST
    {.chain_id = 0, .name = "Zero", .address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.chain_id = 1, .name = "One", .address = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                               0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                               0x01, 0x01, 0x01, 0x01, 0x01, 0x01}},
    {.chain_id = 2, .name = "Two", .address = {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
                                               0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
                                               0x02, 0x02, 0x02, 0x02, 0x02, 0x02}},
    {.chain_id = 0xFFFFFFFFFFFFFFFF,
     .name = "Max",
     .address = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
#endif  // HAVE_EIP7702_WHITELIST_TEST
};

const char *get_delegate_name(const uint64_t *chain_id, const uint8_t *address) {
    for (size_t i = 0; i < ARRAYLEN(EIP7702_WHITELIST); i++) {
        if (((*chain_id == CHAIN_ID_ALL) ||  // request is valid for all chains, no check to do
             (EIP7702_WHITELIST[i].chain_id ==
              CHAIN_ID_ALL) ||  // entry is valid for all chains, no check to do
             (EIP7702_WHITELIST[i].chain_id == *chain_id)) &&
            (memcmp(PIC(EIP7702_WHITELIST[i].address), address, ADDRESS_LENGTH) == 0)) {
            return PIC(EIP7702_WHITELIST[i].name);
        }
    }
    return NULL;
}

#endif  // HAVE_EIP7702_WHITELIST