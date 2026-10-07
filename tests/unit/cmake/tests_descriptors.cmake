# tests_descriptors.cmake

ledger_unit_tests_add_test(NAME test_ledger_pki
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/ledger_pki.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${BOLOS_SDK}/lib_pki
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_handle_check_address
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/swap/handle_check_address.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/swap
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_proxy_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_proxy_info/cmd_proxy_info.c
  MOCK_HEADERS
    ${APP_DIR}/tlv_apdu.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_proxy_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_trusted_name
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_trusted_name/cmd_trusted_name.c
  MOCK_HEADERS
    ${APP_DIR}/tlv_apdu.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_enum_value
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_enum_value/cmd_enum_value.c
  MOCK_HEADERS
    ${APP_DIR}/tlv_apdu.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_enum_value
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_safe_account
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_safe_account/cmd_safe_account.c
  MOCK_HEADERS
    ${APP_DIR}/tlv_apdu.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_safe_account
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_network_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_network_info/cmd_network_info.c
  MOCK_HEADERS
    ${APP_DIR}/tlv_apdu.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_network_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_network_icon
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_network_info/network_icon.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_network_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_handle_swap_sign_transaction
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/swap/handle_swap_sign_transaction.c
    ${APP_DIR}/utils/mem_utils.c
  MOCK_HEADERS
    ${PLUGIN_DIR}/common_utils.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/swap
    ${APP_DIR}/features/sign_tx
    ${APP_DIR}/features/set_plugin
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  COMPILE_OPTIONS
    # CMock generates null-checks for nonnull-annotated parameters; suppress the resulting GCC warning.
    -Wno-nonnull-compare
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_network
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/network.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_network_info
    ${APP_DIR}/features/sign_tx
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_network_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_network_info/network_info.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
    ${APP_DIR}/utils/hash_bytes.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_network_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_proxy_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_proxy_info/proxy_info.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
    ${APP_DIR}/utils/hash_bytes.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_proxy_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_trusted_name
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_trusted_name/trusted_name.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/lib_standard_app/bip32.c
    ${BOLOS_SDK}/lib_lists/lists.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
    ${APP_DIR}/utils/hash_bytes.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_trusted_name
    ${APP_DIR}/features/provide_proxy_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_safe_descriptors
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_safe_account/safe_descriptor.c
    ${APP_DIR}/features/provide_safe_account/signer_descriptor.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
    ${APP_DIR}/utils/hash_bytes.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_safe_account
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_get_tx_simulation
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_tx_simulation/cmd_get_tx_simulation.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_tx_simulation
    ${BOLOS_SDK}/lib_tlv/use_cases
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_get_gating
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_gating/cmd_get_gating.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_gating
    ${APP_DIR}/features/sign_message_eip712
    ${APP_DIR}/features/provide_proxy_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_get_challenge
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/get_challenge/cmd_get_challenge.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/get_challenge
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    CUSTOM_IO_APDU_BUFFER_SIZE=272
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_provide_nft_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_nft_information/cmd_provide_nft_info.c
    ${APP_DIR}/features/provide_nft_information/nft_info.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_nft_information
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_provide_token_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_erc20_token_information/cmd_provide_token_info.c
    ${APP_DIR}/features/provide_erc20_token_information/token_info.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_erc20_token_information
    ${BOLOS_SDK}/lib_tlv/use_cases
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_set_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/set_plugin/cmd_set_plugin.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/set_plugin
    ${APP_DIR}/plugins
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_set_plugin_staging
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/set_plugin/cmd_set_plugin.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/set_plugin
    ${APP_DIR}/plugins
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_NFT_STAGING_KEY
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_set_external_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/set_external_plugin/cmd_set_external_plugin.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/set_external_plugin
    ${APP_DIR}/plugins
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)
