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
#include "u2f_service.h"
#include "u2f_transport.h"

#define U2F_MASK_COMMAND 0x80
#define U2F_COMMAND_HEADER_SIZE 3

static const uint8_t const BROADCAST_CHANNEL[] = {0xff, 0xff, 0xff, 0xff};
static const uint8_t const FORBIDDEN_CHANNEL[] = {0x00, 0x00, 0x00, 0x00};

void u2f_transport_handle(u2f_service_t *service, uint8_t *buffer,
                          uint16_t size, u2f_transport_media_t media) {
    uint16_t channelHeader = (media == U2F_MEDIA_USB ? 4 : 0);
    uint8_t channel[4] = {0};
    if (media == U2F_MEDIA_USB) {
        os_memmove(channel, buffer, 4);
    }
    // screen_printf("U2F transport\n");
    service->packetMedia = media;
    u2f_io_open_session();
    // If busy, answer immediately
    if (service->noReentry) {
        if ((service->transportState == U2F_PROCESSING_COMMAND) ||
            (service->transportState == U2F_SENDING_RESPONSE)) {
            u2f_response_error(service, ERROR_CHANNEL_BUSY, false, channel);
            goto error;
        }
    }
    if (size < (1 + channelHeader)) {
        // Message to short, abort
        u2f_response_error(service, ERROR_PROP_MESSAGE_TOO_SHORT, true,
                           channel);
        goto error;
    }
    if ((buffer[channelHeader] & U2F_MASK_COMMAND) != 0) {
        if (size < (channelHeader + 3)) {
            // Message to short, abort
            u2f_response_error(service, ERROR_PROP_MESSAGE_TOO_SHORT, true,
                               channel);
            goto error;
        }
        // If waiting for a continuation on a different channel, reply BUSY
        // immediately
        if (media == U2F_MEDIA_USB) {
            if ((service->pendingContinuation) &&
                (os_memcmp(channel, service->lastContinuationChannel, 4) !=
                 0) &&
                (buffer[channelHeader] != U2F_CMD_INIT)) {
                u2f_response_error(service, ERROR_CHANNEL_BUSY, false, channel);
                goto error;
            }
        }
        // If a command was already sent, and we are not processing a INIT
        // command, abort
        if ((service->transportState == U2F_HANDLE_SEGMENTED) &&
            !((media == U2F_MEDIA_USB) &&
              (buffer[channelHeader] == U2F_CMD_INIT))) {
            // Unexpected continuation at this stage, abort
            u2f_response_error(service, ERROR_INVALID_SEQ, true, channel);
            goto error;
        }
        // Check the length
        uint16_t commandLength =
            (buffer[channelHeader + 1] << 8) | (buffer[channelHeader + 2]);
        if (commandLength > (service->messageBufferSize - 3)) {
            // Overflow in message size, abort
            u2f_response_error(service, ERROR_INVALID_LEN, true, channel);
            goto error;
        }
        // Check if the command is supported
        switch (buffer[channelHeader]) {
        case U2F_CMD_PING:
        case U2F_CMD_MSG:
            if (media == U2F_MEDIA_USB) {
                if (u2f_is_channel_broadcast(channel) ||
                    u2f_is_channel_forbidden(channel)) {
                    u2f_response_error(service, ERROR_INVALID_CID, true,
                                       channel);
                    goto error;
                }
            }
            break;
        case U2F_CMD_INIT:
            if (media != U2F_MEDIA_USB) {
                // Unknown command, abort
                u2f_response_error(service, ERROR_INVALID_CMD, true, channel);
                goto error;
            }
            break;
        default:
            // Unknown command, abort
            u2f_response_error(service, ERROR_INVALID_CMD, true, channel);
            goto error;
        }
        // Ok, initialize the buffer
        os_memmove(service->channel, channel, 4);
        service->lastCommandLength = commandLength;
        service->expectedContinuationPacket = 0;
        os_memmove(service->messageBuffer, buffer + channelHeader,
                   size - channelHeader);
        service->transportOffset = size - channelHeader;
        service->transportMedia = media;
    } else {
        // Continuation
        if (size < (channelHeader + 2)) {
            // Message to short, abort
            u2f_response_error(service, ERROR_PROP_MESSAGE_TOO_SHORT, true,
                               channel);
            goto error;
        }
        if (media != service->transportMedia) {
            // Mixed medias
            u2f_response_error(service, ERROR_PROP_MEDIA_MIXED, true, channel);
            goto error;
        }
        if (service->transportState != U2F_HANDLE_SEGMENTED) {
            // Unexpected continuation at this stage, abort
            // TODO : review the behavior is HID only
            if (media == U2F_MEDIA_USB) {
                u2f_reset(service, true);
                goto error;
            } else {
                u2f_response_error(service, ERROR_INVALID_SEQ, true, channel);
                goto error;
            }
        }
        if (media == U2F_MEDIA_USB) {
            // Check the channel
            if (os_memcmp(buffer, service->channel, 4) != 0) {
                u2f_response_error(service, ERROR_CHANNEL_BUSY, true, channel);
                goto error;
            }
        }

        if (buffer[channelHeader] != service->expectedContinuationPacket) {
            // Bad continuation packet, abort
            u2f_response_error(service, ERROR_INVALID_SEQ, true, channel);
            goto error;
        }
        if ((service->transportOffset + (size - (channelHeader + 1))) >
            (service->messageBufferSize - 3)) {
            // Overflow, abort
            u2f_response_error(service, ERROR_INVALID_LEN, true, channel);
            goto error;
        }
        os_memmove(service->messageBuffer + service->transportOffset,
                   buffer + channelHeader + 1, size - (channelHeader + 1));
        service->transportOffset += size - (channelHeader + 1);
        service->expectedContinuationPacket++;
    }
    // See if we can process the command
    if ((media != U2F_MEDIA_USB) &&
        (service->transportOffset >
         (service->lastCommandLength + U2F_COMMAND_HEADER_SIZE))) {
        // Overflow, abort
        u2f_response_error(service, ERROR_INVALID_LEN, true, channel);
        goto error;
    } else if (service->transportOffset >=
               (service->lastCommandLength + U2F_COMMAND_HEADER_SIZE)) {
        // screen_printf("Process command\n");
        service->transportState = U2F_PROCESSING_COMMAND;
        service->handleFunction(service, service->messageBuffer, channel);
    } else {
        // screen_printf("segmented\n");
        service->seqTimeout = 0;
        service->transportState = U2F_HANDLE_SEGMENTED;
        service->pendingContinuation = true;
        os_memmove(service->lastContinuationChannel, channel, 4);
        u2f_io_close_session();
    }
    return;
error:
    if ((media == U2F_MEDIA_USB) && (service->pendingContinuation) &&
        (os_memcmp(channel, service->lastContinuationChannel, 4) == 0)) {
        service->pendingContinuation = false;
    }
    return;
}

void u2f_response_error(u2f_service_t *service, char errorCode, bool reset,
                        uint8_t *channel) {
    uint8_t offset = 0;
    os_memset(service->outputBuffer, 0, MAX_SEGMENT_SIZE);
    if (service->transportMedia == U2F_MEDIA_USB) {
        os_memmove(service->outputBuffer + offset, channel, 4);
        offset += 4;
    }
    service->outputBuffer[offset++] = U2F_STATUS_ERROR;
    service->outputBuffer[offset++] = 0x00;
    service->outputBuffer[offset++] = 0x01;
    service->outputBuffer[offset++] = errorCode;
    u2f_send_direct_response_short(service, service->outputBuffer, offset);
    if (reset) {
        u2f_reset(service, true);
    }
}

bool u2f_is_channel_broadcast(uint8_t *channel) {
    return (os_memcmp(channel, BROADCAST_CHANNEL, 4) == 0);
}

bool u2f_is_channel_forbidden(uint8_t *channel) {
    return (os_memcmp(channel, FORBIDDEN_CHANNEL, 4) == 0);
}

#endif
