/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "os.h"
#include "cx.h"
#include "app_cx_sha3.h"

#define S64(x, y) state[x + 5 * y]
#define ROTL64(x, n) (((x) << (n)) | ((x) >> ((64) - (n))))
#define _64BITS(h, l) (h##ULL << 32) | (l##ULL)

static void cx_sha3_theta(uint64bits_t state[]) {
    uint64bits_t C[5];
    uint64bits_t D[5];
    C[0] = S64(0, 0) ^ S64(0, 1) ^ S64(0, 2) ^ S64(0, 3) ^ S64(0, 4);
    C[1] = S64(1, 0) ^ S64(1, 1) ^ S64(1, 2) ^ S64(1, 3) ^ S64(1, 4);
    C[2] = S64(2, 0) ^ S64(2, 1) ^ S64(2, 2) ^ S64(2, 3) ^ S64(2, 4);
    C[3] = S64(3, 0) ^ S64(3, 1) ^ S64(3, 2) ^ S64(3, 3) ^ S64(3, 4);
    C[4] = S64(4, 0) ^ S64(4, 1) ^ S64(4, 2) ^ S64(4, 3) ^ S64(4, 4);

    D[0] = C[4] ^ ROTL64(C[1], 1);
    D[1] = C[0] ^ ROTL64(C[2], 1);
    D[2] = C[1] ^ ROTL64(C[3], 1);
    D[3] = C[2] ^ ROTL64(C[4], 1);
    D[4] = C[3] ^ ROTL64(C[0], 1);

    S64(0, 0) = S64(0, 0) ^ D[0];
    S64(1, 0) = S64(1, 0) ^ D[1];
    S64(2, 0) = S64(2, 0) ^ D[2];
    S64(3, 0) = S64(3, 0) ^ D[3];
    S64(4, 0) = S64(4, 0) ^ D[4];

    S64(0, 1) = S64(0, 1) ^ D[0];
    S64(1, 1) = S64(1, 1) ^ D[1];
    S64(2, 1) = S64(2, 1) ^ D[2];
    S64(3, 1) = S64(3, 1) ^ D[3];
    S64(4, 1) = S64(4, 1) ^ D[4];

    S64(0, 2) = S64(0, 2) ^ D[0];
    S64(1, 2) = S64(1, 2) ^ D[1];
    S64(2, 2) = S64(2, 2) ^ D[2];
    S64(3, 2) = S64(3, 2) ^ D[3];
    S64(4, 2) = S64(4, 2) ^ D[4];

    S64(0, 3) = S64(0, 3) ^ D[0];
    S64(1, 3) = S64(1, 3) ^ D[1];
    S64(2, 3) = S64(2, 3) ^ D[2];
    S64(3, 3) = S64(3, 3) ^ D[3];
    S64(4, 3) = S64(4, 3) ^ D[4];

    S64(0, 4) = S64(0, 4) ^ D[0];
    S64(1, 4) = S64(1, 4) ^ D[1];
    S64(2, 4) = S64(2, 4) ^ D[2];
    S64(3, 4) = S64(3, 4) ^ D[3];
    S64(4, 4) = S64(4, 4) ^ D[4];
}

static void cx_sha3_rho(uint64bits_t state[]) {
    // S64(0,0) = ROTL(S64(1,0), 0);
    S64(0, 1) = ROTL64(S64(0, 1), 36);
    S64(0, 2) = ROTL64(S64(0, 2), 3);
    S64(0, 3) = ROTL64(S64(0, 3), 41);
    S64(0, 4) = ROTL64(S64(0, 4), 18);

    S64(1, 0) = ROTL64(S64(1, 0), 1);
    S64(1, 1) = ROTL64(S64(1, 1), 44);
    S64(1, 2) = ROTL64(S64(1, 2), 10);
    S64(1, 3) = ROTL64(S64(1, 3), 45);
    S64(1, 4) = ROTL64(S64(1, 4), 2);

    S64(2, 0) = ROTL64(S64(2, 0), 62);
    S64(2, 1) = ROTL64(S64(2, 1), 6);
    S64(2, 2) = ROTL64(S64(2, 2), 43);
    S64(2, 3) = ROTL64(S64(2, 3), 15);
    S64(2, 4) = ROTL64(S64(2, 4), 61);

    S64(3, 0) = ROTL64(S64(3, 0), 28);
    S64(3, 1) = ROTL64(S64(3, 1), 55);
    S64(3, 2) = ROTL64(S64(3, 2), 25);
    S64(3, 3) = ROTL64(S64(3, 3), 21);
    S64(3, 4) = ROTL64(S64(3, 4), 56);

    S64(4, 0) = ROTL64(S64(4, 0), 27);
    S64(4, 1) = ROTL64(S64(4, 1), 20);
    S64(4, 2) = ROTL64(S64(4, 2), 39);
    S64(4, 3) = ROTL64(S64(4, 3), 8);
    S64(4, 4) = ROTL64(S64(4, 4), 14);
}

