#ifndef SCHEMA_HASH_H_
#define SCHEMA_HASH_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>

bool compute_schema_hash(void);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // SCHEMA_HASH_H_
