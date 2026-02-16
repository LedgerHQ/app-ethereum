cmake_minimum_required(VERSION 3.14)

if(${CMAKE_VERSION} VERSION_LESS 3.14)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# project information
project(EthereumAppFuzzer
        VERSION 1.0
	      DESCRIPTION "App Ethereum Fuzzer"
        LANGUAGES C)

if(NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK must be defined, CMake will exit.")
  return()
endif()

add_subdirectory(${BOLOS_SDK}/fuzzing ${CMAKE_CURRENT_BINARY_DIR}/ledger-secure-sdk EXCLUDE_FROM_ALL)

set(DEFINES FUZZ)

set(APP_SRC ${CMAKE_SOURCE_DIR}/../../src)
set(PLUGIN_SDK_SRC ${CMAKE_SOURCE_DIR}/../../ethereum-plugin-sdk/src)

file(GLOB_RECURSE C_SOURCES
  ${APP_SRC}/*.c
  ${PLUGIN_SDK_SRC}/*.c
  ${CMAKE_SOURCE_DIR}/mock/*.c
  ${CMAKE_SOURCE_DIR}/src/fuzz_utils.c
)
list(REMOVE_ITEM C_SOURCES
  ${APP_SRC}/main.c
  ${PLUGIN_SDK_SRC}/main.c
)

add_library(code_lib ${C_SOURCES})

target_include_directories(
  code_lib
  PUBLIC ${CMAKE_SOURCE_DIR}/src
         ${CMAKE_SOURCE_DIR}/mock
         ${APP_SRC}
         ${APP_SRC}/features/generic_tx_parser
         ${APP_SRC}/features/getPublicKey
         ${APP_SRC}/features/provide_enum_value
         ${APP_SRC}/features/provide_network_info
         ${APP_SRC}/features/signTx
         ${APP_SRC}/features/provide_trusted_name
         ${APP_SRC}/features/getChallenge
         ${APP_SRC}/features/signMessageEIP712
         ${APP_SRC}/features/provide_proxy_info
         ${APP_SRC}/features/provide_tx_simulation
         ${APP_SRC}/features/signAuthorizationEIP7702
         ${APP_SRC}/features/provide_safe_account
         ${APP_SRC}/features/provide_gating
         ${APP_SRC}/features/signMessageEIP712_common
         ${APP_SRC}/features/signMessageEIP712
         ${APP_SRC}/features/setPlugin
         ${APP_SRC}/plugins/eth2
         ${APP_SRC}/plugins/eip7002
         ${APP_SRC}/plugins/eip7251
         ${APP_SRC}/plugins/erc20
         ${APP_SRC}/plugins/erc721
         ${APP_SRC}/plugins/erc1155
         ${APP_SRC}/plugins/swap_with_calldata
         ${APP_SRC}/nbgl
         ${PLUGIN_SDK_SRC}
)

target_link_libraries(code_lib PUBLIC secure_sdk)
target_compile_definitions(code_lib PUBLIC ${DEFINES} FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1)

# Find and add libbsd
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBSD REQUIRED libbsd)

# Try to find the static libbsd library, because the final run image doesn't have dynamic library
# Keep both options to fallback to dynamic if static is not found
find_library(LIBBSD_STATIC_LIB lib${LIBBSD_STATIC_LIBRARIES}.a HINTS ${LIBBSD_LIBRARY_DIRS})

if(LIBBSD_STATIC_LIB)
    message(STATUS "Using static libbsd")
    target_link_libraries(code_lib PUBLIC ${LIBBSD_STATIC_LIB})
else()
    message(STATUS "Using dynamic libbsd")
    target_link_libraries(code_lib PUBLIC ${LIBBSD_LIBRARIES})
endif()

target_include_directories(code_lib PUBLIC ${LIBBSD_INCLUDE_DIRS})
target_compile_options(code_lib PUBLIC ${LIBBSD_CFLAGS_OTHER})

# Create the different fuzzing targets automatically
file(GLOB FUZZ_HARNESSES "${CMAKE_SOURCE_DIR}/harness/fuzz_*.c")

foreach(HARNESS_FILE ${FUZZ_HARNESSES})
    # Extract the base name of the file without extension
    get_filename_component(HARNESS_NAME ${HARNESS_FILE} NAME_WE)

    # Create the executable target
    add_executable(${HARNESS_NAME} ${HARNESS_FILE})

    # Define the compilation options
    target_compile_definitions(${HARNESS_NAME} PUBLIC macros FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1)

    # Link against the secure_sdk and code_lib libraries
    target_link_libraries(${HARNESS_NAME} PUBLIC secure_sdk code_lib)

    message(STATUS "Creating fuzzer target: ${HARNESS_NAME}")
endforeach()
