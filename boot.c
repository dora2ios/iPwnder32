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
#include <iBoot32Patcher/type.h>

typedef struct img3Tag {
    uint32_t magic;            // see below
    uint32_t totalLength;      // length of tag including "magic" and these two length values
    uint32_t dataLength;       // length of tag data
    // ...
} Img3RootHeader;

typedef struct img3File {
    uint32_t magic;       // ASCII_LE("Img3")
    uint32_t fullSize;    // full size of fw image
    uint32_t sizeNoPack;  // size of fw image without header
    uint32_t sigCheckArea;// although that is just my name for it, this is the
    // size of the start of the data section (the code) up to
    // the start of the RSA signature (SHSH section)
    uint32_t ident;       // identifier of image, used when bootrom is parsing images
    // list to find LLB (illb), LLB parsing it to find iBoot (ibot),
    // etc.
    struct img3Tag  tags[];      // continues until end of file
} Img3Header;

typedef struct Unparsed_KBAG_256 {
    uint32_t magic;       // string with bytes flipped ("KBAG" in little endian)
    uint32_t fullSize;    // size of KBAG from beyond that point to the end of it
    uint32_t tagDataSize; // size of KBAG without this 0xC header
    uint32_t cryptState;  // 1 if the key and IV in the KBAG are encrypted with the GID Key
    // 2 is used with a second KBAG for the S5L8920, use is unknown.
    uint32_t aesType;     // 0x80 = aes128 / 0xc0 = aes192 / 0x100 = aes256
    uint8_t encIV_start;    // IV for the firmware file, encrypted with the GID Key
    // ...   // Key for the firmware file, encrypted with the GID Key
} UnparsedKbagAes256_t;

// decrypt image via access the hardware AES.
int aes(irecv_client_t client, uint8_t* IV, uint8_t* Key, void* inbuf, void** outbuf, size_t outsz){
    void* payload;
    void* response;
    size_t payload_sz;
    size_t data_sz;
    size_t response_len;
    uint16_t cpid;
    uint32_t dfu_image_base;
    uint32_t aes_crypto_cmd;
    
    uint8_t decIV[16];
    uint8_t decKey[32];
    
    cpid = irecv_get_cpid();
    
    switch(cpid) {
        case 0x8950:
            dfu_image_base = 0x10000000;
            aes_crypto_cmd = 0x7300+1;
            break;
        case 0x8955:
            dfu_image_base = 0x10000000;
            aes_crypto_cmd = 0x7340+1;
            break;
        default:
            printf("This device is not supported\n");
            return -1;
    }
    
    payload_sz = 0x5c;
    data_sz = 0x30;
    response_len = 64;
    
    payload = malloc(payload_sz);
    bzero(payload, payload_sz);
    response = malloc(response_len);
    bzero(response, response_len);
    unsigned char buf[16];
    bzero(buf, 16);
    
    *(uint32_t*)(payload+ 0) = EXEC;
    *(uint32_t*)(payload+ 4) = EXEC;
    *(uint32_t*)(payload+ 8) = aes_crypto_cmd;
    *(uint32_t*)(payload+16) = AES_DECRYPT_IOS;
    *(uint32_t*)(payload+20) = dfu_image_base + 16 + (7 * 4);
    *(uint32_t*)(payload+24) = dfu_image_base + 16 + (0 * 4);
    *(uint32_t*)(payload+28) = data_sz;
    *(uint32_t*)(payload+32) = AES_GID_KEY;
    for(int i = 0; i < 16; i++){
        *(uint8_t*)(payload+44+i) = IV[i];
    }
    for(int i = 0; i < 32; i++){
        *(uint8_t*)(payload+44+16+i) = Key[i];
    }
    
    send_data(client, buf, 16);
    irecv_usb_control_transfer(client, 0x21, 1, 0, 0, NULL, 0, 100);
    irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buf, 6, 100);
    irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buf, 6, 100);
    
    send_data(client, payload, payload_sz);
    free(payload);
    
    irecv_usb_control_transfer(client, 0xA1, 2, 0xFFFF, 0, response, response_len, 100);
    
    for(int i = 0; i < 16; i++){
        decIV[i] = *(uint8_t*)(response+16+i);
    }
    for(int i = 0; i < 32; i++){
        decKey[i] = *(uint8_t*)(response+16+16+i);
    }
    
    free(response);
    
    uint8_t my_iv[16];
    int i;
    uint8_t bKey[32];
    int keyBits = 32*8;
    for (i = 0; i < 16; i++) {
        my_iv[i] = decIV[i] & 0xff;
    }
    for (i = 0; i < (keyBits/8); i++) {
        bKey[i] = decKey[i] & 0xff;
    }
    AES_KEY dec_key;
    AES_set_decrypt_key(bKey, keyBits, &dec_key);
    
    uint8_t ivec[16];
    memcpy(ivec, my_iv, 16);
    
    int size = outsz;
    *outbuf = malloc(size);
    
    AES_cbc_encrypt((unsigned char *) (inbuf),
                    (unsigned char *) (*outbuf),
                    size,
                    &dec_key,
                    ivec,
                    AES_DECRYPT);
    
    return 0;
}

