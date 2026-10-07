# tests_address_book.cmake

ledger_unit_tests_add_test(NAME test_address_name_lookup
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/address_name_lookup.c
    ${PLUGIN_DIR}/common_utils.c
  MOCK_HEADERS
    ${APP_DIR}/features/provide_trusted_name/trusted_name.h
    ${APP_DIR}/features/address_book/handle_contacts.h
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/address_book
    ${BOLOS_SDK}/app_features/address_book/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_ADDRESS_BOOK
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)
