cmake_minimum_required(VERSION 3.20)

project(EthereumAppFuzzer
        VERSION 1.0
        DESCRIPTION "Ledger Ethereum App Fuzzer"
        LANGUAGES C)

if(NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK must be defined")
endif()

# Exports the `secure_sdk` target and ledger_fuzz_add_app_target().
include(${BOLOS_SDK}/fuzzing/cmake/LedgerAppFuzz.cmake)
ledger_fuzz_setup()

set(APP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/../..)

# Whole app plus the plugin SDK and the fuzz mocks; both main.c are replaced by
# the fuzz entry point. address_book lives in the SDK, so glob it separately.
file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS
    ${APP_SOURCE_DIR}/src/*.c
    ${APP_SOURCE_DIR}/ethereum-plugin-sdk/src/*.c
    ${CMAKE_SOURCE_DIR}/mock/*.c
    ${BOLOS_SDK}/app_features/address_book/src/*.c)
list(REMOVE_ITEM C_SOURCES
    ${APP_SOURCE_DIR}/src/main.c
    ${APP_SOURCE_DIR}/ethereum-plugin-sdk/src/main.c)

ledger_fuzz_collect_include_dirs(APP_INCLUDE_DIRS
    ${APP_SOURCE_DIR}/src
    ${APP_SOURCE_DIR}/ethereum-plugin-sdk/src)
list(APPEND APP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/mock)

# FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is the shared app/SDK sentinel for
# fuzz-only relaxations. HAVE_* / APPNAME / ... arrive transitively from
# secure_sdk (make list-defines + macros/add_macros.txt).
function(eth_add_fuzz_target NAME INVARIANT)
  # Invariant models are machine-local and regenerated per build (.gitignored).
  ledger_fuzz_bootstrap_file("${INVARIANT}" ".{}\n")
  ledger_fuzz_add_app_target(
      NAME ${NAME}
      HARNESS ${CMAKE_SOURCE_DIR}/harness/${NAME}.c
      INVARIANT ${INVARIANT}
      SOURCES ${C_SOURCES} ${ARGN}
      INCLUDE_DIRECTORIES ${APP_INCLUDE_DIRS}
      COMPILE_DEFINITIONS FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1)
endfunction()

# Full-app target: one input drives a sequence of APDUs through the real dispatcher.
eth_add_fuzz_target(fuzz_app
    "${CMAKE_SOURCE_DIR}/invariants/fuzz_globals.zon"
    ${LEDGER_FUZZ_TLV_MUTATOR_SOURCE})

# Internal plugins are driven through the plugin interface rather than an INS,
# so a fuzzed APDU cannot reach them on its own.
eth_add_fuzz_target(fuzz_plugin "${CMAKE_SOURCE_DIR}/invariants/fuzz_plugin.zon")

# Stateful parsers behind a multi-APDU handshake fuzz_app cannot set up.
eth_add_fuzz_target(fuzz_parser "${CMAKE_SOURCE_DIR}/invariants/fuzz_parser.zon")
