#ifndef GTP_PARAM_NFT_H_
#define GTP_PARAM_NFT_H_

#ifdef HAVE_NFT_SUPPORT

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "gtp_value.h"

typedef struct {
    uint8_t version;
    s_value id;
    s_value collection;
} s_param_nft;

typedef struct {
    s_param_nft *param;
} s_param_nft_context;

bool handle_param_nft_struct(const s_tlv_data *data, s_param_nft_context *context);
bool format_param_nft(const s_param_nft *param, const char *name);

#endif

#endif  // !GTP_PARAM_NFT_H_
