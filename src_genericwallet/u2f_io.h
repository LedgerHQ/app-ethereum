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

#ifndef __U2F_IO_H__

#define __U2F_IO_H__

#include "u2f_service.h"

#define EXCEPTION_DISCONNECT 0x80

void u2f_io_open_session(void);
void u2f_io_send(uint8_t *buffer, uint16_t length, u2f_transport_media_t media);
void u2f_io_close_session(void);

#endif
