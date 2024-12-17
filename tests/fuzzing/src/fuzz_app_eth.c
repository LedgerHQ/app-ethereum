#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "shared_context.h"
#include "network_dynamic.h"

unsigned char G_io_apdu_buffer[IO_APDU_BUFFER_SIZE];
tmpContent_t tmpContent;
const chain_config_t *chainConfig;
txContext_t txContext;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    explicit_bzero(G_io_apdu_buffer, 500);
    explicit_bzero(&tmpContent, sizeof(tmpContent_t));
    explicit_bzero(&txContext, sizeof(txContext_t));
    size_t offset = 0;
    size_t len = 0;
    uint8_t p1;
    uint8_t p2;
    unsigned int tx;

    while (size - offset > 4) {
        if (data[offset++] == 0) break;
        p1 = data[offset++];
        p2 = data[offset++];
        len = data[offset++];
        if (size - offset < len) return 0;
        handleNetworkConfiguration(p1, p2, data + offset, len, &tx);
        offset += len;
    }
    return 0;
}
