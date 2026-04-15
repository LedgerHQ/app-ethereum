#pragma once

#ifdef HAVE_TRANSACTION_CHECKS

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"
#include "os_pki.h"

#define HASH_SIZE    32
#define MSG_SIZE     25
#define URL_SIZE     30
#define PARTNER_SIZE 20

// clang-format off
typedef enum {
    TX_SIMULATION_RISK_BENIGN = 0x00,
    TX_SIMULATION_RISK_WARNING = 0x01,
    TX_SIMULATION_RISK_MALICIOUS = 0x02,
    // Internal level for error handling, not API entry point
    TX_SIMULATION_RISK_UNKNOWN,
} tx_simulation_score_t;
// clang-format on

typedef enum {
    TX_SIMULATION_TYPE_TRANSACTION = 0x00,
    TX_SIMULATION_TYPE_TYPED_DATA = 0x01,
    TX_SIMULATION_TYPE_COUNT,
} tx_simulation_type_t;

typedef enum {
    TX_SIMULATION_CATEGORY_NA = 0x00,
    TX_SIMULATION_CATEGORY_OTHERS = 0x01,
    TX_SIMULATION_CATEGORY_ADDRESS = 0x02,
    TX_SIMULATION_CATEGORY_DAPP = 0x03,
    TX_SIMULATION_CATEGORY_LOSING_OPERATION = 0x04,
    TX_SIMULATION_CATEGORY_COUNT,
} tx_simulation_category_t;

typedef struct tx_simu_s {
    uint64_t chain_id;
    const uint8_t tx_hash[HASH_SIZE];
    const uint8_t domain_hash[HASH_SIZE];
    const char provider_msg[MSG_SIZE + 1];  // +1 for the null terminator
    const char tiny_url[URL_SIZE + 1];      // +1 for the null terminator
    const char address[ADDRESS_LENGTH];
    const char partner[PARTNER_SIZE];
    tx_simulation_score_t risk;
    tx_simulation_type_t type;
    tx_simulation_category_t category;
} tx_simulation_t;

_Static_assert(CERTIFICATE_TRUSTED_NAME_MAXLEN > PARTNER_SIZE - 1,
               "Partner size is too big to get the trusted name");

// Global structure to store the tx simultion parameters
extern tx_simulation_t TX_SIMULATION;

uint16_t handle_tx_simulation(uint8_t p1,
                              uint8_t p2,
                              const uint8_t *data,
                              uint8_t length,
                              unsigned int *flags);
void handle_tx_simulation_opt_in(bool response_expected);
void ui_tx_simulation_opt_in(bool response_expected);

void clear_tx_simulation(void);
void set_tx_simulation_warning(void);

const char *get_tx_simulation_risk_str(void);
const char *get_tx_simulation_category_str(void);

#endif  // HAVE_TRANSACTION_CHECKS

const char *ui_tx_simulation_finish_str(void);