static void cx_sha3_pi(uint64bits_t state[]) {
    uint64bits_t s;

    s = S64(0, 1);
    S64(0, 1) = S64(3, 0);
    S64(3, 0) = S64(3, 3);
    S64(3, 3) = S64(2, 3);
    S64(2, 3) = S64(1, 2);
    S64(1, 2) = S64(2, 1);
    S64(2, 1) = S64(0, 2);
    S64(0, 2) = S64(1, 0);
    S64(1, 0) = S64(1, 1);
    S64(1, 1) = S64(4, 1);
    S64(4, 1) = S64(2, 4);
    S64(2, 4) = S64(4, 2);
    S64(4, 2) = S64(0, 4);
    S64(0, 4) = S64(2, 0);
    S64(2, 0) = S64(2, 2);
    S64(2, 2) = S64(3, 2);
    S64(3, 2) = S64(4, 3);
    S64(4, 3) = S64(3, 4);
    S64(3, 4) = S64(0, 3);
    S64(0, 3) = S64(4, 0);
    S64(4, 0) = S64(4, 4);
    S64(4, 4) = S64(1, 4);
    S64(1, 4) = S64(3, 1);
    S64(3, 1) = S64(1, 3);
    S64(1, 3) = s;
}

static void cx_sha3_chi(uint64bits_t state[]) {
    uint64bits_t S0y, S1y;

    S0y = S64(0, 0);
    S1y = S64(1, 0);
    S64(0, 0) ^= ~S64(1, 0) & S64(2, 0);
    S64(1, 0) ^= ~S64(2, 0) & S64(3, 0);
    S64(2, 0) ^= ~S64(3, 0) & S64(4, 0);
    S64(3, 0) ^= ~S64(4, 0) & S0y;
    S64(4, 0) ^= ~S0y & S1y;

    S0y = S64(0, 1);
    S1y = S64(1, 1);
    S64(0, 1) ^= ~S64(1, 1) & S64(2, 1);
    S64(1, 1) ^= ~S64(2, 1) & S64(3, 1);
    S64(2, 1) ^= ~S64(3, 1) & S64(4, 1);
    S64(3, 1) ^= ~S64(4, 1) & S0y;
    S64(4, 1) ^= ~S0y & S1y;

    S0y = S64(0, 2);
    S1y = S64(1, 2);
    S64(0, 2) ^= ~S64(1, 2) & S64(2, 2);
    S64(1, 2) ^= ~S64(2, 2) & S64(3, 2);
    S64(2, 2) ^= ~S64(3, 2) & S64(4, 2);
    S64(3, 2) ^= ~S64(4, 2) & S0y;
    S64(4, 2) ^= ~S0y & S1y;

    S0y = S64(0, 3);
    S1y = S64(1, 3);
    S64(0, 3) ^= ~S64(1, 3) & S64(2, 3);
    S64(1, 3) ^= ~S64(2, 3) & S64(3, 3);
    S64(2, 3) ^= ~S64(3, 3) & S64(4, 3);
    S64(3, 3) ^= ~S64(4, 3) & S0y;
    S64(4, 3) ^= ~S0y & S1y;

    S0y = S64(0, 4);
    S1y = S64(1, 4);
    S64(0, 4) ^= ~S64(1, 4) & S64(2, 4);
    S64(1, 4) ^= ~S64(2, 4) & S64(3, 4);
    S64(2, 4) ^= ~S64(3, 4) & S64(4, 4);
    S64(3, 4) ^= ~S64(4, 4) & S0y;
    S64(4, 4) ^= ~S0y & S1y;
}

