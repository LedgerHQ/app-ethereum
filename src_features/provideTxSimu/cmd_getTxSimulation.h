#pragma once

#ifdef HAVE_WEB3_CHECKS

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"

// TODO - Check the size of the fields
#define HASH_SIZE    32
#define MSG_SIZE     55
#define URL_SIZE     40
#define PARTNER_SIZE 20

// clang-format off
typedef enum {
    SCORE_UNKNOWN,
    SCORE_BENIGN,
    SCORE_WARNING,
    SCORE_MALICIOUS
} tx_simulation_score_t;
// clang-format on

typedef struct tx_simu_s {
    uint64_t chain_id;
    const char addr[ADDRESS_LENGTH];
    const char tx_hash[HASH_SIZE];
    uint16_t risk;
    tx_simulation_score_t score;
    uint8_t category;
    const char provider_msg[MSG_SIZE];
    const char tiny_url[URL_SIZE];
    const char partner[PARTNER_SIZE];
} tx_simulation_t;

// Global structure to store the tx simultion parameters
extern tx_simulation_t TX_SIMULATION;

uint16_t handleTxSimulation(uint8_t p1, const uint8_t *data, uint8_t length);
void clearTxSimulation(void);
void checkTxSimulationParams(void);

const char *getTxSimuScoreStr(void);
const char *getTxSimuCategoryStr(void);

void ui_display_tx_simulation(bool fromPlugin, bool is_demo);

#endif  // HAVE_WEB3_CHECKS
