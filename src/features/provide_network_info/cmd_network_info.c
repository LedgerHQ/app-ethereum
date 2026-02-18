#include <stdbool.h>
#include "cmd_network_info.h"
#include "apdu_constants.h"
#include "network_info.h"
#include "network_icon.h"
#include "write.h"
#include "tlv_apdu.h"
#include "mem_utils.h"
#include "ui_utils.h"

#define P2_NETWORK_CONFIG 0x00
#define P2_NETWORK_ICON   0x01
#define P2_GET_INFO       0x02

/**
 * @brief Returns the current network configuration.
 *
 * @return APDU length
 */
static uint16_t handle_get_config(void) {
    uint8_t chain_str[sizeof(uint64_t) * 2 + 1] = {0};
    uint32_t tx = 1;  // Init to '1' because there is at least the number of networks
    uint16_t nb_networks = 0;

    // Iterate over the linked list
    flist_node_t *node = g_dynamic_network_list;
    while (node != NULL) {
        network_info_t *net_info = (network_info_t *) node;
        if (net_info->chain_id != 0) {
            PRINTF("[NETWORK] - Found dynamic '%s'\n", net_info->name);
            // Convert chain_id
            explicit_bzero(chain_str, sizeof(chain_str));
            write_u64_be(chain_str, 0, net_info->chain_id);
            memmove(G_io_apdu_buffer + tx, chain_str, sizeof(uint64_t));
            tx += sizeof(uint64_t);
            nb_networks++;
        }
        node = node->next;
    }
    G_io_apdu_buffer[0] = nb_networks;

    return tx;
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
    uint16_t sw = SWO_PARAMETER_ERROR_NO_INFO;
    const buffer_t buf = {.ptr = (uint8_t *) data, .size = length, .offset = 0};

    switch (p2) {
        case P2_NETWORK_CONFIG:
            if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, length, data, &handle_network_tlv_payload)) {
                // If there was an error, free the allocated memory
                network_info_cleanup(g_last_added_network);
                sw = SWO_INCORRECT_DATA;
                break;
            }
            sw = SWO_SUCCESS;
            break;

        case P2_NETWORK_ICON:
            sw = handle_network_icon_chunks(p1, &buf);
            if (sw != SWO_SUCCESS) {
                // If there was an error, free the allocated memory
                network_info_cleanup(g_last_added_network);
            }
            break;

        case P2_GET_INFO:
            if (p1 != 0x00) {
                PRINTF("Error: Unexpected P1 (%u)!\n", p1);
                // If there was an error, free the allocated memory
                network_info_cleanup(g_last_added_network);
                sw = SWO_WRONG_P1_P2;
                break;
            }
            *tx = handle_get_config();
            sw = SWO_SUCCESS;
            break;
        default:
            network_info_cleanup(g_last_added_network);
            sw = SWO_WRONG_P1_P2;
            break;
    }

    return sw;
}
