#ifndef MANAGE_ASSET_INFO_H_
#define MANAGE_ASSET_INFO_H_

#include "shared_context.h"
#include "common_utils.h"
#include "asset_info.h"

void forget_known_assets(void);
int get_asset_index_by_addr(const uint8_t *addr);
extraInfo_t *get_asset_info_by_addr(const uint8_t *contractAddress);
extraInfo_t *get_current_asset_info(void);
void validate_current_asset_info(void);

#endif  // MANAGE_ASSET_INFO_H_
