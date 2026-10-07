#pragma once

#include <stdint.h>
#include <stdbool.h>

// Trimmed-down stand-ins for the SDK's nbgl_contentTagValue_t /
// nbgl_contentTagValueList_t. Only the fields actually touched by app
// code in host tests are present; tests that need richer struct content
// keep their own override.
typedef struct {
    const char *item;
    const char *value;
} nbgl_contentTagValue_t;

typedef struct {
    const nbgl_contentTagValue_t *pairs;
    uint8_t nbPairs;
    bool wrapping;
} nbgl_contentTagValueList_t;

// nbgl_icon_details_t comes from the SDK's nbgl_types.h, which is
// transitively pulled in by network.h. Don't redefine it here.

typedef enum {
    NO_TYPE_WARNING = 0,
    CENTERED_INFO_WARNING,
    QRCODE_WARNING,
    BAR_LIST_WARNING
} nbgl_genericDetailsType_t;

// Minimal stand-in matching the fields used in cmd_get_gating.c.
typedef struct {
    const char *url;
    const char *text1;
    const char *text2;
    bool centered;
} nbgl_layoutQRCode_t;

struct nbgl_icon_details_s;

typedef struct {
    const char *title;
    nbgl_genericDetailsType_t type;
    union {
#ifdef NBGL_QRCODE
        nbgl_layoutQRCode_t qrCode;
#endif
        const void *_reserved;
    };
} nbgl_genericDetails_t;

typedef struct {
    const struct nbgl_icon_details_s *icon;
    const char *title;
    const char *description;
    const char *buttonText;
    const char *footerText;
    const nbgl_genericDetails_t *details;
} nbgl_preludeDetails_t;

typedef enum {
    W3C_ISSUE_WARN = 0,
    W3C_RISK_DETECTED_WARN,
    W3C_THREAT_DETECTED_WARN,
    W3C_NO_THREAT_WARN,
    BLIND_SIGNING_WARN,
    GATED_SIGNING_WARN,
    NB_WARNING_TYPES
} nbgl_warningType_t;

typedef struct {
    uint32_t predefinedSet;
    const char *dAppProvider;
    const char *reportUrl;
    const char *reportProvider;
    const char *providerMessage;
    const void *introDetails;
    const void *reviewDetails;
    const void *info;
    const void *introTopRightIcon;
    const void *reviewTopRightIcon;
    const void *prelude;
} nbgl_warning_t;

// Storage lives in mocks/app_globals.c (weak); tests that drive the
// warning UI screens poke its fields directly.
extern nbgl_warning_t warning;
