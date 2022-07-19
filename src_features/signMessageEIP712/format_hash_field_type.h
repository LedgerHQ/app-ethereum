#ifndef FORMAT_HASH_FIELD_TYPE_H_
#define FORMAT_HASH_FIELD_TYPE_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include "cx.h"

bool format_hash_field_type(const void *const field_ptr, cx_hash_t *hash_ctx);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // FORMAT_HASH_FIELD_TYPE_H_
