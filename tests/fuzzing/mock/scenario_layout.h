#pragma once

/*
 * Bootstrap prefix layout.
 *
 * scripts/update-scenario-layout.py overwrites these values after the first
 * build. Advanced apps can add extra SCEN_* offsets for app-specific globals,
 * but the minimal template only needs the three definitions below.
 */
#define SCEN_PREFIX_SIZE 235
#define SCEN_CTRL_OFF    0
#define SCEN_CTRL_LEN    16
/* Offset of the 1-byte `appState` global inside the Absolution prefix.
 * Auto-patched by update-scenario-layout.py (driven by [layout].extra_args).
 * Used by scripts/generate_seed_corpus.py to place app-state-dependent seeds
 * in the correct state without hardcoding a prefix-size-dependent offset. */
#define SCEN_APPSTATE_OFF 46
