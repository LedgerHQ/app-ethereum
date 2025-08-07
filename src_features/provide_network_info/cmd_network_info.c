#include <stdbool.h>
#include "cmd_network_info.h"
#include "apdu_constants.h"
#include "network_info.h"
#include "write.h"
#include "tlv_apdu.h"
#include "mem_utils.h"
#include "ui_utils.h"

#define P2_NETWORK_CONFIG 0x00
#define P2_NETWORK_ICON   0x01
#define P2_GET_INFO       0x02

typedef struct {
    uint16_t received_size;
    uint16_t expected_size;
} network_payload_t;

// Global structure to temporary store the network icon APDU
static network_payload_t g_icon_payload = {0};
// Global structure to store the icons bitmap
static uint8_t *g_icon_bitmap[MAX_DYNAMIC_NETWORKS] = {0};

/**
 * @brief Check the NETWORK_ICON header.
 *
 * @param[in] data buffer received
 * @param[in] length Length of the field value
 * @return APDU Response code
 */
static bool check_icon_header(const uint8_t *data, uint16_t length, uint16_t *buffer_size) {
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t expected_px = 0;

    // The chunk starts by the Image Header (8 Bytes):
    //   - Width (2 Bytes)
    //   - Height (2 Bytes)
    //   - BPP (1 Byte)
    //   - Img buffer size (3 Bytes)
    if (length < 8) {
        PRINTF("NETWORK_ICON header length mismatch (%d)!\n", length);
        return false;
    }
    width = U2LE(data, 0);
    height = U2LE(data, 2);
#ifdef SCREEN_SIZE_WALLET
    expected_px = 64;
#else
    expected_px = 14;
#endif
    if ((width != expected_px) || (height != expected_px)) {
        PRINTF("NETWORK_ICON size mismatch (%dx%d)!\n", width, height);
        return false;
    }
    *buffer_size = 8 + data[5] + (data[6] << 8) + (data[7] << 16);
    return true;
}

/**
 * @brief Print the registered network icon.
 *
 * Only for debug purpose.
 *
 */
static void print_icon_info(void) {
    PRINTF("****************************************************************************\n");
    PRINTF("[NETWORK_ICON] - Registered in slot %d: icon %dx%d (BPP %d)\n",
           g_current_network_slot,
           DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.width,
           DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.height,
           DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.bpp);
}

/**
 * @brief Parse and check the NETWORK_ICON value.
 *
 * @return whether it was successful or not
 */
static bool parse_icon_buffer(void) {
    uint16_t img_len = 0;
    uint8_t digest[CX_SHA256_SIZE];
    const uint8_t *data = g_icon_bitmap[g_current_network_slot];
    const uint16_t field_len = g_icon_payload.received_size;

    // Check the icon header
    if (!check_icon_header(data, field_len, &img_len)) {
        return false;
    }
    if (field_len != img_len) {
        return false;
    }

    if (field_len > g_icon_payload.expected_size) {
        return false;
    }

    // Check icon hash
    if (cx_sha256_hash(data, field_len, digest) != CX_OK) {
        return false;
    }
    if (memcmp(digest, g_network_icon_hash, CX_SHA256_SIZE) != 0) {
        PRINTF("NETWORK_ICON hash mismatch!\n");
        return false;
    }
    // Free the icon bitmap hash buffer
    mem_buffer_cleanup((void **) &g_network_icon_hash);

    DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.bitmap =
        (const uint8_t *) g_icon_bitmap[g_current_network_slot];
    DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.width = U2LE(data, 0);
    DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.height = U2LE(data, 2);
    // BPP is stored in the upper 4 bits of the 5th byte
    DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.bpp = data[4] >> 4;
    DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.isFile = true;
    memcpy((uint8_t *) DYNAMIC_NETWORK_INFO[g_current_network_slot]->icon.bitmap, data, field_len);
    print_icon_info();
    return true;
}

/**
 * @brief Init the dynamic network icon with the 1st chunk.
 *
 * Analyze the 1st chunk, containing the icon size
 *
 * @param[in] data buffer received, skip payload length
 * @param[in] length of the buffer, reduced by the payload length
 * @return APDU Response code
 */
