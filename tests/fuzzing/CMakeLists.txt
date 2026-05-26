include_guard()
cmake_minimum_required(VERSION 3.14)

project(EthereumAppFuzzer
        VERSION 1.0
        DESCRIPTION "Ledger Ethereum App Fuzzer"
        LANGUAGES C)

if(NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK must be defined")
endif()

if(NOT DEFINED APP_BUILD_PATH)
  get_filename_component(APP_BUILD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

# `ledger_fuzz_setup()` is the SDK entry point for the updated fuzz
# framework. It exports the `secure_sdk` INTERFACE target the develop branch
# already linked against, and additionally registers the Absolution-based
# `ledger_fuzz_add_app_target()` helper used at the bottom of this file.
include(${BOLOS_SDK}/fuzzing/cmake/LedgerAppFuzz.cmake)
ledger_fuzz_setup()

set(APP_SRC ${CMAKE_SOURCE_DIR}/../../src)
set(PLUGIN_SDK_SRC ${CMAKE_SOURCE_DIR}/../../ethereum-plugin-sdk/src)
set(COVERAGE_SIGUSR1_SOURCE "${CMAKE_SOURCE_DIR}/mock/coverage_sigusr1.c")

# Sources that only the global-coverage dispatcher harness needs (it goes
# through `apdu_parser()` and uses the SDK's grammar-aware mutator).
set(DISPATCHER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/fuzz_apdu_adapter.c
    ${CMAKE_SOURCE_DIR}/src/fuzz_apdu_dispatch.c
    ${CMAKE_SOURCE_DIR}/src/fuzz_command_registry.c
    ${CMAKE_SOURCE_DIR}/src/fuzz_non_apdu.c
    ${CMAKE_SOURCE_DIR}/src/fuzz_runtime.c
    ${CMAKE_SOURCE_DIR}/src/fuzz_tlv_config.c)

# Per-feature harnesses keep the original develop-branch entry-point shape
# (one `LLVMFuzzerTestOneInput` per file). They share the bulk of the app
# sources via the common library and only need their own bootstrap helper.
set(HARNESS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/fuzz_utils.c)

file(GLOB_RECURSE APP_SOURCES
    ${APP_SRC}/*.c
    ${PLUGIN_SDK_SRC}/*.c
    ${CMAKE_SOURCE_DIR}/mock/*.c)
list(REMOVE_ITEM APP_SOURCES
    ${APP_SRC}/main.c
    ${PLUGIN_SDK_SRC}/main.c
    # Linked only by the per-feature harnesses (which provide g_network_icons
    # via src/fuzz_utils.c). The dispatcher does not link fuzz_utils.c, so
    # excluding it from APP_SOURCES keeps both targets in sync.
    ${APP_SRC}/nbgl/network_icons.c
    ${COVERAGE_SIGUSR1_SOURCE})

file(GLOB_RECURSE APP_HEADERS
    ${APP_SRC}/*.h
    ${PLUGIN_SDK_SRC}/*.h)
set(APP_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/mock
    ${BOLOS_SDK}/lib_tlv/use_cases)
foreach(header ${APP_HEADERS})
  get_filename_component(header_dir ${header} DIRECTORY)
  list(APPEND APP_INCLUDE_DIRS ${header_dir})
endforeach()
list(REMOVE_DUPLICATES APP_INCLUDE_DIRS)

# `FUZZ` toggles fuzz-only branches in the app sources (e.g. `#ifndef FUZZ`
# NBGL/PKI gates inherited from develop). `FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION`
# is the canonical sentinel both the app and the SDK key off. The rest of the
# `HAVE_*` / `APPNAME` / `APP_TICKER` defines come transitively from
# `secure_sdk → macros` (which runs `make list-defines` on the app Makefile
# and layers `macros/add_macros.txt` on top), so we don't duplicate them here.
set(FUZZ_DEFINES
    FUZZ
    FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBSD REQUIRED libbsd)
find_library(LIBBSD_STATIC_LIB lib${LIBBSD_STATIC_LIBRARIES}.a HINTS ${LIBBSD_LIBRARY_DIRS})
if(LIBBSD_STATIC_LIB)
  message(STATUS "Using static libbsd")
  set(LIBBSD_TARGET ${LIBBSD_STATIC_LIB})
else()
  message(STATUS "Using dynamic libbsd")
  set(LIBBSD_TARGET ${LIBBSD_LIBRARIES})
endif()

# Common library: app sources + mocks + globals. Linked by every fuzz binary
# (per-feature harnesses and the dispatcher) so each app global has exactly
# one definition.
add_library(eth_fuzz_common STATIC ${APP_SOURCES})
target_link_libraries(eth_fuzz_common PUBLIC secure_sdk ${LIBBSD_TARGET})
target_include_directories(eth_fuzz_common PUBLIC
    ${APP_INCLUDE_DIRS}
    ${LIBBSD_INCLUDE_DIRS})
target_compile_definitions(eth_fuzz_common PUBLIC ${FUZZ_DEFINES})
target_compile_options(eth_fuzz_common PUBLIC ${LIBBSD_CFLAGS_OTHER})

# MSan does not understand the volatile byte stores libc and lib_alloc use to
# implement explicit_bzero (https://github.com/google/sanitizers/issues/1507),
# so memory zeroed that way stays poisoned and bleeds into later reads as
# false-positive use-of-uninitialized-value reports. Routing the symbol through
# __wrap_explicit_bzero (defined in mock/mocks.c as memset + compiler barrier)
# keeps the security guarantees of explicit_bzero on-device while letting MSan
# correctly unpoison the buffer in fuzz builds. Applied to every fuzz target
# below so the dispatcher and the per-feature harnesses share the same fix.
set(FUZZ_WRAP_FLAGS -Wl,--wrap=explicit_bzero)

# Per-feature harnesses: one libFuzzer executable per file under harness/.
# Each one provides its own LLVMFuzzerTestOneInput, links the common library
# plus the small per-feature helper, and wraps the SDK memory + crypto
# entries the same way develop did.
file(GLOB FUZZ_HARNESS_FILES ${CMAKE_SOURCE_DIR}/harness/fuzz_*.c)
foreach(HARNESS_FILE ${FUZZ_HARNESS_FILES})
  get_filename_component(HARNESS_NAME ${HARNESS_FILE} NAME_WE)
  add_executable(${HARNESS_NAME} ${HARNESS_FILE} ${HARNESS_SOURCES})
  target_link_libraries(${HARNESS_NAME} PUBLIC eth_fuzz_common)
  target_link_options(${HARNESS_NAME} PRIVATE
      -Wl,--wrap=mem_init
      -Wl,--wrap=mem_alloc
      -Wl,--wrap=mem_free
      -Wl,--wrap=cx_ecdsa_verify_no_throw
      ${FUZZ_WRAP_FLAGS})
  message(STATUS "Per-feature fuzzer target: ${HARNESS_NAME}")
endforeach()

# Dispatcher harness: a single binary driven by Absolution that exercises
# the real APDU dispatcher. Registered through the SDK helper so the
# framework owns the mutator / invariant / scenario glue.
set(DISPATCHER_LINK_SOURCES ${DISPATCHER_SOURCES})
if(FUZZ_ENABLE_SOURCE_COVERAGE AND EXISTS "${COVERAGE_SIGUSR1_SOURCE}")
  list(APPEND DISPATCHER_LINK_SOURCES ${COVERAGE_SIGUSR1_SOURCE})
  message(STATUS "Ethereum fuzz build: SIGUSR1 coverage dump hook enabled")
endif()

ledger_fuzz_add_app_target(
    NAME fuzz_app
    HARNESS ${CMAKE_SOURCE_DIR}/src/fuzz_dispatcher.c
    INVARIANT ${CMAKE_SOURCE_DIR}/invariants/fuzz_globals.zon
    SOURCES ${APP_SOURCES} ${DISPATCHER_LINK_SOURCES}
    EXTRA_TARGETS ${LEDGER_FUZZ_TLV_MUTATOR_SOURCE}
    INCLUDE_DIRECTORIES ${APP_INCLUDE_DIRS}
    COMPILE_DEFINITIONS ${FUZZ_DEFINES})

# `ledger_fuzz_add_app_target()` creates the Absolution-driven `fuzz_app`
# executable but does not expose the link-options hook. Attach the
# explicit_bzero wrap to the same target so the dispatcher binary picks up
# the MSan workaround already applied to the per-feature harnesses above.
target_link_options(fuzz_app PRIVATE ${FUZZ_WRAP_FLAGS})
