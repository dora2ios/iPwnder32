/*
 * iPwnder32 - bootrom exploit for 32-bit (limera1n/checkm8)
 * copyright (C) 2020/05/25 dora2ios
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <irecovery/libirecovery.h>

irecv_client_t client;
int fast_dfu=0;

#include <client.h>
#include <payload.h>

#include <checkm8.h>
#include <limera1n.h>
//#include <demote.h>
#include <partial.h>
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
    printf("\t\t\t  \x1b[35m--fast\x1b[39m: Enable fast mode (s5l8960x only)\n");
    printf("\t\t\t  \x1b[35m--ibss\x1b[39m: Enter pwnediBSS mode (s5l895Xx only)\n");
    printf("\n");
    //printf("\t-d\t\tDemote device to enable JTAG\n");
    //printf("\n");
    printf("\t-f <image.dfu>\tSend image and boot it\n");
}


#define VERSION     2
#define NIVERSION   1
#define NINIVERSION 1

#define FIXNUM      4

int main(int argc, char** argv) {
    int ret;
    int pwned_dfu;
    int use_pwnibss;
    int boot;
    int disableDRA;
    uint16_t cpid;
    
    FILE* fp = NULL;
    void* file;
    size_t file_len;
    
    /* build ver */
    int MajorVer = VERSION;
    int MinorVer = NIVERSION;
    int MinorMinorVer = NINIVERSION;
    
    int BNver = MinorVer+10;
    int MNuver = (MinorVer*3) + (MinorMinorVer*5) + (FIXNUM);
    
    printf("** iPwnder32 v%d.%d.%d [Build: %d%X%d] by @dora2ios\n",
         MajorVer,      // Major version
         MinorVer,
         MinorMinorVer,
         MajorVer,      // Major version
         BNver,         // Build version
         MNuver         // Minor version
         );
    
    if(argc == 1) {
        usage(argv);
        return -1;
        }
    
    if(!strcmp(argv[1], "-p")) {
        pwned_dfu = 1;
        if(argc == 3){
            if(!strcmp(argv[2], "--fast")) {
                fast_dfu = 1;
            }
            if(!strcmp(argv[2], "--ibss")) {
                use_pwnibss = 1;
            }
        }
        
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
    
    if(pwned_dfu || boot){
        while(irecv_get_device() != 0) {
            printf("Waiting for device in DFU mode\n");
            sleep(1);
        }
    }
    
    irecv_device_t device = NULL;
    
    if(pwned_dfu) {
        //uint16_t cpid = irecv_get_cpid();
        const struct irecv_device_info *devinfo = irecv_get_device_info(client);
        irecv_devices_get_device_by_client(client, &device);
        
        if (devinfo&&device){
            cpid = devinfo->cpid;
            printf("\x1b[7m\x1b[1mDFU device infomation\x1b[0m \x1b[1;4m%s\x1b[0m [%s]\n", device->display_name, device->product_type);
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
    
    // send image
    if(boot) {
        boot_client(file, file_len);
    }
    
    
    
    // real pwndfu
    if(use_pwnibss){
        const char* output = "/tmp/ipwnder32/ibss.tmp";
        
        const char* n41 = "iPhone5,1";
        const char* n42 = "iPhone5,2";
        const char* n48 = "iPhone5,3";
        const char* n49 = "iPhone5,4";
        const char* p101 = "iPad3,4";
        const char* p102 = "iPad3,5";
        const char* p103 = "iPad3,6";
        
        mkdir("/tmp/ipwnder32", 0755);
        FILE *fd = fopen(output, "w");
        if (!fd) {
            printf("error opening image!\n");
            return -1;
        }
        
        const char* url;
        const char* path;
        int set_done;
        if(device->product_type == n41){
            url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/5ada2e6df3f933abde79738967960a27371ce9f3.zip";
            path = "AssetData/boot/Firmware/dfu/iBSS.n41ap.RELEASE.dfu";
            set_done = 1;
        }
        
        if(device->product_type == n42){
            url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/a05a5e2e6c81df2c0412c51462919860b8594f75.zip";
            path = "AssetData/boot/Firmware/dfu/iBSS.n42ap.RELEASE.dfu";
            set_done = 1;
        }
        
        if(device->product_type == n48){
            url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/71ece9ff3c211541c5f2acbc6be7b731d342e869.zip";
            path = "AssetData/boot/Firmware/dfu/iBSS.n48ap.RELEASE.dfu";
            set_done = 1;
        }
        
        if(device->product_type == n49){
            url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/455309571ffb5ca30c977897d75db77e440728c1.zip";
            path = "AssetData/boot/Firmware/dfu/iBSS.n49ap.RELEASE.dfu";
            set_done = 1;
        }
        
        if(device->product_type == p101){
            url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/c0cbed078b561911572a09eba30ea2561cdbefe6.zip";
            path = "AssetData/boot/Firmware/dfu/iBSS.p101ap.RELEASE.dfu";
            set_done = 1;
        }
        
        if(device->product_type == p102){
            url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/3e0efaf1480c74195e4840509c5806cc83c99de2.zip";
            path = "AssetData/boot/Firmware/dfu/iBSS.p102ap.RELEASE.dfu";
            set_done = 1;
        }
        
        if(device->product_type == p103){
            url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/238641fd4b8ca2153c9c696328aeeedabede6174.zip";
            path = "AssetData/boot/Firmware/dfu/iBSS.p103ap.RELEASE.dfu";
            set_done = 1;
        }
        
        if(!set_done){
            printf("This device does not support the -b flags\n");
        }
        
        printf("Downloading image...\n");
        ret = partialzip_download_file(url, path, output);
        if(ret != 0){
            printf("Failed to get image.\n");
            return -1;
        }
        
        fd = fopen(output, "r");
        if (!fd) {
            printf("error opening image!\n");
            return -1;
        }
        
        fseek(fd, 0, SEEK_END);
        size_t sz = ftell(fd);
        fseek(fd, 0, SEEK_SET);
        
        void *buf = malloc(sz);
        if (!buf) {
            printf("error allocating file buffer\n");
            fclose(fd);
            return -1;
        }
        
        fread(buf, sz, 1, fd);
        fclose(fd);
        
        boot_client(buf, sz);
        
        
    }
    
    
    
    return 0;
}