static uint16_t handle_first_icon_chunk(const uint8_t *data, uint8_t length) {
    uint16_t img_len = 0;

    // Reset the structures
    explicit_bzero(&g_icon_payload, sizeof(g_icon_payload));

    // Check the icon header
    if (!check_icon_header(data, length, &img_len)) {
        return APDU_RESPONSE_INVALID_DATA;
    }

    if (mem_buffer_allocate((void **) &(g_icon_bitmap[g_current_network_slot]), img_len) == false) {
        PRINTF("Error: Not enough memory for icon bitmap!\n");
        return APDU_RESPONSE_INSUFFICIENT_MEMORY;
    }
    g_icon_payload.expected_size = img_len;

    return APDU_RESPONSE_OK;
}

/**
 * @brief Handle icon data chunk.
 *
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
static uint16_t handle_next_icon_chunk(const uint8_t *data, uint8_t length) {
    if ((g_icon_payload.received_size + length) > g_icon_payload.expected_size) {
        PRINTF("Payload size mismatch!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (g_icon_bitmap[g_current_network_slot] == NULL) {
        PRINTF("Error: Icon bitmap not initialized!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    // Feed into payload
    memcpy(g_icon_bitmap[g_current_network_slot] + g_icon_payload.received_size, data, length);
    g_icon_payload.received_size += length;

    return APDU_RESPONSE_OK;
}

/**
 * @brief Handle icon chunks.
 *
 * @param[in] p1 APDU parameter 1
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
static uint16_t handle_icon_chunks(uint8_t p1, const uint8_t *data, uint8_t length) {
    uint16_t sw;
    uint8_t hash[CX_SHA256_SIZE] = {0};

    if (DYNAMIC_NETWORK_INFO[g_current_network_slot] == NULL) {
        PRINTF("Error: No network info received!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }

    if ((g_network_icon_hash == NULL) || (memcmp(g_network_icon_hash, hash, CX_SHA256_SIZE) == 0)) {
        PRINTF("Error: Icon hash not set!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }

    // Check the received chunk index
    if (p1 == P1_FIRST_CHUNK) {
        // Init the with the 1st chunk
        sw = handle_first_icon_chunk(data, length);
        if (sw != APDU_RESPONSE_OK) {
            return sw;
        }
    } else if (p1 != P1_FOLLOWING_CHUNK) {
        PRINTF("Error: Unexpected P2 (%u)!\n", p1);
        return APDU_RESPONSE_INVALID_P1_P2;
    }

    // Handle the payload
    sw = handle_next_icon_chunk(data, length);
    if (sw != APDU_RESPONSE_OK) {
        return sw;
    }
    if (g_icon_payload.received_size == g_icon_payload.expected_size) {
        // Everything has been received
        if (!parse_icon_buffer()) {
            return APDU_RESPONSE_INVALID_DATA;
        }
        // Prepare for next slot
        g_current_network_slot = (g_current_network_slot + 1) % MAX_DYNAMIC_NETWORKS;
    }
    return APDU_RESPONSE_OK;
}

/**
 * @brief Print the registered network.
 *
 * Only for debug purpose.
 */
static void print_network_info(void) {
    char chain_str[sizeof(uint64_t) * 2 + 1] = {0};

    if (DYNAMIC_NETWORK_INFO[g_current_network_slot] == NULL) {
        return;  // Nothing to print if no network is registered
    }
    PRINTF("****************************************************************************\n");
    u64_to_string(DYNAMIC_NETWORK_INFO[g_current_network_slot]->chain_id,
                  chain_str,
                  sizeof(chain_str));
    PRINTF("[NETWORK] - Registered in slot %u: \"%s\" (%s), for chain_id %s\n",
           g_current_network_slot,
           DYNAMIC_NETWORK_INFO[g_current_network_slot]->name,
           DYNAMIC_NETWORK_INFO[g_current_network_slot]->ticker,
           chain_str);
}

/**
 * @brief Returns the current network configuration.
 *
 * @return APDU length
 */
