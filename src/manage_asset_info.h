#include "shared_context.h"
#include "common_utils.h"
#include "asset_info.h"

void forget_known_assets(void);
extraInfo_t *get_asset_info_by_addr(const uint8_t *contractAddress);
extraInfo_t *get_current_asset_info(void);
void validate_current_asset_info(void);
