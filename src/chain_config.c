#include <string.h>
#include "chain_config.h"

void init_chain_config(chain_config_t *chain_config) {
    explicit_bzero(chain_config, sizeof(*chain_config));
    strlcpy(chain_config->ticker, APP_TICKER, sizeof(chain_config->ticker));
    chain_config->chain_id = APP_CHAIN_ID;
    chain_config->coin_type = APP_COIN_TYPE;
}
