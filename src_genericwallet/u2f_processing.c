#ifdef HAVE_U2F

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
#include "os.h"
#include "cx.h"
#include "u2f_service.h"
#include "u2f_transport.h"
#include "u2f_processing.h"

void handleApdu(volatile unsigned int *flags, volatile unsigned int *tx);
void u2f_proxy_response(u2f_service_t *service, unsigned int tx);

static const uint8_t SW_SUCCESS[] = {0x90, 0x00};
static const uint8_t SW_PROOF_OF_PRESENCE_REQUIRED[] = {0x69, 0x85};
static const uint8_t SW_BAD_KEY_HANDLE[] = {0x6A, 0x80};

static const uint8_t VERSION[] = {'U', '2', 'F', '_', 'V', '2', 0x90, 0x00};
static const uint8_t DUMMY_ZERO[] = {0x00};

static const uint8_t SW_UNKNOWN_INSTRUCTION[] = {0x6d, 0x00};
static const uint8_t SW_UNKNOWN_CLASS[] = {0x6e, 0x00};
static const uint8_t SW_WRONG_LENGTH[] = {0x67, 0x00};
static const uint8_t SW_INTERNAL[] = {0x6F, 0x00};

static const uint8_t NOTIFY_USER_PRESENCE_NEEDED[] = {
    KEEPALIVE_REASON_TUP_NEEDED};

static const uint8_t PROXY_MAGIC[] = {'w', '0', 'w'};

#define INIT_U2F_VERSION 0x02
#define INIT_DEVICE_VERSION_MAJOR 0
#define INIT_DEVICE_VERSION_MINOR 1
#define INIT_BUILD_VERSION 0
#define INIT_CAPABILITIES 0x00

#define FIDO_CLA 0x00
#define FIDO_INS_ENROLL 0x01
#define FIDO_INS_SIGN 0x02
#define FIDO_INS_GET_VERSION 0x03

#define FIDO_INS_PROP_GET_COUNTER 0xC0 // U2F_VENDOR_FIRST

#define P1_SIGN_CHECK_ONLY 0x07
#define P1_SIGN_SIGN 0x03

#define U2F_ENROLL_RESERVED 0x05
#define SIGN_USER_PRESENCE_MASK 0x01

#define MAX_SEQ_TIMEOUT_MS 500
#define MAX_KEEPALIVE_TIMEOUT_MS 500

static const uint8_t DUMMY_USER_PRESENCE[] = {SIGN_USER_PRESENCE_MASK};


void u2f_handle_enroll(u2f_service_t *service, uint8_t p1, uint8_t p2,
                       uint8_t *buffer, uint16_t length) {
    (void)p1;
    (void)p2;
    (void)buffer;
    if (length != 32 + 32) {
        u2f_send_fragmented_response(service, U2F_CMD_MSG,
                                     (uint8_t *)SW_WRONG_LENGTH,
                                     sizeof(SW_WRONG_LENGTH), true);
        return;
    }
    u2f_send_fragmented_response(service, U2F_CMD_MSG, (uint8_t *)SW_INTERNAL,
                                 sizeof(SW_INTERNAL), true);
}

void u2f_handle_sign(u2f_service_t *service, uint8_t p1, uint8_t p2,
                     uint8_t *buffer, uint16_t length) {
    (void)p1;
    (void)p2;
    (void)length;
    uint8_t keyHandleLength;
    uint8_t i;
    volatile unsigned int flags = 0;
    volatile unsigned int tx = 0;

    if (length < 32 + 32 + 1) {
        u2f_send_fragmented_response(service, U2F_CMD_MSG,
                                     (uint8_t *)SW_WRONG_LENGTH,
                                     sizeof(SW_WRONG_LENGTH), true);
        return;
    }
    if ((p1 != P1_SIGN_CHECK_ONLY) && (p1 != P1_SIGN_SIGN)) {
        u2f_response_error(service, ERROR_PROP_INVALID_PARAMETERS_APDU, true,
                           service->channel);
        return;
    }


    keyHandleLength = buffer[64];
    for (i = 0; i < keyHandleLength; i++) {
        buffer[65 + i] ^= PROXY_MAGIC[i % sizeof(PROXY_MAGIC)];
    }
    // Check magic
    if (length != (32 + 32 + 1 + 5 + buffer[65 + 4])) {
        u2f_send_fragmented_response(service, U2F_CMD_MSG,
                                     (uint8_t *)SW_BAD_KEY_HANDLE,
                                     sizeof(SW_BAD_KEY_HANDLE), true);
    }
    // Check that it looks like an APDU
    os_memmove(G_io_apdu_buffer, buffer + 65, keyHandleLength);
    handleApdu(&flags, &tx);
    if ((flags & IO_ASYNCH_REPLY) == 0) {
        u2f_proxy_response(service, tx);
    }
}

void u2f_handle_get_version(u2f_service_t *service, uint8_t p1, uint8_t p2,
                            uint8_t *buffer, uint16_t length) {
    // screen_printf("U2F version\n");
    (void)p1;
    (void)p2;
    (void)buffer;
    if (length != 0) {
        u2f_send_fragmented_response(service, U2F_CMD_MSG,
                                     (uint8_t *)SW_WRONG_LENGTH,
                                     sizeof(SW_WRONG_LENGTH), true);
        return;
    }
    u2f_send_fragmented_response(service, U2F_CMD_MSG, (uint8_t *)VERSION,
                                 sizeof(VERSION), true);
}

