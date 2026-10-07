# tests_low_level.cmake

ledger_unit_tests_add_test(NAME test_uint128
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${APP_DIR}/utils/utils.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_uint256
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/uint256.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/utils/utils.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_utils
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_rlp_utils
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/features/sign_tx/rlp_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_tx
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_rlp_encode
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/features/sign_authorization_eip7702/rlp_encode.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_authorization_eip7702
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_tlv_apdu
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/mem_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_tlv_utils
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_path_array
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/features/generic_tx_parser/gtp_path_array.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_path_slice
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/features/generic_tx_parser/gtp_path_slice.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_calldata
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/features/generic_tx_parser/calldata.c
    ${APP_DIR}/utils/mem_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_token_info
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/features/provide_erc20_token_information/token_info.c
    ${APP_DIR}/utils/mem_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_hash_bytes
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/utils/hash_bytes.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_time_format
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/utils/time_format.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_manage_asset_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/manage_asset_info.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_erc20_token_information
    ${APP_DIR}/features/provide_nft_information
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)
