#pragma once

#include <stdbool.h>
#include <time.h>
#include <stddef.h>

bool time_format_to_yyyymmdd(const time_t *timestamp, char *out, size_t out_size);
bool time_format_to_utc(const time_t *timestamp, char *out, size_t out_size);
