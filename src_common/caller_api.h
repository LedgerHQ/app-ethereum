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

#pragma once

#ifdef HAVE_NBGL
#include "ux.h"
#endif

typedef enum { CALLER_TYPE_CLONE, CALLER_TYPE_PLUGIN } e_caller_type;

typedef struct caller_app_t {
    const char *name;
#ifdef HAVE_NBGL
    const nbgl_icon_details_t *icon;
#endif
    char type;  // does not have to be set by the caller app
} caller_app_t;

extern caller_app_t *caller_app;
