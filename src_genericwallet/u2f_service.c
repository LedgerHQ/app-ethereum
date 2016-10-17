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
#include "u2f_processing.h"
#include "u2f_timer.h"

// not too fast blinking
#define DEFAULT_TIMER_INTERVAL_MS 500

void u2f_reset(u2f_service_t *service, bool keepUserPresence) {
    service->transportState = U2F_IDLE;
    service->runningCommand = false;
    // service->promptUserPresence = false;
    if (service->keepUserPresence) {
        keepUserPresence = true;
        service->keepUserPresence = false;
    }
#ifdef HAVE_NO_USER_PRESENCE_CHECK
    service->keepUserPresence = true;
    service->userPresence = true;
#endif // HAVE_NO_USER_PRESENCE_CHECK
}

void u2f_initialize_service(u2f_service_t *service) {
    service->handleFunction = (u2fHandle_t)u2f_process_message;
    service->timeoutFunction = (u2fTimer_t)u2f_timeout;
    service->timerInterval = DEFAULT_TIMER_INTERVAL_MS;
    u2f_reset(service, false);
    service->promptUserPresence = false;
    service->userPresence = false;
#ifdef HAVE_NO_USER_PRESENCE_CHECK
    service->keepUserPresence = true;
    service->userPresence = true;
#endif // HAVE_NO_USER_PRESENCE_CHECK
}

void u2f_send_direct_response_short(u2f_service_t *service, uint8_t *buffer,
                                    uint16_t len) {
    (void)service;
    uint16_t maxSize = 0;
    switch (service->packetMedia) {
    case U2F_MEDIA_USB:
        maxSize = USB_SEGMENT_SIZE;
        break;
#ifdef HAVE_BLE
    case U2F_MEDIA_BLE:
        maxSize = service->bleMtu;
        break;
#endif
    default:
        PRINTF("Request to send on unsupported media %d\n",
               service->packetMedia);
        break;
    }
    if (len > maxSize) {
        return;
    }
    u2f_io_send(buffer, len, service->packetMedia);
    u2f_io_close_session();
}

void u2f_send_fragmented_response(u2f_service_t *service, uint8_t cmd,
                                  uint8_t *buffer, uint16_t len,
                                  bool resetAfterSend) {
    if (resetAfterSend) {
        service->transportState = U2F_SENDING_RESPONSE;
    }
    service->sending = true;
    service->sendPacketIndex = 0;
    service->sendBuffer = buffer;
    service->sendOffset = 0;
    service->sendLength = len;
    service->sendCmd = cmd;
    service->resetAfterSend = resetAfterSend;
    u2f_continue_sending_fragmented_response(service);
}

void u2f_continue_sending_fragmented_response(u2f_service_t *service) {
    do {
        uint16_t channelHeader =
            (service->transportMedia == U2F_MEDIA_USB ? 4 : 0);
        uint8_t headerSize =
            (service->sendPacketIndex == 0 ? (channelHeader + 3)
                                           : (channelHeader + 1));
        uint16_t maxBlockSize =
            (service->transportMedia == U2F_MEDIA_USB ? USB_SEGMENT_SIZE
                                                      : service->bleMtu);
        uint16_t blockSize = ((service->sendLength - service->sendOffset) >
                                      (maxBlockSize - headerSize)
                                  ? (maxBlockSize - headerSize)
                                  : service->sendLength - service->sendOffset);
        uint16_t dataSize = blockSize + headerSize;
        uint16_t offset = 0;
        // Fragment
        if (service->transportMedia == U2F_MEDIA_USB) {
            os_memset(service->outputBuffer, 0, USB_SEGMENT_SIZE);
            os_memmove(service->outputBuffer + offset, service->channel, 4);
            offset += 4;
        }
        if (service->sendPacketIndex == 0) {
            service->outputBuffer[offset++] = service->sendCmd;
            service->outputBuffer[offset++] = (service->sendLength >> 8);
            service->outputBuffer[offset++] = (service->sendLength & 0xff);
        } else {
            service->outputBuffer[offset++] = (service->sendPacketIndex - 1);
        }
        if (service->sendBuffer != NULL) {
            os_memmove(service->outputBuffer + headerSize,
                       service->sendBuffer + service->sendOffset, blockSize);
        }
        u2f_io_send(service->outputBuffer, dataSize, service->packetMedia);
        service->sendOffset += blockSize;
        service->sendPacketIndex++;
    } while (service->sendOffset != service->sendLength);
    if (service->sendOffset == service->sendLength) {
        u2f_io_close_session();
        service->sending = false;
        if (service->resetAfterSend) {
            u2f_reset(service, false);
        }
    }
}


#endif
