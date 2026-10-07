#include <stdio.h>
#include "time_format.h"

static bool get_time_struct(const time_t *timestamp, struct tm *tstruct) {
    if (gmtime_r(timestamp, tstruct) == NULL) {
        return false;
    }
    return true;
}

bool time_format_to_yyyymmdd(const time_t *timestamp, char *out, size_t out_size) {
    struct tm tstruct;

    if (!get_time_struct(timestamp, &tstruct)) {
        return false;
    }
    snprintf(out,
             out_size,
             "%04d-%02d-%02d",
             tstruct.tm_year + 1900,
             tstruct.tm_mon + 1,
             tstruct.tm_mday);
    return true;
}

bool time_format_to_utc(const time_t *timestamp, char *out, size_t out_size) {
    struct tm tstruct;
    int shown_hour;

    if (!get_time_struct(timestamp, &tstruct)) {
        return false;
    }
    if (tstruct.tm_hour == 0) {
        shown_hour = 12;
    } else {
        shown_hour = tstruct.tm_hour;
        if (shown_hour > 12) {
            shown_hour -= 12;
        }
    }
    snprintf(out,
             out_size,
             "%04d-%02d-%02d\n%02d:%02d:%02d %s UTC",
             tstruct.tm_year + 1900,
             tstruct.tm_mon + 1,
             tstruct.tm_mday,
             shown_hour,
             tstruct.tm_min,
             tstruct.tm_sec,
             (tstruct.tm_hour < 12) ? "AM" : "PM");
    return true;
}
