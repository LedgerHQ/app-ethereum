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

include_directories(
  ${APP_DIR}
  ${PLUGIN_DIR}
  ${APP_DIR}/features/generic_tx_parser
  ${APP_DIR}/features/provide_trusted_name
  ${APP_DIR}/features/getPublicKey
  ${APP_DIR}/features/signTx
  ${APP_DIR}/nbgl
  ${MOCK_DIR}
  ${BOLOS_SDK}/io/include
  ${BOLOS_SDK}/lib_cxng/include
  ${BOLOS_SDK}/lib_nbgl/include
  ${BOLOS_SDK}/lib_standard_app
  ${BOLOS_SDK}/include
  ${LIBBSD_INCLUDE_DIRS}
)

# Network parameter test
add_executable(test_param_network
  ${SRC_DIR}/test_param_network.c
  ${APP_DIR}/features/generic_tx_parser/gtp_value.c
  ${APP_DIR}/features/generic_tx_parser/gtp_param_network.c
  ${PLUGIN_DIR}/common_utils.c
  ${MOCK_DIR}/mock.c
  ${BOLOS_SDK}/lib_standard_app/format.c
)

target_compile_definitions(test_param_network PRIVATE
  HAVE_HASH
  HAVE_SHA256
  HAVE_SHA3
  HAVE_ECC
  HAVE_SPRINTF
)

target_link_libraries(test_param_network PUBLIC
                      cmocka
                      gcov
                      ${LIBBSD_LIBRARIES}
                      -Wl,--wrap=get_network_as_string_from_chain_id
                      -Wl,--wrap=add_to_field_table
)

add_test(test_param_network test_param_network)

# Trusted name parameter test
add_executable(test_param_trusted_name
  ${SRC_DIR}/test_param_trusted_name.c
  ${APP_DIR}/features/generic_tx_parser/gtp_value.c
  ${APP_DIR}/features/generic_tx_parser/gtp_param_trusted_name.c
  ${APP_DIR}/utils.c
  ${PLUGIN_DIR}/common_utils.c
  ${MOCK_DIR}/mock.c
  ${BOLOS_SDK}/lib_standard_app/format.c
)

# Add arraylen.h to the include path before other includes
target_include_directories(test_param_trusted_name BEFORE PRIVATE ${MOCK_DIR})

target_compile_definitions(test_param_trusted_name PRIVATE
  HAVE_HASH
  HAVE_SHA256
  HAVE_SHA3
  HAVE_ECC
)

target_link_libraries(test_param_trusted_name PUBLIC
                      cmocka
                      gcov
                      ${LIBBSD_LIBRARIES}
                      -Wl,--wrap=get_trusted_name
                      -Wl,--wrap=get_public_key
                      -Wl,--wrap=get_current_tx_chain_id
                      -Wl,--wrap=add_to_field_table
)

add_test(test_param_trusted_name test_param_trusted_name)

# Raw parameter test
add_executable(test_param_raw
  ${SRC_DIR}/test_param_raw.c
  ${APP_DIR}/features/generic_tx_parser/gtp_value.c
  ${APP_DIR}/features/generic_tx_parser/gtp_param_raw.c
  ${APP_DIR}/uint256.c
  ${APP_DIR}/uint128.c
  ${APP_DIR}/uint_common.c
  ${APP_DIR}/utils.c
  ${PLUGIN_DIR}/common_utils.c
  ${MOCK_DIR}/mock.c
  ${BOLOS_SDK}/lib_standard_app/format.c
  ${BOLOS_SDK}/lib_standard_app/read.c
  ${BOLOS_SDK}/lib_standard_app/write.c
  ${BOLOS_SDK}/src/os_printf.c
)

# Disable -Wformat warnings for gtp_param_raw.c (uses BOLOS_SDK specific %.*h format)
set_source_files_properties(
  ${APP_DIR}/features/generic_tx_parser/gtp_param_raw.c
  PROPERTIES COMPILE_FLAGS "-Wno-format -Wno-format-extra-args"
)

target_compile_definitions(test_param_raw PRIVATE
  HAVE_HASH
  HAVE_SHA256
  HAVE_SHA3
  HAVE_ECC
  HAVE_SPRINTF
  HAVE_SNPRINTF
  HAVE_SNPRINTF_FORMAT_U
  HAVE_MATH
)

target_link_libraries(test_param_raw PUBLIC
                      cmocka
                      gcov
                      ${LIBBSD_LIBRARIES}
                      -Wl,--wrap=add_to_field_table
)

add_test(test_param_raw test_param_raw)

# Field validation test
add_executable(test_field_validation
  ${SRC_DIR}/test_field_validation.c
  ${APP_DIR}/features/generic_tx_parser/gtp_field.c
  ${APP_DIR}/mem_utils.c
  ${APP_DIR}/list.c
  ${APP_DIR}/utils.c
  ${PLUGIN_DIR}/common_utils.c
  ${MOCK_DIR}/mock.c
  ${MOCK_DIR}/field_validation_mocks.c
  ${BOLOS_SDK}/lib_standard_app/format.c
)

target_compile_definitions(test_field_validation PRIVATE
  HAVE_HASH
  HAVE_SHA256
  HAVE_SHA3
  HAVE_ECC
)

target_compile_options(test_field_validation PRIVATE
  -fshort-enums
)

target_link_libraries(test_field_validation PUBLIC
                      cmocka
                      gcov
                      ${LIBBSD_LIBRARIES}
)

add_test(test_field_validation test_field_validation)
