#pragma once

// Stub for the production-generated `glyphs.h` that the SDK's ux_nbgl.h
// includes transitively. Production builds emit one declaration per icon
// (`extern const nbgl_icon_details_t C_chain_1_64px;` etc.) from PNG
// assets via the rakelib glyph rules. Tests that exercise UI code with
// ICONGLYPH / ICONHOME defined to a sentinel name (`test_glyph`,
// `test_home_glyph`) need a forward decl visible at both the production
// source's compile time and the test's. Expose them here so any test
// pulling this header gets the references for free.

#include "nbgl_types.h"

extern nbgl_icon_details_t test_glyph;
extern nbgl_icon_details_t test_home_glyph;

// LARGE_LEDGER_ICON maps to C_Ledger_64px (SCREEN_SIZE_WALLET) or
// C_Ledger_14px (Nano). Declare both; the test TU defines whichever applies.
extern const nbgl_icon_details_t C_Ledger_64px;
extern const nbgl_icon_details_t C_Ledger_14px;
