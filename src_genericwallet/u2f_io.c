#ifdef HAVE_U2F

/*
*******************************************************************************
*   Portable FIDO U2F implementation
*   Ledger Blue specific initialization
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

#include "os_io_seproxyhal.h"

#include "u2f_io.h"
#include "u2f_transport.h"

extern void u2f_reset_display(void);

volatile unsigned char u2fCommandSent = 0;
volatile unsigned char u2fFirstCommand = 0;
volatile unsigned char u2fClosed = 0;

void u2f_io_open_session(void) {
    // PRINTF("u2f_io_open_session\n");
    u2fCommandSent = 0;
    u2fFirstCommand = 1;
    u2fClosed = 0;
}

unsigned char u2fSegment[MAX_SEGMENT_SIZE];

void u2f_io_send(uint8_t *buffer, uint16_t length,
                 u2f_transport_media_t media) {
    if (media == U2F_MEDIA_USB) {
        os_memset(u2fSegment, 0, sizeof(u2fSegment));
    }
    os_memmove(u2fSegment, buffer, length);
    // PRINTF("u2f_io_send\n");
    if (u2fFirstCommand) {
        u2fFirstCommand = 0;
    }
    switch (media) {
    case U2F_MEDIA_USB:
        io_usb_send_apdu_data(u2fSegment, USB_SEGMENT_SIZE);
        break;
#ifdef HAVE_BLE
    case U2F_MEDIA_BLE:
        BLE_protocol_send(buffer, length);
        break;
#endif
    default:
        PRINTF("Request to send on unsupported media %d\n", media);
        break;
    }
}

void u2f_io_close_session(void) {
    // PRINTF("u2f_close_session\n");
    if (!u2fClosed) {
        // u2f_reset_display();
        u2fClosed = 1;
    }
}

#endif
