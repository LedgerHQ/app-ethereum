#ifdef HAVE_DOMAIN_NAME

#ifndef DOMAIN_NAME_H_
#define DOMAIN_NAME_H_

#include <stdint.h>
#include <stdbool.h>

#define DOMAIN_NAME_MAX_LENGTH 30

bool has_domain_name(const uint64_t *chain_id, const uint8_t *addr);
void handle_provide_domain_name(uint8_t p1, uint8_t p2, const uint8_t *data, uint8_t length);

extern char g_domain_name[DOMAIN_NAME_MAX_LENGTH + 1];

#endif  // DOMAIN_NAME_H_

#endif  // HAVE_DOMAIN_NAME
