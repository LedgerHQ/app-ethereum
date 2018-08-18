#include "os.h"

#ifndef CHAIN_CONFIG_H

#define CHAIN_CONFIG_H

typedef enum chain_kind_e {
	CHAIN_KIND_ETHEREUM,
	CHAIN_KIND_ETHEREUM_CLASSIC,
	CHAIN_KIND_EXPANSE,
	CHAIN_KIND_POA,
	CHAIN_KIND_RSK,
	CHAIN_KIND_UBIQ,
	CHAIN_KIND_WANCHAIN,
	CHAIN_KIND_KUSD,
	CHAIN_KIND_PIRL,
	CHAIN_KIND_AKROMA,
	CHAIN_KIND_MUSICOIN,
	CHAIN_KIND_CALLISTO,
	CHAIN_KIND_ETHERSOCIAL,
	CHAIN_KIND_ELLAISM,
	CHAIN_KIND_ETHER1,
	CHAIN_KIND_ETHERGEM,
	CHAIN_KIND_ATHEIOS,
	CHAIN_KIND_GOCHAIN,
	CHAIN_KIND_EOSCLASSIC,
	CHAIN_KIND_MIX,
	CHAIN_KIND_AQUACHAIN
} chain_kind_t;

typedef struct chain_config_s {
	const char* coinName; // ticker
	uint32_t chainId;
	chain_kind_t kind;
	
#ifdef TARGET_BLUE
    const char* header_text;
    unsigned int color_header;
    unsigned int color_dashboard;
#endif // TARGET_BLUE

} chain_config_t;

#endif