void u2f_handle_cmd_init(u2f_service_t *service, uint8_t *buffer,
                         uint16_t length, uint8_t *channelInit) {
    // screen_printf("U2F init\n");
    uint8_t channel[4];
    (void)length;
    uint16_t offset = 0;
    if (u2f_is_channel_forbidden(channelInit)) {
        u2f_response_error(service, ERROR_INVALID_CID, true, channelInit);
        return;
    }
    if (u2f_is_channel_broadcast(channelInit)) {
        cx_rng(channel, 4);
    } else {
        os_memmove(channel, channelInit, 4);
    }
    os_memmove(service->messageBuffer + offset, buffer, 8);
    offset += 8;
    os_memmove(service->messageBuffer + offset, channel, 4);
    offset += 4;
    service->messageBuffer[offset++] = INIT_U2F_VERSION;
    service->messageBuffer[offset++] = INIT_DEVICE_VERSION_MAJOR;
    service->messageBuffer[offset++] = INIT_DEVICE_VERSION_MINOR;
    service->messageBuffer[offset++] = INIT_BUILD_VERSION;
    service->messageBuffer[offset++] = INIT_CAPABILITIES;
    if (u2f_is_channel_broadcast(channelInit)) {
        os_memset(service->channel, 0xff, 4);
    } else {
        os_memmove(service->channel, channel, 4);
    }
    service->keepUserPresence = true;
    u2f_send_fragmented_response(service, U2F_CMD_INIT, service->messageBuffer,
                                 offset, true);
    // os_memmove(service->channel, channel, 4);
}

void u2f_handle_cmd_ping(u2f_service_t *service, uint8_t *buffer,
                         uint16_t length) {
    // screen_printf("U2F ping\n");
    u2f_send_fragmented_response(service, U2F_CMD_PING, buffer, length, true);
}

void u2f_handle_cmd_msg(u2f_service_t *service, uint8_t *buffer,
                        uint16_t length) {
    // screen_printf("U2F msg\n");
    uint8_t cla = buffer[0];
    uint8_t ins = buffer[1];
    uint8_t p1 = buffer[2];
    uint8_t p2 = buffer[3];
    uint32_t dataLength = (buffer[4] << 16) | (buffer[5] << 8) | (buffer[6]);
    if ((dataLength != (uint16_t)(length - 9)) &&
        (dataLength != (uint16_t)(length - 7))) { // Le is optional
        u2f_send_fragmented_response(service, U2F_CMD_MSG,
                                     (uint8_t *)SW_WRONG_LENGTH,
                                     sizeof(SW_WRONG_LENGTH), true);
        return;
    }
    if (cla != FIDO_CLA) {
        u2f_send_fragmented_response(service, U2F_CMD_MSG,
                                     (uint8_t *)SW_UNKNOWN_CLASS,
                                     sizeof(SW_UNKNOWN_CLASS), true);
        return;
    }
    switch (ins) {
    case FIDO_INS_ENROLL:
        // screen_printf("enroll\n");
        u2f_handle_enroll(service, p1, p2, buffer + 7, dataLength);
        break;
    case FIDO_INS_SIGN:
        // screen_printf("sign\n");
        u2f_handle_sign(service, p1, p2, buffer + 7, dataLength);
        break;
    case FIDO_INS_GET_VERSION:
        // screen_printf("version\n");
        u2f_handle_get_version(service, p1, p2, buffer + 7, dataLength);
        break;
    default:
        // screen_printf("unsupported\n");
        u2f_send_fragmented_response(service, U2F_CMD_MSG,
                                     (uint8_t *)SW_UNKNOWN_INSTRUCTION,
                                     sizeof(SW_UNKNOWN_INSTRUCTION), true);
        return;
    }
}

void u2f_process_message(u2f_service_t *service, uint8_t *buffer,
                         uint8_t *channel) {
    uint8_t cmd = buffer[0];
    uint16_t length = (buffer[1] << 8) | (buffer[2]);
    switch (cmd) {
    case U2F_CMD_INIT:
        u2f_handle_cmd_init(service, buffer + 3, length, channel);
        break;
    case U2F_CMD_PING:
        service->pendingContinuation = false;
        u2f_handle_cmd_ping(service, buffer + 3, length);
        break;
    case U2F_CMD_MSG:
        service->pendingContinuation = false;
        if (!service->noReentry && service->runningCommand) {
            u2f_response_error(service, ERROR_CHANNEL_BUSY, false,
                               service->channel);
            break;
        }
        service->runningCommand = true;
        u2f_handle_cmd_msg(service, buffer + 3, length);
        break;
    }
}

void u2f_timeout(u2f_service_t *service) {
    service->timerNeedGeneralStatus = true;
    if ((service->transportMedia == U2F_MEDIA_USB) &&
        (service->pendingContinuation)) {
        service->seqTimeout += service->timerInterval;
        if (service->seqTimeout > MAX_SEQ_TIMEOUT_MS) {
            service->pendingContinuation = false;
            u2f_response_error(service, ERROR_MSG_TIMEOUT, true,
                               service->lastContinuationChannel);
        }
    }
#ifdef HAVE_BLE
    if ((service->transportMedia == U2F_MEDIA_BLE) &&
        (service->requireKeepalive)) {
        service->keepaliveTimeout += service->timerInterval;
        if (service->keepaliveTimeout > MAX_KEEPALIVE_TIMEOUT_MS) {
            service->keepaliveTimeout = 0;
            u2f_send_fragmented_response(service, U2F_CMD_KEEPALIVE,
                                         (uint8_t *)NOTIFY_USER_PRESENCE_NEEDED,
                                         sizeof(NOTIFY_USER_PRESENCE_NEEDED),
                                         false);
        }
    }
#endif
}

#endif
