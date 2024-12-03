#ifndef CMD_TX_INFO_H_
#define CMD_TX_INFO_H_

#include <stdint.h>
#include "gtp_tx_info.h"

 bool verify_struct(const s_tx_info_ctx *context);
uint16_t handle_tx_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload);
void gcs_cleanup(void);

#endif  // !CMD_TX_INFO_H_
