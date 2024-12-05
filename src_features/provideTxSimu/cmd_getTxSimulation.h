#pragma once

#include <stdint.h>

#define HASH_SIZE 32
#define MSG_SIZE  30
#define URL_SIZE  30

// clang-format off
typedef enum {
    RISK_BENIGN,
    RISK_WARNING,
    RISK_MALICIOUS,
    RISK_NB_MAX
} tx_simulation_risk_t;
// clang-format on

typedef struct tx_simu_s {
    uint64_t chain_id;
    const char tx_hash[HASH_SIZE];
    uint16_t risk;
    uint8_t category;
    const char provider_msg[MSG_SIZE];
    const char tiny_url[URL_SIZE];
} tx_simulation_t;

// Global structure to store the tx simultion parameters
extern tx_simulation_t TX_SIMULATION;

uint16_t handleTxSimulation(const uint8_t *data, uint8_t length);

tx_simulation_risk_t getTxSimuRiskScore(void);
const char *getTxSimuRiskScorStr(void);
const char *getTxSimuRiskcategory(void);
