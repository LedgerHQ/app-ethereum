/*
 * Canonical fuzz command registry.
 *
 * Field order:
 *   group, ins_symbol, p1_max, p2_max, flags, payload_kind, tlv_kind, allow_empty_data
 *
 * Keep existing command indices stable for corpus compatibility. New production
 * APDU entries should be appended unless an ABI break and harness_version bump
 * are intentional.
 */
FUZZ_COMMAND(APDU, INS_GET_PUBLIC_KEY,                1,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_SIGN,                          0x80, 0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_GET_APP_CONFIGURATION,         0,    0,    0,                 RAW, NONE,            true)
FUZZ_COMMAND(APDU, INS_SIGN_PERSONAL_MESSAGE,         0x80, 0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_PROVIDE_ERC20_TOKEN_INFORMATION, 0,  0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_SIGN_EIP_712_MESSAGE,          1,    1,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_GET_ETH2_PUBLIC_KEY,           1,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_SET_ETH2_WITHDRAWAL_INDEX,     0,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_SET_EXTERNAL_PLUGIN,           0,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_PROVIDE_NFT_INFORMATION,       0,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_SET_PLUGIN,                    0,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_PERFORM_PRIVACY_OPERATION,     1,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_EIP712_STRUCT_DEF,             0,    0xFF, FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_EIP712_STRUCT_IMPL,            1,    0xFF, FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_EIP712_FILTERING,              1,    0xFF, FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_GET_CHALLENGE,                 0,    0,    0,                 RAW, NONE,            true)
FUZZ_COMMAND(APDU, INS_PROVIDE_TRUSTED_NAME,          1,    0,    FUZZ_CMD_HAS_DATA, TLV, TRUSTED_NAME,    false)
FUZZ_COMMAND(APDU, INS_PROVIDE_ENUM_VALUE,            1,    0,    FUZZ_CMD_HAS_DATA, TLV, ENUM_VALUE,      false)
FUZZ_COMMAND(APDU, INS_GTP_TRANSACTION_INFO,          1,    0,    FUZZ_CMD_HAS_DATA, TLV, GTP_TX_INFO,     false)
FUZZ_COMMAND(APDU, INS_GTP_FIELD,                     1,    0,    FUZZ_CMD_HAS_DATA, TLV, GTP_FIELD,       false)
FUZZ_COMMAND(APDU, INS_PROVIDE_PROXY_INFO,            1,    0,    FUZZ_CMD_HAS_DATA, TLV, PROXY_INFO,      false)
FUZZ_COMMAND(APDU, INS_PROVIDE_NETWORK_CONFIGURATION, 1,    0,    FUZZ_CMD_HAS_DATA, TLV, NETWORK_INFO,    false)
FUZZ_COMMAND(APDU, INS_PROVIDE_TX_SIMULATION,         1,    0,    FUZZ_CMD_HAS_DATA, TLV, TX_SIMULATION,   false)
FUZZ_COMMAND(APDU, INS_SIGN_EIP7702_AUTHORIZATION,    1,    0,    FUZZ_CMD_HAS_DATA, TLV, AUTH_7702,       false)
FUZZ_COMMAND(APDU, INS_PROVIDE_SAFE_ACCOUNT,          1,    1,    FUZZ_CMD_HAS_DATA, TLV, SAFE_DESCRIPTOR, false)
FUZZ_COMMAND(APDU, INS_PROVIDE_GATING,                1,    0,    FUZZ_CMD_HAS_DATA, TLV, GATING,          false)
FUZZ_COMMAND(EXTENSION, INS_FUZZ_SWAP_CHECK_ADDRESS,  0,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(EXTENSION, INS_FUZZ_SWAP_PRINTABLE_AMOUNT, 0,  0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(EXTENSION, INS_FUZZ_SWAP_COPY_TRANSACTION, 0,  0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(EXTENSION, INS_FUZZ_PLUGIN_UTILS,        0,    0,    FUZZ_CMD_HAS_DATA, RAW, NONE,            false)
FUZZ_COMMAND(APDU, INS_PROVIDE_MAP_ENTRY,             1,    0,    FUZZ_CMD_HAS_DATA, TLV, MAP_ENTRY,       false)
