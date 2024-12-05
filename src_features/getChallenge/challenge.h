#ifdef HAVE_TRUSTED_NAME

#ifndef CHALLENGE_H_
#define CHALLENGE_H_

#include <stdint.h>

void roll_challenge(void);
uint32_t get_challenge(void);
uint16_t handle_get_challenge(unsigned int *tx);

#endif  // CHALLENGE_H_

#endif  // HAVE_TRUSTED_NAME
