/*
 * demote.c - demote checkm8 devices
 * copyright (C) 2020 dora2ios
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <common.h>
#include <demote.h>
#include <client.h>

size_t read32_demote_sz = 28;
size_t write32_demote_sz = 32;
size_t read64_demote_sz = 40;
size_t write64_demote_sz = 44;

int demote_client() {
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
        printf("This device is not in checkm8 pwned DFU mode.\n");
        return -1;
    }
    
    printf("\x1b[36mFound checkm8 device\x1b[39m\n");
    
    /* demote */
    void* response;
    void* payload_read_demote;
    void* payload_write_demote;
    size_t payload_read_demote_sz;
    size_t payload_write_demote_sz;
    
    uintptr_t demote_register;
    uintptr_t new_demote_register;
    uintptr_t demotion_reg;
    uintptr_t dfu_image_base;
    uint16_t cpid;
    uint8_t bit;
    
    int response_len = 0x14;
    unsigned char buf[16];
    bzero(buf, 16);
    
    response = malloc(response_len);
    
    cpid = irecv_get_cpid();
    switch(cpid) {
        case 0x8947:
            demotion_reg   = 0x3F500000;
            dfu_image_base = 0x34000000;
            bit=32;
            payload_read_demote_sz = read32_demote_sz;
            payload_write_demote_sz = write32_demote_sz;
            break;
        case 0x8950:
            demotion_reg   = 0x3F500000;
            dfu_image_base = 0x10000000;
            bit=32;
            payload_read_demote_sz = read32_demote_sz;
            payload_write_demote_sz = write32_demote_sz;
            break;
        case 0x8955:
            demotion_reg   = 0x3F500000;
            dfu_image_base = 0x10000000;
            bit=32;
            payload_read_demote_sz = read32_demote_sz;
            payload_write_demote_sz = write32_demote_sz;
            break;
        case 0x8960:
            demotion_reg   = 0x000000020E02A000;
            dfu_image_base = 0x0000000180380000;
            bit=64;
            payload_read_demote_sz = read64_demote_sz;
            payload_write_demote_sz = write64_demote_sz;
            break;
        default:
            printf("This device is not supported\n");
            return -1;
    }
    
    payload_read_demote = malloc(payload_read_demote_sz);
    payload_write_demote = malloc(payload_write_demote_sz);
    bzero(payload_read_demote, payload_read_demote_sz);
    bzero(payload_write_demote, payload_write_demote_sz);
    
    switch(bit) {
        case 64: /* 64-bit */
            *((uint32_t*)payload_read_demote+0) = MEMC; /* uint32_t */
            *((uint32_t*)payload_read_demote+1) = MEMC; /* uint32_t */
            *((uint64_t*)payload_read_demote+2) = dfu_image_base + 16 + (0 * 8); // load_base + 16 + (index * 4)
            *((uint64_t*)payload_read_demote+3) = demotion_reg;
            *((uint64_t*)payload_read_demote+4) = 4;
            break;
            
        default: /* 32-bit */
            *((uint32_t*)payload_read_demote+0) = MEMC;
            *((uint32_t*)payload_read_demote+1) = MEMC;
            *((uint32_t*)payload_read_demote+4) = dfu_image_base + 16 + (0 * 4); // load_base + 16 + (index * 4)
            *((uint32_t*)payload_read_demote+5) = demotion_reg;
            *((uint32_t*)payload_read_demote+6) = 4;
            break;
    }
    
    irecv_reset(client);
    irecv_close(client);
    client = NULL;
    usleep(1000);
    irecv_open_with_ecid_and_attempts(&client, 0, 5);
    if(!client) {
        printf("Failed to reconnect to device.\n");
        return -1;
    }
    
    send_data(client, buf, 16);
    
    irecv_usb_control_transfer(client, 0x21, 1, 0, 0, NULL, 0, 100);
    irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buf, 6, 100);
    irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buf, 6, 100);

    send_data(client, payload_read_demote, payload_read_demote_sz);
    
    irecv_usb_control_transfer(client, 0xA1, 2, 0xFFFF, 0, response, 0x14, 100);
    
    demote_register = *((uint32_t*)response+4);
    printf("\x1b[36mDemotion register:\x1b[39m 0x%lx\n", demote_register);
    
    if(demote_register & 1){
        printf("\x1b[43;7;1mAttempting to demote device\x1b[49;0m\n");

        switch(bit) {
            case 64:
                *((uint32_t*)payload_write_demote+0) = MEMC; /* uint32_t */
                *((uint32_t*)payload_write_demote+1) = MEMC; /* uint32_t */
                *((uint64_t*)payload_write_demote+2) = demotion_reg;
                *((uint64_t*)payload_write_demote+3) = dfu_image_base + 16 + (3 * 8); // load_base + 16 + (index * 4)
                *((uint64_t*)payload_write_demote+4) = 4;
                *((uint32_t*)payload_write_demote+(5*2/* uint32_t */)) = demote_register & 0xFFFFFFFE;
                break;
                
            default: /* 32-bit */
                *((uint32_t*)payload_write_demote+0) = MEMC;
                *((uint32_t*)payload_write_demote+1) = MEMC;
                *((uint32_t*)payload_write_demote+4) = demotion_reg;
                *((uint32_t*)payload_write_demote+5) = dfu_image_base + 16 + (3 * 4); // load_base + 16 + (index * 4)
                *((uint32_t*)payload_write_demote+6) = 4;
                *((uint32_t*)payload_write_demote+7) = demote_register & 0xFFFFFFFE;
                break;
        }
        
        irecv_reset(client);
        irecv_close(client);
        client = NULL;
        usleep(1000);
        irecv_open_with_ecid_and_attempts(&client, 0, 5);
        if(!client) {
            printf("Failed to reconnect to device.\n");
            return -1;
        }
        
        send_data(client, buf, 16);
        
        irecv_usb_control_transfer(client, 0x21, 1, 0, 0, NULL, 0, 100);
        irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buf, 6, 100);
        irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buf, 6, 100);
        
        send_data(client, payload_write_demote, payload_write_demote_sz);
        free(payload_write_demote);
        
        irecv_usb_control_transfer(client, 0xA1, 2, 0xFFFF, 0, NULL, 0, 100);
        
        irecv_reset(client);
        irecv_close(client);
        client = NULL;
        usleep(1000);
        irecv_open_with_ecid_and_attempts(&client, 0, 5);
        if(!client) {
            printf("Failed to reconnect to device.\n");
            return -1;
        }
        
        send_data(client, buf, 16);
        
        irecv_usb_control_transfer(client, 0x21, 1, 0, 0, NULL, 0, 100);
        irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buf, 6, 100);
        irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buf, 6, 100);
        
        send_data(client, payload_read_demote, payload_read_demote_sz);
        free(payload_read_demote);
        
        irecv_usb_control_transfer(client, 0xA1, 2, 0xFFFF, 0, response, 0x14, 100);
        
        new_demote_register = *((uint32_t*)response+4);
        
        free(response);
        
        printf("\x1b[35mDemotion register:\x1b[39m 0x%lx\n", new_demote_register);
        if(new_demote_register != demote_register){
            printf("\x1b[36mDemote: \x1b[31;1msuccess\x1b[39;0m\n");
        } else {
            printf("\x1b[36mDemote: \x1b[35mfailed\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
    } else {
        printf("\x1b[31mDevice is already demoted.\x1b[39m\n");
    }
    
    irecv_close(client);
    return 0;
}
