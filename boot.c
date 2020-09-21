/*
 * boot.c
 * copyright (C) 2020/05/25 dora2ios
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <openssl/aes.h>

#include <common.h>
#include <boot.h>
#include <client.h>

int boot_client_n(irecv_client_t client, char* ibss, size_t ibss_sz) {
    int ret;
    printf("\x1b[36mUploading soft DFU\x1b[39m\n");
    ret = irecv_send_buffer(client, (unsigned char*)ibss, ibss_sz, 0);
    if(ret != 0) {
        printf("Failed to upload soft DFU.\n");
        return -1;
    }
    
    ret = irecv_finish_transfer(client);
    if(ret != 0) {
        printf("Failed to execute soft DFU.\n");
        return -1;
    }
    return 0;
}

// boot for 32-bit checkm8 devices
int boot_client(void* buf, size_t sz) {
    int ret;
    if(!client) {
        ret = irecv_get_device();
        if(ret != 0) {
            printf("No device found.");
            return -1;
        }
    }
    
    const struct irecv_device_info* info = irecv_get_device_info(client);
    char* pwnd_str = strstr(info->serial_string, "PWND:[");
    if(!pwnd_str) {
        irecv_close(client);
        printf("Device not in pwned DFU mode.\n");
        return -1;
    }
    
    char* checkm8_str = strstr(info->serial_string, "checkm8");
    if(!checkm8_str) {
        //printf("This device is not in checkm8 pwned DFU mode.\n");
        boot_client_n(client, buf, sz); // jump to normal boot
        free(buf);
        return 0;
    }
    
    if(info->cpid == 0x8960){
        //printf("This device is in checkm8 pwned DFU mode. But this is 64-bit.\n");
        boot_client_n(client, buf, sz); // // jump to normal boot
        free(buf);
        return 0;
    }
    
    unsigned char blank[16];
    bzero(blank, 16);
    
    send_data(client, blank, 16);
    irecv_usb_control_transfer(client, 0x21, 1, 0, 0, NULL, 0, 100);
    irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, blank, 6, 100);
    irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, blank, 6, 100);
    
    printf("\x1b[36mUploading soft DFU\x1b[39m\n");
    size_t len = 0;
    while(len < sz) {
        size_t size = ((sz - len) > 0x800) ? 0x800 : (sz - len);
        size_t sent = irecv_usb_control_transfer(client, 0x21, 1, 0, 0, (unsigned char*)&buf[len], size, 1000);
        if(sent != size) {
            printf("Failed to upload iBSS.\n");
            return -1;
        }
        len += size;
    }
    
   irecv_usb_control_transfer(client, 0xA1, 2, 0xFFFF, 0, buf, 0, 100);
    
    //irecv_close(client);
    return 0;
}
