#include <stdbool.h>
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
// Temporary buffer to store the icon bitmap during reception
static uint8_t *g_icon_bitmap = NULL;

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
#ifdef TARGET_APEX
    expected_px = 48;
#else
    expected_px = 64;
#endif
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
    if (g_last_added_network == NULL) {
        return;
    }
    PRINTF("****************************************************************************\n");
    PRINTF("[NETWORK_ICON] - Registered icon %dx%d (BPP %d) for network '%s'\n",
           g_last_added_network->icon.width,
           g_last_added_network->icon.height,
           g_last_added_network->icon.bpp,
           g_last_added_network->name);
}

/**
 * @brief Parse and check the NETWORK_ICON value.
 *
 * @return whether it was successful or not
 */
static bool parse_icon_buffer(void) {
    uint16_t img_len = 0;
    uint8_t digest[CX_SHA256_SIZE] = {0};
    const uint16_t field_len = g_icon_payload.received_size;

    if (g_last_added_network == NULL) {
        PRINTF("Error: No network to associate icon with!\n");
        return false;
    }

    // Check the icon header
    if (!check_icon_header(g_icon_bitmap, field_len, &img_len)) {
        return false;
    }
    if (field_len != img_len) {
        return false;
    }

    if (field_len > g_icon_payload.expected_size) {
        return false;
    }

    // Check icon hash
    if (cx_sha256_hash(g_icon_bitmap, field_len, digest) != CX_OK) {
        return false;
    }
    if (memcmp(digest, g_network_icon_hash, CX_SHA256_SIZE) != 0) {
        PRINTF("NETWORK_ICON hash mismatch!\n");
        return false;
    }
    // Free the icon bitmap hash buffer
    mem_buffer_cleanup((void **) &g_network_icon_hash);

    // Transfer the icon bitmap buffer to the network (ownership transfer)
    // The network takes ownership of the persistent buffer
    g_last_added_network->icon.bitmap = (const uint8_t *) g_icon_bitmap;
    g_last_added_network->icon.width = U2LE(g_icon_bitmap, 0);
    g_last_added_network->icon.height = U2LE(g_icon_bitmap, 2);
    // BPP is stored in the upper 4 bits of the 5th byte
    g_last_added_network->icon.bpp = g_icon_bitmap[4] >> 4;
    g_last_added_network->icon.isFile = true;

    // Reset temporary pointer (ownership transferred, don't free it!)
    g_icon_bitmap = NULL;
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
static uint16_t handle_first_icon_chunk(const buffer_t *buf) {
    uint16_t img_len = 0;

    // Reset the structures
    explicit_bzero(&g_icon_payload, sizeof(g_icon_payload));

    // Check the icon header
    if (!check_icon_header(buf->ptr, buf->size, &img_len)) {
        return SWO_INCORRECT_DATA;
    }

    // Do not track the allocation in logs, because this buffer is expected to stay allocated
    if (mem_buffer_persistent((void **) &g_icon_bitmap, img_len) == false) {
        PRINTF("Error: Not enough memory for icon bitmap!\n");
        return SWO_INSUFFICIENT_MEMORY;
    }
    g_icon_payload.expected_size = img_len;

    return SWO_SUCCESS;
}

/**
 * @brief Handle icon data chunk.
 *
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
static uint16_t handle_next_icon_chunk(const buffer_t *buf) {
    if ((g_icon_payload.received_size + buf->size) > g_icon_payload.expected_size) {
        PRINTF("Payload size mismatch!\n");
        return SWO_INCORRECT_DATA;
    }
    if (g_icon_bitmap == NULL) {
        PRINTF("Error: Icon bitmap not initialized!\n");
        return SWO_INCORRECT_DATA;
    }
    // Feed into payload
    memcpy(g_icon_bitmap + g_icon_payload.received_size, buf->ptr, buf->size);
    g_icon_payload.received_size += buf->size;

    return SWO_SUCCESS;
}

/**
 * @brief Handle icon chunks.
 *
 * @param[in] p1 APDU parameter 1
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
uint16_t handle_network_icon_chunks(uint8_t p1, const buffer_t *buf) {
    uint16_t sw = SWO_PARAMETER_ERROR_NO_INFO;

    if (g_last_added_network == NULL) {
        PRINTF("Error: No network info received!\n");
        return SWO_INCORRECT_DATA;
    }

    if ((g_network_icon_hash == NULL) || (allzeroes(g_network_icon_hash, CX_SHA256_SIZE) == 1)) {
        PRINTF("Error: Icon hash not set!\n");
        return SWO_INCORRECT_DATA;
    }

    // Check the received chunk index
    if (p1 == P1_FIRST_CHUNK) {
        // Init the with the 1st chunk
        sw = handle_first_icon_chunk(buf);
        if (sw != SWO_SUCCESS) {
            return sw;
        }
    } else if (p1 != P1_FOLLOWING_CHUNK) {
        PRINTF("Error: Unexpected P2 (%u)!\n", p1);
        return SWO_WRONG_P1_P2;
    }

    // Handle the payload
    sw = handle_next_icon_chunk(buf);
    if (sw != SWO_SUCCESS) {
        return sw;
    }
    if (g_icon_payload.received_size == g_icon_payload.expected_size) {
        // Everything has been received
        if (!parse_icon_buffer()) {
            return SWO_INCORRECT_DATA;
        }
        print_icon_info();
    }
    return SWO_SUCCESS;
}

/**
 * @brief Clear the Icon memory.
 *
 */
void clear_icon(void) {
    mem_buffer_cleanup((void **) &g_icon_bitmap);
    mem_buffer_cleanup((void **) &g_network_icon_hash);
}
