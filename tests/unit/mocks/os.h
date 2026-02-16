#pragma once

#include <bsd/string.h>  // strlcpy, strlcat from libbsd

/**
 * @brief Array length macro (from BOLOS_SDK os_utils.h)
 */
#define ARRAYLEN(array) (sizeof(array) / sizeof(array[0]))
