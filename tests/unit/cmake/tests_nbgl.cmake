# tests_nbgl.cmake

ledger_unit_tests_add_test(NAME test_network_icons
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/nbgl/ui_icons.c
    ${MOCK_DIR}/net_icons_stub.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/nbgl
    ${APP_DIR}/features/provide_network_info
    ${APP_DIR}/plugins
    ${SDK_DIR}/lib_nbgl/include
    ${SDK_DIR}/lib_ux_nbgl
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_NBGL
    ICONGLYPH=test_glyph
    ICONHOME=test_home_glyph
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_ui_icons
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/nbgl/ui_icons.c
    ${MOCK_DIR}/net_icons_stub.c
  MOCK_HEADERS
    ${APP_DIR}/network.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/nbgl
    ${APP_DIR}/features/provide_network_info
    ${APP_DIR}/plugins
    ${SDK_DIR}/lib_nbgl/include
    ${SDK_DIR}/lib_ux_nbgl
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_NBGL
    ICONGLYPH=test_glyph
    ICONHOME=test_home_glyph
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_ui_utils
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/nbgl/ui_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/nbgl
    ${SDK_DIR}/lib_nbgl/include
    ${SDK_DIR}/lib_ux_nbgl
    ${SDK_DIR}/lib_alloc
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_NBGL
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)

ledger_unit_tests_add_test(NAME test_common_712
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/sign_message_eip712_common/common_712.c
    ${APP_DIR}/nbgl/ui_utils.c
    ${PLUGIN_DIR}/common_utils.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/sign_message_eip712_common
    ${APP_DIR}/features/provide_tx_simulation
    ${SDK_DIR}/lib_tlv/use_cases
    ${SDK_DIR}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_NBGL
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=mem_utils_calloc
    -Wl,--wrap=mem_utils_free
    -Wl,--wrap=mem_utils_free_and_null
)
