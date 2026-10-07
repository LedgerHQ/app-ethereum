# tests_sign.cmake

ledger_unit_tests_add_test(NAME test_eth_ustream_helpers
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/sign_tx/eth_ustream.c
    ${APP_DIR}/features/sign_tx/rlp_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_tx
    ${APP_DIR}/features/generic_tx_parser
    ${APP_DIR}/features/provide_network_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_eth_ustream_typed
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/sign_tx/eth_ustream.c
    ${APP_DIR}/features/sign_tx/rlp_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_tx
    ${APP_DIR}/features/generic_tx_parser
    ${APP_DIR}/features/provide_network_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_eth_swap_utils
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/swap/eth_swap_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/swap
    ${APP_DIR}/features/sign_tx
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    CUSTOM_IO_APDU_BUFFER_SIZE=272
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_logic_sign_tx_fee
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/sign_tx/logic_sign_tx.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_tx
    ${APP_DIR}/features/get_public_key
    ${APP_DIR}/features/generic_tx_parser
    ${APP_DIR}/features/provide_network_info
    ${APP_DIR}/features/provide_proxy_info
    ${APP_DIR}/plugins
    ${APP_DIR}/swap
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    CUSTOM_IO_APDU_BUFFER_SIZE=272
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_logic_sign_tx_finalize
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/sign_tx/logic_sign_tx.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_tx
    ${APP_DIR}/features/get_public_key
    ${APP_DIR}/features/generic_tx_parser
    ${APP_DIR}/features/provide_network_info
    ${APP_DIR}/features/provide_proxy_info
    ${APP_DIR}/plugins
    ${APP_DIR}/swap
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    CUSTOM_IO_APDU_BUFFER_SIZE=272
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=getEthDisplayableAddress
    -Wl,--wrap=amountToString
)

ledger_unit_tests_add_test(NAME test_cmd_sign_tx
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/sign_tx/cmd_sign_tx.c
    ${APP_DIR}/features/sign_tx/eth_ustream.c
    ${APP_DIR}/features/sign_tx/rlp_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_tx
    ${APP_DIR}/features/generic_tx_parser
    ${APP_DIR}/features/provide_network_info
    ${APP_DIR}/plugins
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    CUSTOM_IO_APDU_BUFFER_SIZE=272
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_sign_message
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/sign_message/cmd_sign_message.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_message
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_commands_7702
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/sign_authorization_eip7702/commands_7702.c
    ${APP_DIR}/features/sign_authorization_eip7702/auth_7702.c
    ${APP_DIR}/features/sign_authorization_eip7702/rlp_encode.c
    ${APP_DIR}/features/sign_authorization_eip7702/whitelist_7702.c
    ${APP_DIR}/tlv_apdu.c
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

ledger_unit_tests_add_test(NAME test_cmd_get_public_key
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/get_public_key/cmd_get_public_key.c
    ${APP_DIR}/features/get_public_key/get_public_key.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/get_public_key
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    CUSTOM_IO_APDU_BUFFER_SIZE=272
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_perform_privacy_operation
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/perform_privacy_operation/cmd_perform_privacy_operation.c
    ${APP_DIR}/features/perform_privacy_operation/logic_perform_privacy_operation.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/perform_privacy_operation
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    CUSTOM_IO_APDU_BUFFER_SIZE=272
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=getEthAddressStringFromRawKey
)

ledger_unit_tests_add_test(NAME test_cmd_get_app_configuration
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/get_app_configuration/cmd_get_app_configuration.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/get_app_configuration
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_set_eth2_withdrawal_index
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/set_eth2_withdrawal_index/cmd_set_eth2_withdrawal_index.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/set_eth2_withdrawal_index
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_ETH2
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_get_eth2_public_key
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/get_eth2_public_key/cmd_get_eth2_public_key.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/get_eth2_public_key
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_ETH2
    CUSTOM_IO_APDU_BUFFER_SIZE=272
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_handle_get_printable_amount
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/swap/handle_get_printable_amount.c
  MOCK_HEADERS
    ${PLUGIN_DIR}/common_utils.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/swap
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  COMPILE_OPTIONS
    # CMock generates null-checks for nonnull-annotated parameters; suppress the resulting GCC warning.
    -Wno-nonnull-compare
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)
