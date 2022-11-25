#ifdef HAVE_DOMAIN_NAME

#ifndef CHALLENGE_H_
#define CHALLENGE_H_

#include <stdint.h>

void roll_challenge(void);
uint32_t get_challenge(void);
void handle_get_challenge(void);

#endif  // CHALLENGE_H_

#endif  // HAVE_DOMAIN_NAME
