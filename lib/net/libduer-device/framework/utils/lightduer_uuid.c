/**
 * Copyright (2019) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * File: lightduer_uuid.h
 * Auth: Chen Chi (chenchi01@baidu.com)
 * Desc: uuid generation
 */

#include "lightduer_uuid.h"

#include <stdlib.h>
#include "lightduer_lib.h"

void duer_generate_uuid(char *uuid_out)
{
    const char *c = "89ab";
    char *p = uuid_out;
    for (int n = 0; n < 16; ++n) {
        int b = rand() % 255;
        switch (n) {
        case 6:
            sprintf(p, "4%x", b % 15);
            break;
        case 8:
            sprintf(p, "%c%x", c[rand() % DUER_STRLEN(c)], b % 15);
            break;
        default:
            sprintf(p, "%02x", b);
            break;
        }

        p += 2;

        switch (n) {
        case 3:
        case 5:
        case 7:
        case 9:
            *p++ = '-';
            break;
        }
    }
    *p = 0;
}

