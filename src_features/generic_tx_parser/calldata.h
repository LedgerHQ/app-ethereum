#ifndef CALLDATA_H_
#define CALLDATA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CALLDATA_SELECTOR_SIZE 4
#define CALLDATA_CHUNK_SIZE    32

bool calldata_init(size_t size);
bool calldata_append(const uint8_t *buffer, size_t size);
void calldata_cleanup(void);
const uint8_t *calldata_get_selector(void);
const uint8_t *calldata_get_chunk(int idx);

#endif  // !CALLDATA_H_
