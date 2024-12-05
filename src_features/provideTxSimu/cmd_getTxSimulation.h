#pragma once

#include <stdint.h>
#include <stdbool.h>

#define HASH_SIZE 32
#define MSG_SIZE  30
#define URL_SIZE  30

// clang-format off
typedef enum {
    SCORE_UNKNOWN,
    SCORE_BENIGN,
    SCORE_WARNING,
    SCORE_MALICIOUS,
    SCORE_NB_MAX
} tx_simulation_score_t;
// clang-format on

typedef struct tx_simu_s {
    uint64_t chain_id;
    const char tx_hash[HASH_SIZE];
    uint16_t risk;
    tx_simulation_score_t score;
    uint8_t category;
    const char provider_msg[MSG_SIZE];
    const char tiny_url[URL_SIZE];
} tx_simulation_t;

// Global structure to store the tx simultion parameters
extern tx_simulation_t TX_SIMULATION;

uint16_t handleTxSimulation(uint8_t p1, const uint8_t *data, uint8_t length);
void clearTxSimulation(void);
void checkTxSimulationParams(void);

const char *getTxSimuScoreStr(void);
const char *getTxSimuCategoryStr(void);

void ui_display_tx_simulation(bool is_demo);
