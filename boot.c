/*
 * boot.c
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ircv.h>

static int send_data(irecv_client_t client, unsigned char* data, size_t size){
    return irecv_usb_control_transfer(client, 0x21, 1, 0, 0, data, size, 100);
}

static int irecv_get_device(irecv_client_t client) {
    if(client) {
        irecv_close(client);
        client = NULL;
    }
    irecv_open_with_ecid(&client, 0);
    if(!client) {
        return -1;
    }
    int mode = 0;
    
    irecv_get_mode(client, &mode);
    if(mode != IRECV_K_DFU_MODE) {
        irecv_close(client);
        client = NULL;
        return -1;
    }
    return 0;
}

int boot_client_n(irecv_client_t client, char* ibss, size_t ibss_sz) {
    int ret;
    printf("\x1b[36mUploading image\x1b[39m\n");
    ret = irecv_send_buffer(client, (unsigned char*)ibss, ibss_sz, 0);
    if(ret != 0) {
        printf("\x1b[31mERROR: Failed to upload image.\x1b[39m\n");
        return -1;
    }
    
    ret = irecv_finish_transfer(client);
    if(ret != 0) {
        printf("\x1b[31mERROR: Failed to execute image.\x1b[39m\n");
        return -1;
    }
    return 0;
}

// boot for 32-bit checkm8 devices
int boot_client(irecv_client_t client, void* buf, size_t sz) {
    int ret;
    if(!client) {
        ret = irecv_get_device(client);
        if(ret != 0) {
            printf("\x1b[31mERROR: No device found.\x1b[39m");
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
    
    char* checkm8_str = strstr(info->serial_string, "ipwnder");
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
    
    printf("\x1b[36mUploading pwned iBSS\x1b[39m\n");
    size_t len = 0;
    size_t size;
    size_t sent;
    while(len < sz) {
        size = ((sz - len) > 0x800) ? 0x800 : (sz - len);
        sent = irecv_usb_control_transfer(client, 0x21, 1, 0, 0, (unsigned char*)&buf[len], size, 1000);
        if(sent != size) {
            printf("\x1b[31mERROR: Failed to upload image.\x1b[39m\n");
            return -1;
        }
        len += size;
    }
    
   irecv_usb_control_transfer(client, 0xA1, 2, 0xFFFF, 0, NULL, 0, 100);
    
    //irecv_close(client);
    return 0;
}
