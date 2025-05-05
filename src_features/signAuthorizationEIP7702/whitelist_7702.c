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
    // Multichain deployment of
    // https://github.com/eth-infinitism/account-abstraction/blob/develop/deployments/ethereum/Simple7702Account.json
    {.chain_id = 0,
     .name = "Simple7702Account",
     .address = {0x4C, 0xd2, 0x41, 0xE8, 0xd1, 0x51, 0x0e, 0x30, 0xb2, 0x07,
                 0x63, 0x97, 0xaf, 0xc7, 0x50, 0x8A, 0xe5, 0x9C, 0x66, 0xc9}},
    {.chain_id = 0,
     .name = "MetaMask Delegation Manager",
     .address = {0xdb, 0x9B, 0x1e, 0x94, 0xB5, 0xb6, 0x9D, 0xf7, 0xe4, 0x01,
                 0xDD, 0xbe, 0xdE, 0x43, 0x49, 0x11, 0x41, 0x04, 0x7d, 0xB3}},
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
