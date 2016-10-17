/*
*******************************************************************************
*   Portable FIDO U2F implementation
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*   limitations under the License.
********************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifndef __U2F_TIMER_H__

#define __U2F_TIMER_H__

typedef void (*u2fTimer_t)(struct u2f_service_t *service);

void u2f_timer_init(void);
void u2f_timer_register(uint32_t timerMs, u2fTimer_t timerCallback);
void u2f_timer_cancel(void);

#endif
