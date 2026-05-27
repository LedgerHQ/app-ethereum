#include "fuzz_tlv_config.h"

#include "mocks.h"
#include "fuzz_command_registry.h"
#include "scenario_layout.h"

#define FUZZ_PREFIX_SIZE_FALLBACK SCEN_PREFIX_SIZE
#define FUZZ_CTRL_OFF             SCEN_CTRL_OFF
#define FUZZ_CTRL_LEN             SCEN_CTRL_LEN
#define fuzz_lane_is_structured(data, ps) \
    ((ps) > FUZZ_CTRL_OFF && (data)[FUZZ_CTRL_OFF] > FUZZ_STRUCTURED_LANE_THRESHOLD)

#include "fuzz_mutator.h"
#include "fuzz_layout_check.h"
#include "tlv_mutator.h"

static const tlv_tag_info_t TAGS_TRUSTED_NAME[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x10, 3, 3},
    {0x12, 1, 4},
    {0x13, 1, 2},
    {0x14, 1, 2},
    {0x20, 1, 30},
    {0x21, 1, 4},
    {0x22, 20, 20},
    {0x23, 1, 8},
    {0x70, 1, 1},
    {0x71, 1, 1},
    {0x72, 1, 32},
    {0x74, 20, 20},
    {0x75, 9, 41},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_ENUM_VALUE[] = {
    {0x00, 1, 1},
    {0x01, 1, 8},
    {0x02, 20, 20},
    {0x03, 0, 4},
    {0x04, 1, 1},
    {0x05, 1, 1},
    {0x06, 1, 20},
    {0xff, 8, 72},
};

static const tlv_tag_info_t TAGS_GATING[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x22, 20, 20},
    {0x23, 1, 8},
    {0x40, 0, 28},
    {0x82, 0, 100},
    {0x83, 0, 30},
    {0x84, 1, 1},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_TX_SIMULATION[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x22, 20, 20},
    {0x23, 1, 8},
    {0x27, 0, 32},
    {0x28, 0, 32},
    {0x80, 1, 1},
    {0x81, 1, 1},
    {0x82, 0, 25},
    {0x83, 0, 30},
    {0x84, 1, 1},
    {0x85, 1, 64},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_PROXY_INFO[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x12, 1, 4},
    {0x22, 20, 20},
    {0x23, 1, 8},
    {0x41, 0, 4},
    {0x42, 20, 20},
    {0x43, 1, 1},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_NETWORK_INFO[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x51, 1, 1},
    {0x23, 1, 8},
    {0x52, 0, 31},
    {0x24, 0, 50},
    {0x53, 0, 32},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_SAFE_DESCRIPTOR[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x12, 1, 4},
    {0x22, 20, 20},
    {0xa0, 1, 2},
    {0xa1, 1, 2},
    {0xa2, 1, 1},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_GTP_TX_INFO[] = {
    {0x00, 1, 1},
    {0x01, 1, 8},
    {0x02, 20, 20},
    {0x03, 0, 4},
    {0x04, 0, 32},
    {0x05, 0, 31},
    {0x06, 0, 23},
    {0x07, 0, 31},
    {0x08, 0, 27},
    {0x09, 0, 31},
    {0x0a, 0, 4},
    {0xff, 0, 72},
};

static const tlv_tag_info_t TAGS_GTP_FIELD[] = {
    {0x00, 1, 1},
    {0x01, 1, 20},
    {0x02, 1, 1},
    {0x03, 1, 64},
    {0x04, 1, 1},
    {0x05, 1, 64},
};

static const tlv_tag_info_t TAGS_AUTH_7702[] = {
    {0x00, 1, 1},
    {0x01, 20, 20},
    {0x02, 1, 8},
    {0x03, 1, 8},
};

static const tlv_tag_info_t TAGS_MAP_ENTRY[] = {
    {0x00, 1, 1},
    {0x01, 1, 8},
    {0x02, 20, 20},
    {0x03, 4, 4},
    {0x04, 1, 1},
    {0x05, 1, 32},
    {0x06, 1, 32},
    {0xff, 70, 72},
};