// check img3 header
int check_img3_file_format(irecv_client_t client, void* file, size_t sz, void** out, size_t* outsz){
    uint32_t Img3header_magic = *(uint32_t*)(file + offsetof(struct img3File, magic));
    switch(Img3header_magic) {
        case ARMv7_VECTOR:
            // Do nothing
            printf("\x1b[36mDecrypted Img3 image\x1b[39m\n");
            *out = malloc(sz);
            *outsz = sz;
            memcpy(*out, file, *outsz);
            return 0;
            break;
            
        case IMG3_HEADER:
            printf("\x1b[36mPacked Img3 image\x1b[39m\n");
            uint32_t ibss_data_start;
            uint32_t tag_header = 0;
            int isKBAG = 0;
            uint8_t IV[16];
            uint8_t Key[32];
            
            uint32_t img3_ident = *(uint32_t*)(file + offsetof(struct img3File, ident));
            //printf("Ident : 0x%08x\n", img3_ident);
            if (img3_ident == IMG3_ILLB || img3_ident == IMG3_IBSS){
                printf("\x1b[35mDetect iBSS/LLB image\x1b[39m\n");
            } else {
                printf("Invalid image\n");
                return -1;
            }
            
            uint32_t img3_fullSize = *(uint32_t*)(file + offsetof(struct img3File, fullSize));
            uint32_t img3_sizeNoPack = *(uint32_t*)(file + offsetof(struct img3File, sizeNoPack));
            
            uint32_t next = img3_fullSize - img3_sizeNoPack; //0x14
            
            for(uint32_t next_tag = next; next_tag < img3_fullSize;){
                uint32_t img3_tag_magic = *(uint32_t*)(file + next_tag + offsetof(struct img3Tag, magic));
                //printf("tag magic: 0x%08x\n", img3_tag_magic);
                uint32_t img3_tag_totalLength = *(uint32_t*)(file + next_tag + offsetof(struct img3Tag, totalLength));
                //printf("tag totalLength: 0x%08x\n", img3_tag_totalLength);
                uint32_t img3_tag_dataLength = *(uint32_t*)(file + next_tag + offsetof(struct img3Tag, dataLength));
                //printf("tag dataLength: 0x%08x\n", img3_tag_dataLength);
                
                if(img3_tag_magic == IMG3_DATA) {
                    tag_header = img3_tag_magic;
                    *outsz = img3_tag_dataLength;
                    ibss_data_start = next_tag + offsetof(struct img3Tag, dataLength) + 4;
                }
                
                if(img3_tag_magic == IMG3_KBAG) {
                    if(*(uint32_t*)(file + next_tag + offsetof(struct Unparsed_KBAG_256, cryptState)) == 1){
                        isKBAG = 1;
                        uint32_t tagDataSize = *(uint32_t*)(file + next_tag + offsetof(struct Unparsed_KBAG_256, tagDataSize));
                        //printf("tagDataSize: 0x%08x\n", tagDataSize);
                        for(int i = 0; i < 16; i++){
                            IV[i] = *(uint8_t*)(file + next_tag + offsetof(struct Unparsed_KBAG_256, encIV_start)+i);
                        }
                        for(int i = 0; i < 32; i++){
                            Key[i] = *(uint8_t*)(file + next_tag + offsetof(struct Unparsed_KBAG_256, encIV_start)+16+i);
                        }
                    }
                }
                
                next_tag += img3_tag_totalLength;
            }
            
            if(tag_header != IMG3_DATA) {
                printf("Invalid image\n");
                return -1;
            }
            
            //printf("decrypted size: 0x%08zx\n", *outsz);
            //printf("decrypted image offset: 0x%08x\n", ibss_data_start);
            
            if(isKBAG == 1 && *(uint32_t*)(file + ibss_data_start) != IMG3_DATA){
                void* fuck = malloc(*outsz);
                memcpy(fuck, file+ibss_data_start, *outsz);
                void* fuck2;
                aes(client, IV, Key, fuck, &fuck2, *outsz);
                memcpy(file+ibss_data_start, fuck2, *outsz);
                
                free(fuck);
                free(fuck2);
            }
            
            *out = malloc(*outsz);
            memcpy(*out, file+ibss_data_start, *outsz);
            
            uint32_t out_magic = *(uint32_t*)(*out + offsetof(struct img3File, magic));
            //printf("out magic: 0x%08x\n", out_magic);
            return 0;
            break;
            
        default:
            printf("Invalid image\n");
            return -1;
            break;
    }
    
    printf("Invalid image\n");
    return -1;
}

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
int boot_client(void* buf, size_t sz, int pwn) {
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
    
    void* ibss;
    size_t ibss_sz;
    unsigned char blank[16];
    bzero(blank, 16);
    
    ret = check_img3_file_format(client, buf, sz, &ibss, &ibss_sz);
    
    if (ret != 0){
        printf("Failed to make soft DFU.\n");
        irecv_close(client);
        return -1;
    }
    
    // iBoot32Patcher
    if(pwn){
        iboot32_pacther_t conf;
        iboot32pacher_init(&conf); // init
        
        conf.rsa = true; // enable rsa patch
        
        printf("\x1b[31mApply RSA patch to image\x1b[39m\n");
        ret = iBoot32Patcher(ibss, ibss_sz, &conf); // patch ibss/illb
        if(ret != 0){
            printf("Failed to patch soft DFU.\n");
            irecv_close(client);
            return -1;
        }
    }
    
    send_data(client, blank, 16);
    irecv_usb_control_transfer(client, 0x21, 1, 0, 0, NULL, 0, 100);
    irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, blank, 6, 100);
    irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, blank, 6, 100);
    
    printf("\x1b[36mUploading soft DFU\x1b[39m\n");
    size_t len = 0;
    while(len < ibss_sz) {
        size_t size = ((ibss_sz - len) > 0x800) ? 0x800 : (ibss_sz - len);
        size_t sent = irecv_usb_control_transfer(client, 0x21, 1, 0, 0, (unsigned char*)&ibss[len], size, 1000);
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
