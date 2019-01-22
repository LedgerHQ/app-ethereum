/*******************************************************************************
*   Ledger Ethereum App
*   (c) 2016-2019 Ledger
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
********************************************************************************/

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>

#include "uint256.h"

void array_hexstr(char *strbuf, const void *bin, unsigned int len);

void convertUint256BE(uint8_t *data, uint32_t length, uint256_t *target);

int local_strchr(char *string, char ch);

uint32_t getV(txContent_t *txContent);

#endif /* _UTILS_H_ */