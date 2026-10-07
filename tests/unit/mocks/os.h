#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <bsd/string.h>  // strlcpy, strlcat from libbsd

// Mirror the SDK os.h chain: pull in the BE/LE byte helpers (U4BE, ...)
// and HD-wallet derivation modes (HDW_NORMAL, ...) so test units that
// exercise raw APDU parsers and key-derivation helpers find them.
#include "os_utils.h"
#include "os_seed.h"
#include "os_pic.h"

/**
 * @brief os entry point
 */
void app_main(void);

/**
 * @brief Array length macro (from BOLOS_SDK os_utils.h)
 */
#define ARRAYLEN(array) (sizeof(array) / sizeof(array[0]))

/**
 * @brief Hex-encode bytes as a lowercase null-terminated string.
 *        Mirrors BOLOS_SDK os_utils.h declaration; impl lives in mock.c.
 */
int bytes_to_lowercase_hex(char *out, size_t outl, const void *value, size_t len);

/**
 * @brief All-zero buffer probe (from BOLOS_SDK os_utils.h).
 *        Tests that touch zero-buffer guards must provide an implementation.
 */
bool is_printable_string(const char *str, size_t len);
bool is_displayable_string(const char *str, size_t len);
bool is_zeroes_buffer(const void *buf, size_t n);

// Mirror os_io.h: CUSTOM_IO_APDU_BUFFER_SIZE takes precedence over the
// hardware constant so tests can shrink the buffer without touching the SDK.
#ifndef OS_IO_BUFFER_SIZE
#ifdef CUSTOM_IO_APDU_BUFFER_SIZE
#define OS_IO_BUFFER_SIZE CUSTOM_IO_APDU_BUFFER_SIZE
#else
#define OS_IO_BUFFER_SIZE OS_IO_SEPH_BUFFER_SIZE
#endif
#endif

/**
 * @brief APDU TX buffer (from BOLOS_SDK os_io.h). Storage lives in
 *        mocks/app_globals.c.
 */
extern uint8_t G_io_tx_buffer[OS_IO_BUFFER_SIZE + 1];

/**
 * @brief NVM write (from BOLOS_SDK os_nvm.h). Tests that exercise the
 *        persistent-counter path must wrap or stub this symbol.
 */
void nvm_write(void *dst, void *src, unsigned int len);
