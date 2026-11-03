cmake_minimum_required(VERSION 3.10)
project(unit_tests VERSION 0.1 DESCRIPTION "Unit tests for Ledger Ethereum application" LANGUAGES C)

if(NOT DEFINED ENV{BOLOS_SDK})
  message(FATAL_ERROR "BOLOS_SDK environment variable is not set. Please set it to the path of the Ledger Secure SDK.")
endif()
set(SDK_DIR $ENV{BOLOS_SDK})
include(${SDK_DIR}/cmake/LedgerUT.cmake)

set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
set(MOCK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mocks")
set(PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ethereum-plugin-sdk/src")

# Find libbsd for BSD string functions (strlcpy, strlcat)
find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(LIBBSD libbsd)
endif()
if(NOT LIBBSD_FOUND)
  find_path(LIBBSD_INCLUDE_DIR bsd/string.h)
  find_library(LIBBSD_LIBRARY NAMES bsd)
  if(LIBBSD_INCLUDE_DIR AND LIBBSD_LIBRARY)
    set(LIBBSD_FOUND TRUE)
    set(LIBBSD_INCLUDE_DIRS ${LIBBSD_INCLUDE_DIR})
    set(LIBBSD_LIBRARIES ${LIBBSD_LIBRARY})
    message(STATUS "Found libbsd manually: ${LIBBSD_LIBRARY}")
  else()
    message(WARNING "libbsd not found - BSD string functions may not be available")
    set(LIBBSD_INCLUDE_DIRS "")
    set(LIBBSD_LIBRARIES "")
  endif()
endif()

# =============================================================================
# Shared helper variables – referenced from every tests_*.cmake
# =============================================================================

# Sources compiled into every test binary.
set(COMMON_SOURCES
  ${MOCK_DIR}/sdk_stubs.c
  ${MOCK_DIR}/system_stubs.c
  ${LEDGER_UT_EXCEPTION_STUBS}
  ${SDK_DIR}/lib_standard_app/format.c
  ${SDK_DIR}/lib_standard_app/read.c
  ${SDK_DIR}/lib_standard_app/write.c
  ${SDK_DIR}/lib_tlv/tlv_library.c
)

# Sources for tests that need shared app globals (all non-NO_GLOBALS tests).
set(COMMON_SOURCES_WITH_GLOBALS
  ${COMMON_SOURCES}
  ${MOCK_DIR}/app_globals.c
)

# Include directories added to every test target.
set(COMMON_INCLUDE_DIRS
  ${APP_DIR}
  ${PLUGIN_DIR}
  ${APP_DIR}/features/generic_tx_parser
  ${APP_DIR}/features/gcs
  ${APP_DIR}/features/provide_erc20_token_information
  ${APP_DIR}/features/provide_nft_information
  ${APP_DIR}/features/provide_trusted_name
  ${APP_DIR}/features/provide_map_entry
  ${APP_DIR}/features/provide_proxy_info
  ${APP_DIR}/features/get_public_key
  ${APP_DIR}/features/sign_tx
  ${APP_DIR}/features/sign_authorization_eip7702
  ${APP_DIR}/features/sign_message_eip712
  ${APP_DIR}/features/get_challenge
  ${APP_DIR}/features/provide_network_info
  ${APP_DIR}/nbgl
  ${APP_DIR}/utils
  ${APP_DIR}/swap
  ${MOCK_DIR}
  ${SDK_DIR}/io/include
  ${SDK_DIR}/lib_cxng/include
  ${SDK_DIR}/lib_nbgl/include
  ${SDK_DIR}/lib_lists
  ${SDK_DIR}/lib_standard_app
  ${SDK_DIR}/lib_tlv
  ${SDK_DIR}/lib_alloc
  ${SDK_DIR}/lib_ux_nbgl
  ${SDK_DIR}/lib_u2f/include
  ${SDK_DIR}/include
  ${LIBBSD_INCLUDE_DIRS}
)

# Compile definitions added to every test target.
# UT_COMPILE_DEFS_SDK is provided by the SDK but not auto-applied by
# ledger_unit_tests_add_test(); forward it here so all test targets
# inherit the full SDK-standard set. HAVE_TRANSACTION_CHECKS and HAVE_7702_SUPPORT
# are app-specific.
set(COMMON_COMPILE_DEFS
  ${UT_COMPILE_DEFS_SDK}
  HAVE_TRANSACTION_CHECKS
  HAVE_7702_SUPPORT
)

# Link libbsd via link_libraries() so it appears AFTER object files in the link
# command (GNU ld processes -l flags left-to-right; libraries must come after
# the objects that reference them).
if(LIBBSD_LIBRARIES)
  link_directories(${LIBBSD_LIBRARY_DIRS})
  link_libraries(bsd)
endif()

# COMMON_LINK_OPTIONS carries only any remaining raw linker flags (empty for now).
set(COMMON_LINK_OPTIONS "")

# Same char signedness and enum width as the firmware build (SDK Makefile.defines).
add_compile_options(-funsigned-char -fshort-enums)

# =============================================================================
# Test targets (grouped by feature)
# =============================================================================

# Test sources live in src/; ledger_unit_tests_init() is called there so that
# CMAKE_CURRENT_SOURCE_DIR resolves to src/ when ledger_unit_tests_add_test()
# looks up ${NAME}.c.
add_subdirectory(src)
