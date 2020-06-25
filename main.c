/*
 * iPwnder32 - bootrom exploit for 32-bit (limera1n/checkm8)
 * copyright (C) 2020/05/25 dora2ios
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <irecovery/libirecovery.h>

irecv_client_t client;

#include <client.h>
#include <payload.h>

#include <checkm8.h>
#include <limera1n.h>
#include <demote.h>
#include <boot.h>

void usage(char** argv) {
    printf("usage: %s [options]\n", argv[0]);
    printf("\t-p\t\tput device in pwned DFU mode\n");
    printf("\t\t\t*support device lists\n");
    printf("\t\t\t\t s5l8920x ... limera1n\n");
    printf("\t\t\t\t s5l8922x ... limera1n\n");
    printf("\t\t\t\t s5l8930x ... limera1n\n");
    printf("\t\t\t\t s5l895Xx ... checkm8\n");
  //printf("\t\t\t\t s5l8960x ... checkm8\n");
    printf("\n");
    printf("\t-d\t\tdemote device to enable JTAG\n");
    printf("\n");
    printf("\t-f <ibss/illb>\tenter soft DFU mode\n");
}

int main(int argc, char** argv) {
    int ret;
    int pwned_dfu;
    int demote;
    int boot;
    uint16_t cpid;
    
    FILE* fp = NULL;
    void* file;
    size_t file_len;
    
    if(argc == 1) {
        usage(argv);
        return -1;
        }
    
    if(!strcmp(argv[1], "-p")) {
        pwned_dfu = 1;
    } else if(!strcmp(argv[1], "-d")) {
        demote = 1;
    } else if(!strcmp(argv[1], "-f")) {
        boot = 1;
        
        fp = fopen(argv[2], "rb");
        if(!fp) {
            printf("ERROR: opening %s!\n", argv[2]);
            return -1;
        }
        
        fseek(fp, 0, SEEK_END);
        file_len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        file = (void*)malloc(file_len);
        fread(file, 1, file_len, fp);
        fclose(fp);
        
    } else {
        usage(argv);
        return -1;
    }
    
    if(pwned_dfu || demote || boot){
        while(irecv_get_device() != 0) {
            printf("Waiting for device in DFU mode\n");
            sleep(1);
        }
    }
    
    if(pwned_dfu) {
        //uint16_t cpid = irecv_get_cpid();
        const struct irecv_device_info *devinfo = irecv_get_device_info(client);
        if (devinfo){
            cpid = devinfo->cpid;
            printf("** DFU device infomation\n");
            printf("CPID:0x%04X CPRV:0x%02X BDID:0x%02X ECID:0x%016llX CPFM:0x%02X SCEP:0x%02X IBFL:0x%02X\n" , devinfo->cpid, devinfo->cprv, devinfo->bdid, devinfo->ecid, devinfo->cpfm, devinfo->scep, devinfo->ibfl);
            printf("SRTG:[%s] ", (devinfo->srtg) ? devinfo->srtg : "N/A");
            char* p = strstr(devinfo->serial_string, "PWND:[");
            if (p) {
                p+=6;
                char* pend = strchr(p, ']');
                if (pend) {
                    printf("PWND:[%.*s]", (int)(pend-p), p);
                }
            }
            printf("\n");
            
        } else {
            printf("Could not get device info?!\n");
        }
        
        if(!cpid){
            printf("Failed to get CPID from DFU device.\n");
            return -1;
        }
        
        ret = 0;
        if((cpid|0xf) == (0x8920|0xf)){
            // iPhone 3GS(old/new), iPod touch 3G
            limera1n_init();
            ret = 1;
        }
        
        if(cpid == 0x8930){
            // Apple A4
            limera1n_init();
            ret = 1;
        }
        
        if((cpid|0xf) == (0x8950|0xf)){
            // iPhone 5/5c, iPad 4
            checkm8_init();
            ret = 1;
        }
        
        if(cpid == 0x8960){
            printf("exploiting with checkm8 [BETA]\n");
            checkm8_init();
            ret = 1;
        }
        
        if(ret != 1){
            printf("This device is not supported.\n");
        }
        
        ret = do_exploit();
        if (ret != 0) {
            exploit_exit();
            printf("Failed to enter Pwned DFU mode.\n");
            return -1;
        }
        
        exploit_exit();
    }
    
    if(demote) {
        demote_client();
    }
    
    if(boot) {
        boot_client(file, file_len);
    }
    
    
    return 0;
}
