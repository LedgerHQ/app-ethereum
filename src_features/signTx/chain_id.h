#include <stdint.h>

#define NETWORK_STRING_MAX_SIZE

typedef struct network_info_s {
    uint32_t chain_id;
    char *name;
} network_info_t;

// Fills `dest` with the network name corresponding to `chain_id`.
// Returns a pointer to `dest` if successful, else returns `NULL`.
char *get_network_name_from_chain_id(char *dest, uint8_t dest_size, uint32_t chain_id);