/*
 * payload.h
 * copyright (C) 2020/05/25 dora2ios
 *
 */

#ifndef payload_H
#define payload_H

#include <stddef.h>

typedef struct checkm8_config {
    uint16_t large_leak;
    uint16_t hole;
    int overwrite_offset;
    uint16_t leak;
    unsigned char* overwrite;
    size_t overwrite_len;
    unsigned char* payload;
    size_t payload_len;
} checkm8_config_t;

int get_payload_configuration(uint16_t cpid, const char* identifier, checkm8_config_t* config);
int gen_limera1n(int cpid, int rom, unsigned char** payload, size_t* payload_len);

#endif
