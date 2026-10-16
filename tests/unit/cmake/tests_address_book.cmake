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

ledger_unit_tests_add_test(NAME test_handle_identity
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/address_book/handle_identity.c
    ${APP_DIR}/features/address_book/address_book_ctx.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/address_book
    ${BOLOS_SDK}/app_features/address_book/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_ADDRESS_BOOK
    HAVE_NBGL
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
    -Wl,--wrap=mem_utils_calloc
    -Wl,--wrap=mem_utils_free_and_null
)

ledger_unit_tests_add_test(NAME test_handle_contacts
  SOURCES
    ${COMMON_SOURCES}
    ${APP_DIR}/features/address_book/handle_contacts.c
    ${BOLOS_SDK}/lib_lists/lists.c
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

ledger_unit_tests_add_test(NAME test_handle_ledger_account
  SOURCES
    ${COMMON_SOURCES_WITH_GLOBALS}
    ${APP_DIR}/features/address_book/handle_ledger_account.c
    ${APP_DIR}/features/address_book/handle_contacts.c
    ${APP_DIR}/features/address_book/address_book_ctx.c
    ${BOLOS_SDK}/lib_lists/lists.c
  INCLUDE_DIRS
    ${COMMON_INCLUDE_DIRS}
    ${APP_DIR}/features/address_book
    ${BOLOS_SDK}/app_features/address_book/include
    ${BOLOS_SDK}/io_legacy/include
  COMPILE_DEFS
    ${COMMON_COMPILE_DEFS}
    HAVE_ADDRESS_BOOK
    HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT
    HAVE_NBGL
  LINK_OPTIONS
    ${COMMON_LINK_OPTIONS}
)
