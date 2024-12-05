#pragma once

#ifdef HAVE_WEB3_CHECKS

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"
#include "nbgl_use_case.h"

#define HASH_SIZE    32
#define MSG_SIZE     25
#define URL_SIZE     30
#define PARTNER_SIZE 20

// clang-format off
typedef enum {
    RISK_UNKNOWN,
    RISK_BENIGN,
    RISK_WARNING,
    RISK_MALICIOUS
} tx_simulation_score_t;
// clang-format on

typedef enum {
    SIMU_TYPE_UNKNOWN,
    SIMU_TYPE_TRANSACTION,
    SIMU_TYPE_TYPED_DATA,
    SIMU_TYPE_PERSONAL_MESSAGE
} tx_simulation_type_t;

typedef struct tx_simu_s {
    uint64_t chain_id;
    const char tx_hash[HASH_SIZE];
    const char domain_hash[HASH_SIZE];
    const char provider_msg[MSG_SIZE];
    const char tiny_url[URL_SIZE];
    const char addr[ADDRESS_LENGTH];
    const char partner[PARTNER_SIZE];
    tx_simulation_score_t risk;
    tx_simulation_type_t type;
    uint8_t category;
} tx_simulation_t;

// Global structure to store the tx simultion parameters
extern tx_simulation_t TX_SIMULATION;

uint16_t handleTxSimulation(uint8_t p1, const uint8_t *data, uint8_t length, unsigned int *flags);
void handleTxSimulationOptIn(bool response_expected);
void ui_tx_simulation_error(nbgl_choiceCallback_t callback);
void ui_tx_simulation_opt_in(bool response_expected);

void clearTxSimulation(void);
bool checkTxSimulationParams(bool checkTxHash, bool checkFromAddr);
void setTxSimuWarning(nbgl_warning_t *p_warning, bool checkTxHash, bool checkFromAddr);

const char *getTxSimuRiskStr(void);
const char *getTxSimuCategoryStr(void);

#endif  // HAVE_WEB3_CHECKS

const char *ui_tx_simulation_finish_str(void);
