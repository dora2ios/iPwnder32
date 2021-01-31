/*
 * checkm8.h
 * copyright (C) 2020/05/25 dora2ios
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef CHECKM8_H
#define CHECKM8_H

#include <stdint.h>

int checkm8_32_exploit(irecv_client_t client, irecv_device_t device, const struct irecv_device_info *devinfo);

typedef struct checkm8_32 {
    uint16_t large_leak;
    int overwrite_offset;
    unsigned char* overwrite;
    size_t overwrite_len;
    unsigned char* payload;
    size_t payload_len;
} checkm8_32_t;

int get_payload_configuration(uint16_t cpid, const char* identifier, checkm8_32_t* config);

#endif
