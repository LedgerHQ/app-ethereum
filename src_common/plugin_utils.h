/*****************************************************************************
 *   Ledger
 *   (c) 2023 Ledger SAS
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
 *****************************************************************************/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "eth_plugin_interface.h"

#define SELECTOR_SIZE    4
#define PARAMETER_LENGTH 32

void copy_address(uint8_t* dst, const uint8_t* parameter, uint8_t dst_size);

void copy_parameter(uint8_t* dst, const uint8_t* parameter, uint8_t dst_size);

// Get the value from the beginning of the parameter (right to left) and check if the rest of it is
// zero
bool U2BE_from_parameter(const uint8_t* parameter, uint16_t* value);
bool U4BE_from_parameter(const uint8_t* parameter, uint32_t* value);

bool find_selector(uint32_t selector, const uint32_t* array, size_t size, size_t* idx);
