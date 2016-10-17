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

#ifndef __U2F_TRANSPORT_H__

#define __U2F_TRANSPORT_H__

#include "u2f_service.h"

#define MAX_SEGMENT_SIZE                                                       \
    (USB_SEGMENT_SIZE > BLE_SEGMENT_SIZE ? USB_SEGMENT_SIZE : BLE_SEGMENT_SIZE)

// Shared commands
#define U2F_CMD_PING 0x81
#define U2F_CMD_MSG 0x83

// USB only commands
#define U2F_CMD_INIT 0x86
#define U2F_CMD_LOCK 0x84
#define U2F_CMD_WINK 0x88

// BLE only commands
#define U2F_CMD_KEEPALIVE 0x82
#define KEEPALIVE_REASON_PROCESSING 0x01
#define KEEPALIVE_REASON_TUP_NEEDED 0x02

#define U2F_STATUS_ERROR 0xBF

// Shared errors
#define ERROR_NONE 0x00
#define ERROR_INVALID_CMD 0x01
#define ERROR_INVALID_PAR 0x02
#define ERROR_INVALID_LEN 0x03
#define ERROR_INVALID_SEQ 0x04
#define ERROR_MSG_TIMEOUT 0x05
#define ERROR_OTHER 0x7f
// USB only errors
#define ERROR_CHANNEL_BUSY 0x06
#define ERROR_LOCK_REQUIRED 0x0a
#define ERROR_INVALID_CID 0x0b
#define ERROR_PROP_UNKNOWN_COMMAND 0x80
#define ERROR_PROP_COMMAND_TOO_LONG 0x81
#define ERROR_PROP_INVALID_CONTINUATION 0x82
#define ERROR_PROP_UNEXPECTED_CONTINUATION 0x83
#define ERROR_PROP_CONTINUATION_OVERFLOW 0x84
#define ERROR_PROP_MESSAGE_TOO_SHORT 0x85
#define ERROR_PROP_UNCONSISTENT_MSG_LENGTH 0x86
#define ERROR_PROP_UNSUPPORTED_MSG_APDU 0x87
#define ERROR_PROP_INVALID_DATA_LENGTH_APDU 0x88
#define ERROR_PROP_INTERNAL_ERROR_APDU 0x89
#define ERROR_PROP_INVALID_PARAMETERS_APDU 0x8A
#define ERROR_PROP_INVALID_DATA_APDU 0x8B
#define ERROR_PROP_DEVICE_NOT_SETUP 0x8C
#define ERROR_PROP_MEDIA_MIXED 0x8D

void u2f_transport_handle(u2f_service_t *service, uint8_t *buffer,
                          uint16_t size, u2f_transport_media_t media);
void u2f_response_error(u2f_service_t *service, char errorCode, bool reset,
                        uint8_t *channel);
bool u2f_is_channel_broadcast(uint8_t *channel);
bool u2f_is_channel_forbidden(uint8_t *channel);

#endif
