#pragma once

typedef enum { CALLER_TYPE_CLONE, CALLER_TYPE_PLUGIN } e_caller_type;

typedef struct caller_app_t {
    const char *name;
#ifdef HAVE_NBGL
    const nbgl_icon_details_t *icon;
#endif
    char type;  // does not have to be set by the caller app
} caller_app_t;

extern caller_app_t *caller_app;
