#include "fuzz_command_registry.h"

#define FUZZ_COMMAND(group_name,         \
                     ins_symbol,         \
                     p1_max_value,       \
                     p2_max_value,       \
                     flags_value,        \
                     payload_kind_value, \
                     tlv_kind_value,     \
                     allow_empty_value)  \
    {                                    \
        .cla = CLA,                      \
        .ins = ins_symbol,               \
        .p1_max = p1_max_value,          \
        .p2_max = p2_max_value,          \
        .flags = flags_value,            \
    },
const fuzz_command_spec_t fuzz_commands[] = {
#include "fuzz_command_registry.inc"
};
#undef FUZZ_COMMAND

#define FUZZ_COMMAND(group_name,                           \
                     ins_symbol,                           \
                     p1_max_value,                         \
                     p2_max_value,                         \
                     flags_value,                          \
                     payload_kind_value,                   \
                     tlv_kind_value,                       \
                     allow_empty_value)                    \
    {                                                      \
        .group = FUZZ_COMMAND_GROUP_##group_name,          \
        .name = #ins_symbol,                               \
        .ins = ins_symbol,                                 \
        .payload_kind = FUZZ_PAYLOAD_##payload_kind_value, \
        .tlv_kind = FUZZ_TLV_##tlv_kind_value,             \
        .allow_empty_data = allow_empty_value,             \
    },
const fuzz_command_meta_t fuzz_command_metas[] = {
#include "fuzz_command_registry.inc"
};
#undef FUZZ_COMMAND

const size_t fuzz_n_commands = sizeof(fuzz_commands) / sizeof(fuzz_commands[0]);

_Static_assert(sizeof(fuzz_commands) / sizeof(fuzz_commands[0]) ==
                   sizeof(fuzz_command_metas) / sizeof(fuzz_command_metas[0]),
               "fuzz command registry arrays must stay aligned");

const fuzz_command_meta_t *fuzz_command_meta_by_ins(uint8_t ins) {
    for (size_t i = 0; i < fuzz_n_commands; ++i) {
        if (fuzz_command_metas[i].ins == ins) {
            return &fuzz_command_metas[i];
        }
    }
    return NULL;
}

bool fuzz_command_is_extension_ins(uint8_t ins) {
    const fuzz_command_meta_t *meta = fuzz_command_meta_by_ins(ins);

    return (meta != NULL) && (meta->group == FUZZ_COMMAND_GROUP_EXTENSION);
}
