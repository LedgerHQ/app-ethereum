#ifdef HAVE_TRUSTED_NAME

#ifndef TRUSTED_NAME_H_
#define TRUSTED_NAME_H_

#include <stdint.h>
#include <stdbool.h>

#define TRUSTED_NAME_MAX_LENGTH 255
#define ECDSA_SIG_MAX_LENGTH    73

typedef struct {
    uint8_t key_id;
    uint8_t algo_id;
    uint8_t name_length;
    uint8_t sig_length;
    uint8_t sig[ECDSA_SIG_MAX_LENGTH];
} s_trusted_name;

bool verify_trusted_name(const uint64_t *chain_id, const uint8_t *addr);
void handle_provide_trusted_name(uint8_t p1, uint8_t p2, const uint8_t *data, uint8_t length);

extern char trusted_name[TRUSTED_NAME_MAX_LENGTH + 1];

#endif  // TRUSTED_NAME_H_

#endif  // HAVE_TRUSTED_NAME
