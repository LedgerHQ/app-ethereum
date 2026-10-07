# tests_plugins.cmake

ledger_unit_tests_add_test(NAME test_erc20_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/plugins/erc20/erc20_plugin.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/plugins
    ${APP_DIR}/plugins/erc20
    ${APP_DIR}/swap
    ${APP_DIR}/features/sign_tx
    ${APP_DIR}/features/provide_erc20_token_information
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_erc721_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/plugins/erc721/erc721_plugin.c
    ${APP_DIR}/plugins/erc721/erc721_provide_parameters.c
    ${APP_DIR}/plugins/erc721/erc721_ui.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/plugins
    ${APP_DIR}/plugins/erc721
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=getEthDisplayableAddress
)

ledger_unit_tests_add_test(NAME test_erc1155_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/plugins/erc1155/erc1155_plugin.c
    ${APP_DIR}/plugins/erc1155/erc1155_provide_parameters.c
    ${APP_DIR}/plugins/erc1155/erc1155_ui.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/plugins
    ${APP_DIR}/plugins/erc1155
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=getEthDisplayableAddress
)

ledger_unit_tests_add_test(NAME test_eth2_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/plugins/eth2/eth2_plugin.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/plugins
    ${APP_DIR}/plugins/eth2
    ${APP_DIR}/features/get_eth2_public_key
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_ETH2
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=amountToString
)

ledger_unit_tests_add_test(NAME test_eip7002_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/plugins/eip7002/eip7002_plugin.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/plugins
    ${APP_DIR}/plugins/eip7002
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=amountToString
)

ledger_unit_tests_add_test(NAME test_eip7251_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/plugins/eip7251/eip7251_plugin.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/plugins
    ${APP_DIR}/plugins/eip7251
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=amountToString
)

ledger_unit_tests_add_test(NAME test_swap_with_calldata_plugin
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/plugins/swap_with_calldata/swap_with_calldata_plugin.c
    ${APP_DIR}/utils/utils.c
    ${APP_DIR}/uint128.c
    ${APP_DIR}/uint256.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/plugins
    ${APP_DIR}/plugins/swap_with_calldata
    ${BOLOS_SDK}
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_eth_plugin_handler
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/plugins/eth_plugin_handler.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/plugins
    ${APP_DIR}/plugins/erc20
    ${APP_DIR}/plugins/eth2
    ${APP_DIR}/plugins/erc721
    ${APP_DIR}/plugins/erc1155
    ${APP_DIR}/plugins/eip7002
    ${APP_DIR}/plugins/eip7251
    ${APP_DIR}/plugins/swap_with_calldata
    ${APP_DIR}/swap
    ${APP_DIR}/features/sign_tx
    ${APP_DIR}/features/set_plugin
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)
