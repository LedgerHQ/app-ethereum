#pragma once

#ifdef HAVE_TRANSACTION_CHECKS

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"
#include "os_pki.h"
#include "tlv_use_case_transaction_check.h"

_Static_assert(CERTIFICATE_TRUSTED_NAME_MAXLEN > TRANSACTION_CHECK_PARTNER_SIZE - 1,
               "Partner size is too big to get the trusted name");

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