#define TLV_CFG(arr) \
    { .tags_info = (arr), .num_tags = sizeof(arr) / sizeof((arr)[0]) }

static const tlv_fuzz_config_t k_tlv_configs_by_kind[] = {
    [FUZZ_TLV_NONE] = {0},
    [FUZZ_TLV_TRUSTED_NAME] = TLV_CFG(TAGS_TRUSTED_NAME),
    [FUZZ_TLV_ENUM_VALUE] = TLV_CFG(TAGS_ENUM_VALUE),
    [FUZZ_TLV_GATING] = TLV_CFG(TAGS_GATING),
    [FUZZ_TLV_TX_SIMULATION] = TLV_CFG(TAGS_TX_SIMULATION),
    [FUZZ_TLV_PROXY_INFO] = TLV_CFG(TAGS_PROXY_INFO),
    [FUZZ_TLV_NETWORK_INFO] = TLV_CFG(TAGS_NETWORK_INFO),
    [FUZZ_TLV_SAFE_DESCRIPTOR] = TLV_CFG(TAGS_SAFE_DESCRIPTOR),
    [FUZZ_TLV_GTP_TX_INFO] = TLV_CFG(TAGS_GTP_TX_INFO),
    [FUZZ_TLV_GTP_FIELD] = TLV_CFG(TAGS_GTP_FIELD),
    [FUZZ_TLV_AUTH_7702] = TLV_CFG(TAGS_AUTH_7702),
    [FUZZ_TLV_MAP_ENTRY] = TLV_CFG(TAGS_MAP_ENTRY),
};

#define _TLV_INLINE_NONE \
    { .tags_info = NULL, .num_tags = 0 }
#define _TLV_INLINE_TRUSTED_NAME    TLV_CFG(TAGS_TRUSTED_NAME)
#define _TLV_INLINE_ENUM_VALUE      TLV_CFG(TAGS_ENUM_VALUE)
#define _TLV_INLINE_GATING          TLV_CFG(TAGS_GATING)
#define _TLV_INLINE_TX_SIMULATION   TLV_CFG(TAGS_TX_SIMULATION)
#define _TLV_INLINE_PROXY_INFO      TLV_CFG(TAGS_PROXY_INFO)
#define _TLV_INLINE_NETWORK_INFO    TLV_CFG(TAGS_NETWORK_INFO)
#define _TLV_INLINE_SAFE_DESCRIPTOR TLV_CFG(TAGS_SAFE_DESCRIPTOR)
#define _TLV_INLINE_GTP_TX_INFO     TLV_CFG(TAGS_GTP_TX_INFO)
#define _TLV_INLINE_GTP_FIELD       TLV_CFG(TAGS_GTP_FIELD)
#define _TLV_INLINE_AUTH_7702       TLV_CFG(TAGS_AUTH_7702)
#define _TLV_INLINE_MAP_ENTRY       TLV_CFG(TAGS_MAP_ENTRY)

#define FUZZ_COMMAND(group,              \
                     ins_symbol,         \
                     p1_max_value,       \
                     p2_max_value,       \
                     flags_value,        \
                     payload_kind_value, \
                     tlv_kind_value,     \
                     allow_empty_value)  \
    _TLV_INLINE_##tlv_kind_value,
static const tlv_fuzz_config_t k_command_tlv_configs[] = {
#include "fuzz_command_registry.inc"
};
#undef FUZZ_COMMAND

size_t fuzz_ethereum_custom_mutator(uint8_t *data,
                                    size_t size,
                                    size_t max_size,
                                    unsigned int seed) {
    return fuzz_tlv_dispatch_mutate(data,
                                    size,
                                    max_size,
                                    seed,
                                    k_command_tlv_configs,
                                    fuzz_n_commands);
}