static uint16_t handle_get_config(void) {
    uint8_t chain_str[sizeof(uint64_t) * 2 + 1];
    uint32_t tx = 1;  // Init to '1' because there is at least the number of networks
    uint16_t nb_networks = 0;

    for (size_t i = 0; i < MAX_DYNAMIC_NETWORKS; i++) {
        if ((DYNAMIC_NETWORK_INFO[i]) && (DYNAMIC_NETWORK_INFO[i]->chain_id != 0)) {
            PRINTF("[NETWORK] - Found dynamic %s\n", DYNAMIC_NETWORK_INFO[i]->name);
            // Convert chain_id
            explicit_bzero(chain_str, sizeof(chain_str));
            write_u64_be(chain_str, 0, DYNAMIC_NETWORK_INFO[i]->chain_id);
            memmove(G_io_apdu_buffer + tx, chain_str, sizeof(uint64_t));
            tx += sizeof(uint64_t);
            nb_networks++;
        }
    }
    G_io_apdu_buffer[0] = nb_networks;

    return tx;
}

/**
 * @brief Handle Network Configuration TLV.
 *
 * @param[in] payload TLV buffer received
 * @param[in] size of the buffer
 * @return whether it was successful or not
 */
static bool handle_tlv_payload(const uint8_t *payload, uint16_t size) {
    bool parsing_ret;
    s_network_info_ctx ctx = {0};

    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);
    parsing_ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_network_info_struct, &ctx);
    if (!parsing_ret || !verify_network_info_struct(&ctx)) {
        return false;
    }
    print_network_info();
    return true;
}

/**
 * @brief Handle Network Configuration APDU.
 *
 * @param[in] p1 APDU parameter 1
 * @param[in] p2 APDU parameter 2
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @param[in] tx output length
 * @return APDU Response code
 */
uint16_t handle_network_info(uint8_t p1,
                             uint8_t p2,
                             const uint8_t *data,
                             uint8_t length,
                             unsigned int *tx) {
    uint16_t sw = APDU_RESPONSE_UNKNOWN;

    switch (p2) {
        case P2_NETWORK_CONFIG:
            if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, length, data, &handle_tlv_payload)) {
                // If there was an error, free the allocated memory
                network_info_cleanup(g_current_network_slot);
                sw = APDU_RESPONSE_INVALID_DATA;
                break;
            }
            sw = APDU_RESPONSE_OK;
            break;

        case P2_NETWORK_ICON:
            sw = handle_icon_chunks(p1, data, length);
            if (sw != APDU_RESPONSE_OK) {
                // If there was an error, free the allocated memory
                network_info_cleanup(g_current_network_slot);
            }
            break;

        case P2_GET_INFO:
            if (p1 != 0x00) {
                PRINTF("Error: Unexpected P1 (%u)!\n", p1);
                // If there was an error, free the allocated memory
                network_info_cleanup(g_current_network_slot);
                sw = APDU_RESPONSE_INVALID_P1_P2;
                break;
            }
            *tx = handle_get_config();
            sw = APDU_RESPONSE_OK;
            break;
        default:
            network_info_cleanup(g_current_network_slot);
            sw = APDU_RESPONSE_INVALID_P1_P2;
            break;
    }

    if ((sw != APDU_RESPONSE_OK) ||
        (g_icon_payload.received_size == g_icon_payload.expected_size)) {
        explicit_bzero(&g_icon_payload, sizeof(g_icon_payload));
    }

    return sw;
}

/**
 * @brief Cleanup Network Configuration context.
 *
 */
void network_info_cleanup(uint8_t slot) {
    if (slot == MAX_DYNAMIC_NETWORKS) {
        // Cleanup all slots
        for (slot = 0; slot < MAX_DYNAMIC_NETWORKS; slot++) {
            mem_buffer_cleanup((void **) &(g_icon_bitmap[slot]));
            mem_buffer_cleanup((void **) &(DYNAMIC_NETWORK_INFO[slot]));
        }
    } else {
        // Cleanup only the specified slot
        mem_buffer_cleanup((void **) &(g_icon_bitmap[slot]));
        mem_buffer_cleanup((void **) &(DYNAMIC_NETWORK_INFO[slot]));
    }
    // Free the icon hash if it was allocated
    mem_buffer_cleanup((void **) &g_network_icon_hash);
}
