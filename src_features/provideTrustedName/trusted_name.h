#ifdef HAVE_TRUSTED_NAME

#ifndef TRUSTED_NAME_H_
#define TRUSTED_NAME_H_

#include <stdint.h>
#include <stdbool.h>

#define TRUSTED_NAME_MAX_LENGTH 30

bool has_trusted_name(const uint64_t *chain_id, const uint8_t *addr);
uint16_t handle_provide_trusted_name(uint8_t p1, const uint8_t *data, uint8_t length);

extern char g_trusted_name[TRUSTED_NAME_MAX_LENGTH + 1];

#endif  // TRUSTED_NAME_H_

#endif  // HAVE_TRUSTED_NAME
