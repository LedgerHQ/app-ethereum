#ifndef __STARK_CRYPTO_H__
#define __STARK_CRYPTO_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "os.h"
#include "cx.h"

/* EC points */
#define FIELD_ELEMENT_SIZE (32)
#define EC_POINT_SIZE      (2 * FIELD_ELEMENT_SIZE + 1)
typedef unsigned char FieldElement[FIELD_ELEMENT_SIZE];
typedef unsigned char ECPoint[EC_POINT_SIZE];

void pedersen(FieldElement res, /* out */
              FieldElement a,
              FieldElement b);

#endif