static const uint64bits_t WIDE C_cx_iota_RC[24] = {
    _64BITS(0x00000000, 0x00000001), _64BITS(0x00000000, 0x00008082),
    _64BITS(0x80000000, 0x0000808A), _64BITS(0x80000000, 0x80008000),
    _64BITS(0x00000000, 0x0000808B), _64BITS(0x00000000, 0x80000001),
    _64BITS(0x80000000, 0x80008081), _64BITS(0x80000000, 0x00008009),
    _64BITS(0x00000000, 0x0000008A), _64BITS(0x00000000, 0x00000088),
    _64BITS(0x00000000, 0x80008009), _64BITS(0x00000000, 0x8000000A),
    _64BITS(0x00000000, 0x8000808B), _64BITS(0x80000000, 0x0000008B),
    _64BITS(0x80000000, 0x00008089), _64BITS(0x80000000, 0x00008003),
    _64BITS(0x80000000, 0x00008002), _64BITS(0x80000000, 0x00000080),
    _64BITS(0x00000000, 0x0000800A), _64BITS(0x80000000, 0x8000000A),
    _64BITS(0x80000000, 0x80008081), _64BITS(0x80000000, 0x00008080),
    _64BITS(0x00000000, 0x80000001), _64BITS(0x80000000, 0x80008008)};

static void cx_sha3_iota(uint64bits_t state[], int round) {
    S64(0, 0) ^= C_cx_iota_RC[round];
}

int app_cx_sha3_init(app_cx_sha3_t *hash, int size) {
    os_memset(hash, 0, sizeof(app_cx_sha3_t));
    hash->output_size = size >> 3;
    hash->block_size = (1600 - 2 * size) >> 3;
    return 0;
}

void app_cx_sha3_block(app_cx_sha3_t *hash) {
    uint64bits_t *block;
    uint64bits_t *acc;
    int r;

    block = (uint64bits_t *)hash->block;
    acc = (uint64bits_t *)hash->acc;

    acc[0] ^= block[0];
    acc[1] ^= block[1];
    acc[2] ^= block[2];
    acc[3] ^= block[3];
    acc[4] ^= block[4];
    acc[5] ^= block[5];
    acc[6] ^= block[6];
    acc[7] ^= block[7];
    acc[8] ^= block[8];
    if (hash->block_size > 72) {
        acc[9] ^= block[9];
        acc[10] ^= block[10];
        acc[11] ^= block[11];
        acc[12] ^= block[12];
        if (hash->block_size > 104) {
            acc[13] ^= block[13];
            acc[14] ^= block[14];
            acc[15] ^= block[15];
            acc[16] ^= block[16];
            if (hash->block_size > 136) {
                acc[17] ^= block[17];
            }
        }
    }

    for (r = 0; r < 24; r++) {
        cx_sha3_theta(acc);
        cx_sha3_rho(acc);
        cx_sha3_pi(acc);
        cx_sha3_chi(acc);
        cx_sha3_iota(acc, r);
    }
}

int app_cx_hash(cx_hash_t *hash, int mode, unsigned char WIDE *in, int len,
                unsigned char *out) {
    unsigned int r;
    unsigned char block_size;
    unsigned char *block;
    int blen;
    unsigned char *acc;

    // --- init locals ---
    block_size = 0;
    blen = 0;
    block = NULL;
    acc = NULL;

    block_size = ((app_cx_sha3_t *)hash)->block_size;
    block = ((app_cx_sha3_t *)hash)->block;
    blen = ((app_cx_sha3_t *)hash)->blen;
    ((app_cx_sha3_t *)hash)->blen = 0;
    acc = (unsigned char *)((app_cx_sha3_t *)hash)->acc;

    // --- append input data and process all blocks ---
    if ((blen + len) >= block_size) {
        r = block_size - blen;
        do {
            os_memmove(block + blen, in, r);
            app_cx_sha3_block((app_cx_sha3_t *)hash);
            blen = 0;
            hash->counter++;
            in += r;
            len -= r;
            r = block_size;
        } while (len >= block_size);
    }

    // --- remainder ---
    os_memmove(block + blen, in, len);
    blen += len;
    ((app_cx_sha3_t *)hash)->blen = blen;

    // --- last block ---
    if (mode & CX_LAST) {
        os_memset(block + blen, 0, (200 - blen));
        block[blen] |= (hash->algo == CX_SHA3 ? 0x06 : 0x01);
        block[block_size - 1] |= 0x80;
        app_cx_sha3_block((app_cx_sha3_t *)hash);
        // provide result
        len = ((cx_sha3_t *)hash)->output_size;
        if (out) {
            os_memmove(out, acc, len);
        }
        ((app_cx_sha3_t *)hash)->blen = 0;
    }

    return len;
}
