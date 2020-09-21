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
int fast_dfu=0;

#include <client.h>
#include <payload.h>

#include <checkm8.h>
#include <limera1n.h>
#include <demote.h>
#include <boot.h>

void usage(char** argv) {
    printf("iPwnder32 - A tool to exploit bootrom for 32-bit devices\n");
    printf("Usage: %s [options]\n", argv[0]);
    printf("\t-p [flag]\tPut device in pwned DFU mode\n");
    printf("\t\t\t[Support device lists]\n");
    printf("\t\t\t  \x1b[36ms5l8920x\x1b[39m - \x1b[31mlimera1n\x1b[39m\n");
    printf("\t\t\t  \x1b[36ms5l8922x\x1b[39m - \x1b[31mlimera1n\x1b[39m\n");
    printf("\t\t\t  \x1b[36ms5l8930x\x1b[39m - \x1b[31mlimera1n\x1b[39m\n");
    printf("\t\t\t  \x1b[36ms5l895Xx\x1b[39m - \x1b[35mcheckm8\x1b[39m\n");
    printf("\t\t\t  \x1b[36ms5l8960x\x1b[39m - \x1b[35mcheckm8\x1b[39m\n");
    printf("\t\t\t[Additional flag]\n");
    printf("\t\t\t  \x1b[35m-f\x1b[39m: Enable fast mode (s5l8960x only)\n");
    printf("\n");
    printf("\t-d\t\tDemote device to enable JTAG\n");
    printf("\n");
    printf("\t-f <img>\tSend DFU image and boot it\n");
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
        if(argc == 3){
            if(!strcmp(argv[2], "-f")) {
                fast_dfu = 1;
            }
        }
        
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
            printf("\x1b[7m\x1b[1mDFU device infomation\x1b[0m\n");
            printf("CPID:0x%04X CPRV:0x%02X BDID:0x%02X ECID:0x%016llX CPFM:0x%02X SCEP:0x%02X IBFL:0x%02X\n" , devinfo->cpid, devinfo->cprv, devinfo->bdid, devinfo->ecid, devinfo->cpfm, devinfo->scep, devinfo->ibfl);
            if((devinfo->cpfm & 0xFFFFFFFE) == 0x00){
                printf("\x1b[7;1mProduction Mode: \x1b[41mDevelopment\x1b[49;0m\n");
                printf("\x1b[31;1mThis device is development or demoted device.\x1b[39;0m\n");
            }
            printf("SRTG:[%s] ", (devinfo->srtg) ? devinfo->srtg : "N/A");
            char* p = strstr(devinfo->serial_string, "PWND:[");
            if (p) {
                p+=6;
                char* pend = strchr(p, ']');
                if (pend) {
                    printf("PWND:[\x1b[35;1m%.*s\x1b[39;0m]", (int)(pend-p), p);
                }
            }
            printf("\n");
            if(!(devinfo->srtg)){
                 printf("Make sure device is in SecureROM DFU mode and not LLB/iBSS (soft) DFU mode.\n");
                exploit_exit();
                return 0;
            }
            
            
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
            // Apple A6(X)
            checkm8_init();
            ret = 1;
        }
        
        if(cpid == 0x8960){
            // Apple A7
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
