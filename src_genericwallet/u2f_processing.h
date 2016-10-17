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

#ifndef __U2F_PROCESSING_H__

#define __U2F_PROCESSING_H__

void u2f_process_message(u2f_service_t *service, uint8_t *buffer,
                         uint8_t *channel);
void u2f_timeout(u2f_service_t *service);

void u2f_handle_ux_callback(u2f_service_t *service);

#endif
