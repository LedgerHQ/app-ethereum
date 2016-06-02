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

#pragma once

#ifndef uint64bits_t
typedef unsigned long long uint64bits_t;
#endif

struct app_cx_sha3_s {
    struct cx_hash_header_s header;

    // x bytes per input block, depends on sha3-xxx output size
    unsigned int output_size;
    unsigned char block_size;
    //
    int blen;
    unsigned char block[200];
    // use 64bits type to ensure alignment
    uint64bits_t acc[25];
    // unsigned char    acc[200];
};
typedef struct app_cx_sha3_s app_cx_sha3_t;

int app_cx_sha3_init(app_cx_sha3_t *hash, int size);
void app_cx_sha3_block(app_cx_sha3_t *hash);
int app_cx_hash(cx_hash_t *hash, int mode, unsigned char WIDE *in, int len,
                unsigned char *out);
