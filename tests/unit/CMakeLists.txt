cmake_minimum_required(VERSION 3.10)

if(${CMAKE_VERSION} VERSION_LESS 3.10)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# project information
project(unit_tests
        VERSION 0.1
	      DESCRIPTION "Unit tests for Ledger Ethereum application"
        LANGUAGES C)

# Check for BOLOS_SDK (optional for unit tests, provides SDK headers if available)
if(NOT DEFINED BOLOS_SDK)
  set(BOLOS_SDK "/opt/ledger-secure-sdk")
  message(STATUS "BOLOS_SDK not defined, using default: ${BOLOS_SDK}")
else()
  message(STATUS "Using BOLOS_SDK: ${BOLOS_SDK}")
endif()

# guard against bad build-type strings
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

include(CTest)
ENABLE_TESTING()

# specify C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Find libbsd for BSD string functions (strlcpy, strlcat)
find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(LIBBSD libbsd)
endif()

# Fallback if pkg-config or libbsd not found via pkg-config
if(NOT LIBBSD_FOUND)
  # Try to find libbsd manually
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

add_compile_options(-Wall -Wextra -g -pedantic --coverage)
# Flag depending on the Build Type
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}-O3")

set(GCC_COVERAGE_LINK_FLAGS "--coverage -lgcov")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")

set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(MOCK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mocks")
set(PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ethereum-plugin-sdk/src")

add_compile_definitions(TEST)
# APPNAME is provided as a CFLAG by the SDK build (makefile_conf/chain/*.mk); the
# unit-test build must define it too, since network.c uses it as the native-chain
# name fallback. "Ethereum" matches the default chain expected by the tests.
add_compile_definitions(APPNAME="Ethereum")

include_directories(
  ${APP_DIR}
  ${PLUGIN_DIR}
  ${APP_DIR}/features/generic_tx_parser
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
  ${APP_DIR}/nbgl
  ${APP_DIR}/utils
  ${MOCK_DIR}
  ${BOLOS_SDK}/io/include
  ${BOLOS_SDK}/lib_cxng/include
  ${BOLOS_SDK}/lib_nbgl/include
  ${BOLOS_SDK}/lib_lists
  ${BOLOS_SDK}/lib_standard_app
  ${BOLOS_SDK}/lib_tlv
  ${BOLOS_SDK}/lib_alloc
  ${BOLOS_SDK}/include
  ${LIBBSD_INCLUDE_DIRS}
)

# =============================================================================
# Test infrastructure: shared libraries + helper function
# =============================================================================

# Stubs that compile standalone (forward-declares all SDK opaque types,
# so HAVE_SHA256 etc. are not required).
add_library(eth_tests_mocks STATIC ${MOCK_DIR}/mock.c)

# Shared app-side globals (txContext, tmpContent, g_chainConfig, ...).
# Layout of internalStorage_t depends on HAVE_TRANSACTION_CHECKS so we
# widen the compile defs here to the union of every consumer's view --
# the library and all callers then agree on struct sizes.
add_library(eth_tests_app_globals STATIC ${MOCK_DIR}/app_globals.c)
target_compile_definitions(eth_tests_app_globals PRIVATE
  HAVE_ECDSA HAVE_HASH HAVE_SHA256 HAVE_SHA3 HAVE_ECC HAVE_MATH
  HAVE_TRANSACTION_CHECKS
)
target_include_directories(eth_tests_app_globals PRIVATE
  ${APP_DIR}/features/provide_network_info
)

# SDK helpers used by 50+ targets.
add_library(eth_tests_sdk_helpers STATIC
  ${BOLOS_SDK}/lib_standard_app/format.c
  ${BOLOS_SDK}/lib_standard_app/read.c
  ${BOLOS_SDK}/lib_standard_app/write.c
  ${BOLOS_SDK}/lib_tlv/tlv_library.c
)
set_source_files_properties(
  ${BOLOS_SDK}/lib_tlv/tlv_library.c
  PROPERTIES COMPILE_FLAGS "-Wno-pedantic"
)
# tlv_library.c calls pic() from eth_tests_mocks; declare the dependency so
# CMake lists the libraries in an order the single-pass linker can resolve.
target_link_libraries(eth_tests_sdk_helpers PUBLIC eth_tests_mocks)

include(cmake/add_eth_unit_test.cmake)

# =============================================================================
# Test targets (grouped by feature -- one .cmake file per group)
# =============================================================================

include(cmake/tests_low_level.cmake)
include(cmake/tests_gtp.cmake)
include(cmake/tests_descriptors.cmake)
include(cmake/tests_sign.cmake)
include(cmake/tests_plugins.cmake)
include(cmake/tests_nbgl.cmake)
