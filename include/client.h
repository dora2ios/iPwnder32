/*
 * client.h
 * copyright (C) 2020/05/25 dora2ios
 *
 */

#ifndef client_H
#define client_H

#include <irecovery/libirecovery.h>

typedef int(*exploit_supported_t)(irecv_client_t client);
typedef int(*exploit_func_t)(irecv_client_t client);

typedef struct exploit_list {
    char* name;
    exploit_supported_t supported;
    exploit_func_t exploit;
    struct exploit_list* next;
} exploit_list_t;

extern exploit_list_t* exploits;
extern irecv_client_t client;

void exploit_add(char* name, exploit_supported_t supported, exploit_func_t exploit);
int irecv_get_device();
uint16_t irecv_get_cpid();
void exploit_exit();
int do_exploit();
int i3gs_bootrom();

int send_data(irecv_client_t client, unsigned char* data, size_t size);
int reset_counters(irecv_client_t client);
int get_data(irecv_client_t client, int amount);

#endif
