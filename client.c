/*
 * client.c
 * copyright (C) 2020/05/25 dora2ios
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <client.h>

// synackuk's belladonna
// https://github.com/synackuk/belladonna/blob/824363bbc287da3cd2d97cba69aa746db89a86b6/src/exploits/exploits.c#L9
exploit_list_t* exploits = NULL;
void exploit_add(char* name, exploit_supported_t supported, exploit_func_t exploit) {
    exploit_list_t* new_exploit = malloc(sizeof(exploit_list_t));
    new_exploit->name = name;
    new_exploit->supported = supported;
    new_exploit->exploit = exploit;
    new_exploit->next = exploits;
    exploits = new_exploit;
}

void exploit_exit() {
    exploit_list_t* exploit = exploits;
    exploit_list_t* to_free;
    while(exploit != NULL) {
        to_free = exploit;
        exploit = to_free->next;
        free(to_free);
    }
    exploits = NULL;
}

int do_exploit() {
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
    if(pwnd_str) {
        printf("\x1b[31mThis device is already in pwned DFU mode!\x1b[39m\n");
        return 0;
    }
    exploit_list_t* curr = exploits;
    while(curr != NULL) {
        ret = curr->supported(client);
        if(ret == 0){
            ret = curr->exploit(client);
            if(ret != 0) {
                client = NULL;
                printf("Failed to enter Pwned DFU mode.\n");
                return -1;
            }
            irecv_open_with_ecid(&client, 0);
            if(!client) {
                printf("Failed to reconnect to device.\n");
                return -1;
            }
            return 0;
        }
        curr = curr->next;
    }
    printf("Device not supported.\n");
    return -1;
}


int irecv_get_device() {
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

uint16_t irecv_get_cpid(){
    const struct irecv_device_info *devinfo = irecv_get_device_info(client);
    if (devinfo) {
        //printf("CPID: 0x%04x\n", devinfo->cpid);
        return devinfo->cpid;
    }
    
    printf("Could not get device info?!\n");
    return -1;
}

// check bootrom version for 3gs
int i3gs_bootrom() {
    const struct irecv_device_info *devinfo = irecv_get_device_info(client);
    if (devinfo) {
        //printf("%s\n", devinfo->srtg);
        char* i3gs_ptr = strstr(devinfo->srtg, "iBoot-359.3");
        if(i3gs_ptr) {
            return 0; // oldBR
        }
        return 1; // newBR
    }
    
    printf("Could not get device info?!\n");
    return -1;
}

// limera1n helper
int send_data(irecv_client_t client, unsigned char* data, size_t size){
    return irecv_usb_control_transfer(client, 0x21, 1, 0, 0, data, size, 100);
}

int reset_counters(irecv_client_t client){
    return irecv_usb_control_transfer(client, 0x21, 4, 0, 0, NULL, 0, 100);
}

int get_data(irecv_client_t client, int amount){
    unsigned char part[amount];
    return irecv_usb_control_transfer(client, 0xA1, 2, 0, 0, part, amount, 100);
}
