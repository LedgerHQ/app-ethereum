/*******************************************************************************
 *   Ledger Ethereum App
 *   (c) 2025 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ********************************************************************************/

#pragma once

#ifdef HAVE_GATING_SUPPORT

#include <stdint.h>
#include <stdbool.h>

uint16_t handle_gating(uint8_t p1, uint8_t p2, const uint8_t *data, uint8_t length);

void clear_gating(void);
bool set_gating_warning(void);

#endif  // HAVE_GATING_SUPPORT
