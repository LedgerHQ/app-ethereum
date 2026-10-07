# tests_gtp.cmake

ledger_unit_tests_add_test(NAME test_param_network
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_value.c
    ${APP_DIR}/features/generic_tx_parser/gtp_param_network.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_param_trusted_name
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_value.c
    ${APP_DIR}/features/generic_tx_parser/gtp_param_trusted_name.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/features/provide_trusted_name/trusted_name.h
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_param_raw
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_value.c
    ${APP_DIR}/features/generic_tx_parser/gtp_param_raw.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/uint256.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/utils/utils.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/src/os_printf.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_SNPRINTF
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_param_group
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_group.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint256.c
    ${APP_DIR}/uint128.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_param_token_amount
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_token_amount.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/uint256.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/utils/utils.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/src/os_printf.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_SNPRINTF
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_param_calldata
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_calldata.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint256.c
    ${APP_DIR}/uint128.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_SNPRINTF
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_param_amount
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_amount.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
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

ledger_unit_tests_add_test(NAME test_param_datetime
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_datetime.c
    ${APP_DIR}/utils/time_format.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
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

ledger_unit_tests_add_test(NAME test_param_duration
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_duration.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
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

ledger_unit_tests_add_test(NAME test_param_unit
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_unit.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
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

ledger_unit_tests_add_test(NAME test_param_enum
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_enum.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_enum_value
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_param_token
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_token.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_param_nft
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_param_nft.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_nft_information
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_field_validation
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_field.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint256.c
    ${APP_DIR}/uint128.c
    ${PLUGIN_DIR}/common_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_field_validation_stubs.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  COMPILE_OPTIONS
    -fshort-enums
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_field_table
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_field_table.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_message_eip712
    ${APP_DIR}/features/provide_trusted_name
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_tx_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_tx_info.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${APP_DIR}/utils/time_format.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  MOCK_HEADERS
    ${APP_DIR}/public_keys.h
    ${APP_DIR}/utils/hash_bytes.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_proxy_info
    ${APP_DIR}/features/provide_trusted_name
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_tx_ctx
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/tx_ctx.c
    ${APP_DIR}/features/generic_tx_parser/calldata.c
    ${APP_DIR}/utils/mem_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_proxy_info
    ${APP_DIR}/features/provide_trusted_name
    ${APP_DIR}/features/sign_message_eip712
    ${APP_DIR}/features/get_public_key
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_data_path
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_data_path.c
    ${APP_DIR}/features/generic_tx_parser/gtp_path_array.c
    ${APP_DIR}/features/generic_tx_parser/gtp_path_slice.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
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

ledger_unit_tests_add_test(NAME test_value
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/gtp_value.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_map_entry
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_provide_map_entry
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_map_entry/map_entry.c
    ${APP_DIR}/tlv_apdu.c
    ${APP_DIR}/utils/tlv_utils.c
    ${APP_DIR}/utils/mem_utils.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint256.c
    ${APP_DIR}/uint128.c
    ${PLUGIN_DIR}/common_utils.c
    ${BOLOS_SDK}/lib_lists/lists.c
    ${BOLOS_SDK}/src/os_printf.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_enum_value
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_enum_value/enum_value.c
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
    ${APP_DIR}/features/provide_enum_value
    ${APP_DIR}/features/provide_proxy_info
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_field_tx_info
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/generic_tx_parser/cmd_field.c
    ${APP_DIR}/features/generic_tx_parser/cmd_tx_info.c
  MOCK_HEADERS
    ${APP_DIR}/tlv_apdu.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_cmd_map_entry
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/provide_map_entry/cmd_map_entry.c
  MOCK_HEADERS
    ${APP_DIR}/tlv_apdu.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/provide_map_entry
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)
