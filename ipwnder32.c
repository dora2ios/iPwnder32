/*
 * ipwnder32.c
 * copyright (C) 2020/01/22 dora2ios
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
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <ircv.h>
#include <checkm8.h>
#include <limera1n.h>
#include <partial.h>
#include <boot.h>

#define VERSION     3
#define NIVERSION   1
#define NINIVERSION 1

#define FIXNUM      130

irecv_client_t client;

// Log

static int empty(void){
    return 0;
}
#ifdef HAVE_DEBUG
#define DEBUG_(...) printf(__VA_ARGS__)
int have_dev = 1;
#else
#define DEBUG_(...) empty()
int have_dev;
#endif

// header
int iboot32patcher(void *buff, size_t sz);

static int irecv_get_device() {
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

static void print_devinfo(irecv_device_t device, const struct irecv_device_info *devinfo){
    printf("\x1b[7m\x1b[1mDFU device infomation\x1b[0m \x1b[1;4m%s\x1b[0m [%s]\n", device->display_name, device->product_type);
    printf("CPID:0x%04X CPRV:0x%02X BDID:0x%02X ECID:0x%016llX CPFM:0x%02X SCEP:0x%02X IBFL:0x%02X\n" , devinfo->cpid, devinfo->cprv, devinfo->bdid, devinfo->ecid, devinfo->cpfm, devinfo->scep, devinfo->ibfl);
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
    
}

static int dl_file(const char* url, const char* path, const char* realpath){
    int r;
    printf("Downloading image: %s ...\n", realpath);
    r = partialzip_download_file(url, path, realpath);
    if(r != 0){
        printf("\x1b[31mERROR: Failed to get image.\x1b[39m\n");
        return -1;
    }
    return 0;
}

const char* n41 = "iPhone5,1";
const char* n42 = "iPhone5,2";
const char* n48 = "iPhone5,3";
const char* n49 = "iPhone5,4";
const char* p101 = "iPad3,4";
const char* p102 = "iPad3,5";
const char* p103 = "iPad3,6";

#ifndef IPHONEOS_ARM
const char *n41_ibss = "image3/ibss.n41";
const char *n42_ibss = "image3/ibss.n42";
const char *n48_ibss = "image3/ibss.n48";
const char *n49_ibss = "image3/ibss.n49";
const char *p101_ibss = "image3/ibss.p101";
const char *p102_ibss = "image3/ibss.p102";
const char *p103_ibss = "image3/ibss.p103";
#ifdef HAVE_HOOKER
const char *n42_ibecX = "image3/ibecX.n42";
#endif
#else
const char *n41_ibss = "/var/mobile/image3/ibss.n41";
const char *n42_ibss = "/var/mobile/image3/ibss.n42";
const char *n48_ibss = "/var/mobile/image3/ibss.n48";
const char *n49_ibss = "/var/mobile/image3/ibss.n49";
const char *p101_ibss = "/var/mobile/image3/ibss.p101";
const char *p102_ibss = "/var/mobile/image3/ibss.p102";
const char *p103_ibss = "/var/mobile/image3/ibss.p103";
#ifdef HAVE_HOOKER
const char *n42_ibecX = "/var/mobile/image3/ibecX.n42";
#endif
#endif

static int init_dl(){
    const char* url;
    const char* path;
    const char* realpath;
    FILE *fd;
    int r;
    
    realpath = n41_ibss;
    fd = fopen(realpath, "r");
    if(!fd){
        url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/5ada2e6df3f933abde79738967960a27371ce9f3.zip";
        path = "AssetData/boot/Firmware/dfu/iBSS.n41ap.RELEASE.dfu";
        if(dl_file(url, path, realpath) != 0) return -1;
    }
    
    realpath = n42_ibss;
    fd = fopen(realpath, "r");
    if(!fd){
        url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/a05a5e2e6c81df2c0412c51462919860b8594f75.zip";
        path = "AssetData/boot/Firmware/dfu/iBSS.n42ap.RELEASE.dfu";
        if(dl_file(url, path, realpath) != 0) return -1;
    }
    
    realpath = n48_ibss;
    fd = fopen(realpath, "r");
    if(!fd){
        url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/71ece9ff3c211541c5f2acbc6be7b731d342e869.zip";
        path = "AssetData/boot/Firmware/dfu/iBSS.n48ap.RELEASE.dfu";
        if(dl_file(url, path, realpath) != 0) return -1;
    }
    
    realpath = n49_ibss;
    fd = fopen(realpath, "r");
    if(!fd){
        url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/455309571ffb5ca30c977897d75db77e440728c1.zip";
        path = "AssetData/boot/Firmware/dfu/iBSS.n49ap.RELEASE.dfu";
        if(dl_file(url, path, realpath) != 0) return -1;
    }
    
    realpath = p101_ibss;
    fd = fopen(realpath, "r");
    if(!fd){
        url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/c0cbed078b561911572a09eba30ea2561cdbefe6.zip";
        path = "AssetData/boot/Firmware/dfu/iBSS.p101ap.RELEASE.dfu";
        if(dl_file(url, path, realpath) != 0) return -1;
    }
    
    realpath = p102_ibss;
    fd = fopen(realpath, "r");
    if(!fd){
        url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/3e0efaf1480c74195e4840509c5806cc83c99de2.zip";
        path = "AssetData/boot/Firmware/dfu/iBSS.p102ap.RELEASE.dfu";
        if(dl_file(url, path, realpath) != 0) return -1;
    }
    
    realpath = p103_ibss;
    fd = fopen(realpath, "r");
    if(!fd){
        url = "http://appldnld.apple.com/iOS7.1/031-4897.20140627.JCWhk/238641fd4b8ca2153c9c696328aeeedabede6174.zip";
        path = "AssetData/boot/Firmware/dfu/iBSS.p103ap.RELEASE.dfu";
        if(dl_file(url, path, realpath) != 0) return -1;
    }
    
#ifdef HAVE_HOOKER
    realpath = n42_ibecX;
    fd = fopen(realpath, "r");
    if(!fd){
        url = "https://updates.cdn-apple.com/2019/ios/091-24535-20190722-93574A92-9931-11E9-B99A-60D0A77C2E40/com_apple_MobileAsset_SoftwareUpdate/902d4bef678d5819577812ba216d3750299f63c3.zip";
        path = "AssetData/boot/Firmware/dfu/iBEC.iphone5.RELEASE.dfu";
        if(dl_file(url, path, realpath) != 0) return -1;
    }
#endif
    
    
    return 0;
}

static void usage(char** argv) {
    printf("iPwnder32 - A tool to exploit bootrom for 32-bit devices\n");
    printf("Usage: %s [options]\n", argv[0]);
    printf("\t-p [flag]\tPut device in pwned (soft)DFU mode\n");
    printf("\t\x1b[31m<NOTE>\x1b[39m If you use s5l895Xx, your device will automatically load pwned iBSS.\n");
    printf("\t\t\t[Support device lists]\n");
    printf("\t\t\t  \x1b[36ms5l8920x\x1b[39m - \x1b[31mlimera1n\x1b[39m\n");
    printf("\t\t\t  \x1b[36ms5l8922x\x1b[39m - \x1b[31mlimera1n\x1b[39m\n");
    printf("\t\t\t  \x1b[36ms5l8930x\x1b[39m - \x1b[31mlimera1n\x1b[39m\n");
    printf("\t\t\t  \x1b[36ms5l895Xx\x1b[39m - \x1b[35mcheckm8\x1b[39m\n");
    printf("\t\t\t  \x1b[36ms5l8960x\x1b[39m - \x1b[35mcheckm8\x1b[39m\n");
    printf("\t\t\t[Additional flag]\n");
    printf("\t\t\t  \x1b[35m--noibss\x1b[39m: Do not enter pwnediBSS (s5l895Xx only)\n");
    printf("\n");
    printf("\t-f <img>\tSend image and boot it\n");
    printf("\n");
#ifdef HAVE_HOOKER
    printf("\t-t\t\tJailbreak iOS 10 with 32-bit devices [TBX32]\n");
    printf("\t\t\t[Support device lists]\n");
    printf("\t\t\t  \x1b[36miPhone 5 [iPhone5,2]\x1b[39m - \x1b[35m(iOS 10.3.4)\x1b[39m\n");
    printf("\n");
#endif
}


int main(int argc, char** argv) {
    int r;
    const char* ibsspath;
    const char* ibecpath;
    
    int pwndfu=0;
#ifdef HAVE_HOOKER
    int TBX=0;
#endif
    int pwnibss=0;
    int no_pwnibss=0;
    int send=0;
    
    FILE* fp = NULL;
    void* file;
    size_t file_len;
    
    /* build ver */
    int MajorVer = VERSION;
    int MinorVer = NIVERSION;
    int MinorMinorVer = NINIVERSION;
    
    int BNver = MinorVer+10;
    int MNuver = (MinorVer*3) + (MinorMinorVer*5) + (FIXNUM);
    
    printf("\x1b[1m** iPwnder32 - %s v%d.%d.%d [%d%X%d]\x1b[0m by @dora2ios\n",
         have_dev ? "DEBUG":"RELEASE",
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
        pwndfu = 1;
        if(argc >= 3){
            if(!strcmp(argv[2], "--noibss")) {
                no_pwnibss = 1;
            }
        }
#ifdef HAVE_HOOKER
    } else if(!strcmp(argv[1], "-t")) {
        pwndfu = 1;
        TBX = 1;
#endif
    } else if(!strcmp(argv[1], "-f")) {
        send = 1;
        
        fp = fopen(argv[2], "rb");
        if(!fp) {
            printf("\x1b[31;1mERROR: opening %s!\x1b[39;0m\n", argv[2]);
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
    
    // get device
    int i = 0;
    printf("\x1b[36mWaiting for device in DFU mode...\x1b[39m\n");
    while(irecv_get_device() != 0) {
#ifdef HAVE_DEBUG2
        DEBUG_("Waiting for device in DFU mode: %d\n", i+1);
        sleep(1);
        i++;
        if(i>=5){
            printf("\x1b[31mERROR: Failed to get DFU device.\x1b[39m\n");
            return -1;
        }
#endif
    }
    
    irecv_device_t device = NULL;
    const struct irecv_device_info *devinfo = irecv_get_device_info(client);
    irecv_devices_get_device_by_client(client, &device);
    
    if (devinfo && device){
        print_devinfo(device, devinfo);
        
    } else {
        printf("\x1b[31mERROR: Failed to get device info.\x1b[39m\n");
        irecv_close(client);
        return -1;
    }
    
    if(!devinfo->cpid){
        printf("\x1b[31mERROR: Failed to get CPID form DFU device.\x1b[39m\n");
        irecv_close(client);
        return -1;
    }
    
    
    if((devinfo->cpid|0xf) == (0x8950|0xf) && no_pwnibss != 1){
        // init
        char *outdir = "image3/";
        DIR *d = opendir(outdir);
        if (!d) {
            printf("Making directory: %s\n", outdir);
            r = mkdir("image3/", 0755);
            if(r != 0){
                printf("\x1b[31;1mERROR: opening dir: %s!\x1b[39;0m\n", outdir);
            }
        }
        if(init_dl() != 0){
            return -1;
        }
    }
    
    
    if(send == 1){
        // ibss/illb
        if(!(devinfo->srtg)){
            printf("\x1b[31mERROR: This device is in soft (LLB/iBSS) DFU mode.\x1b[39m\n");
            printf("\x1b[31mplease put device in normal DFU mode.\x1b[39m\n");
            irecv_close(client);
            return 0;
        }
        
        if((devinfo->cpid|0xf) == (0x8950|0xf)){
            boot_client(client, file, file_len);
        } else {
            boot_client_n(client, file, file_len);
        }
        
    }
    
    
    if(pwndfu == 1){
        
        // !ibss/illb
        if(!(devinfo->srtg)){
            printf("\x1b[31mERROR: This device is in soft (LLB/iBSS) DFU mode.\x1b[39m\n");
            printf("\x1b[31mplease put device in normal DFU mode.\x1b[39m\n");
            irecv_close(client);
            return 0;
        }
        
        if((devinfo->cpid|0xf) == (0x8920|0xf) || devinfo->cpid == 0x8930){
            // iPhone 3GS(old/new), iPod touch 3G
            limera1n_exploit(client, device, devinfo);
        } else if((devinfo->cpid|0xf) == (0x8950|0xf)){
            //printf("checkm8_32\n");
            checkm8_32_exploit(client, device, devinfo);
            pwnibss = 1;
        } else if(devinfo->cpid == 0x8960){
            //printf("checkm8_32\n");
            checkm8_32_exploit(client, device, devinfo);
        } else {
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
        }
        
    }
    
    
    
    if(pwnibss == 1 && no_pwnibss != 1){
        client = NULL;
        usleep(100);
        irecv_open_with_ecid_and_attempts(&client, 0, 5);
        if(!client) {
            printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
            return -1;
        }
        
        devinfo = irecv_get_device_info(client);
        irecv_devices_get_device_by_client(client, &device);
        
        
        const char *path;
        FILE *fd;
        
        if(device->product_type == n41){
            path = n41_ibss;
        }
        
        if(device->product_type == n42){
            path = n42_ibss;
        }
        
        if(device->product_type == n48){
            path = n48_ibss;
        }
        
        if(device->product_type == n49){
            path = n49_ibss;
        }
        
        if(device->product_type == p101){
            path = p101_ibss;
        }
        
        if(device->product_type == p102){
            path = p102_ibss;
        }
        
        if(device->product_type == p103){
            path = p103_ibss;
        }
        
        if(!path){
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
        fd = fopen(path, "r");
        if (!fd) {
            printf("\x1b[31;1mERROR: opening image: %s!\x1b[39;0m\n", path);
            irecv_close(client);
            return -1;
        }
        
        fseek(fd, 0, SEEK_END);
        size_t sz = ftell(fd);
        fseek(fd, 0, SEEK_SET);
        
        void *buf = malloc(sz);
        if (!buf) {
            printf("\x1b[31mERROR: allocating file buffer!\x1b[39m\n");
            fclose(fd);
            irecv_close(client);
            return -1;
        }
        
        fread(buf, sz, 1, fd);
        fclose(fd);
        
        boot_client(client, buf, sz);
        free(buf);
        
#ifdef HAVE_HOOKER
        if(TBX != 1){
#endif
            client = NULL;
            usleep(1000);
            irecv_open_with_ecid_and_attempts(&client, 0, 10);
            if(!client) {
                printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
                return -1;
            }
            printf("\x1b[31;1mDevice is now in pwned iBSS mode!\x1b[39;0m\n");
            irecv_close(client);
#ifdef HAVE_HOOKER
        }
#endif
    }
    
    
#ifdef HAVE_HOOKER
    if(TBX == 1 && (devinfo->cpid|0xf) == (0x8950|0xf)){
        client = NULL;
        usleep(1000);
        irecv_open_with_ecid_and_attempts(&client, 0, 10);
        if(!client) {
            printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
            return -1;
        }
        
        devinfo = irecv_get_device_info(client);
        irecv_devices_get_device_by_client(client, &device);
        
        if((devinfo->srtg)){
            printf("\x1b[31mERROR: This device is not in soft (iBSS) DFU mode.\x1b[39m\n");
            irecv_close(client);
            return 0;
        }
        
        const char *path;
        FILE *fd;
        
        if(device->product_type == n41){
            //path = n41_ibecX;
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
        if(device->product_type == n42){
            path = n42_ibecX;
        }
        
        if(device->product_type == n48){
            //path = n48_ibecX;
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
        if(device->product_type == n49){
            //path = n49_ibecX;
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
        if(device->product_type == p101){
            //path = p101_ibecX;
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
        if(device->product_type == p102){
            //path = p102_ibecX;
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
        if(device->product_type == p103){
            //path = p103_ibecX;
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
        if(!path){
            printf("\x1b[31mERROR: This device is not supported.\x1b[39m\n");
            irecv_close(client);
            return -1;
        }
        
        
        fd = fopen(path, "r");
        if (!fd) {
            printf("\x1b[31;1mERROR: opening image: %s!\x1b[39;0m\n", path);
            return -1;
        }
        
        fseek(fd, 0, SEEK_END);
        size_t sz = ftell(fd);
        fseek(fd, 0, SEEK_SET);
        
        void *buf = malloc(sz);
        if (!buf) {
            printf("\x1b[31mERROR: allocating file buffer!\x1b[39m\n");
            fclose(fd);
            return -1;
        }
        
        fread(buf, sz, 1, fd);
        fclose(fd);
        
        uint32_t data_sz = *(uint32_t*)(buf+0x3C);
        DEBUG_("DATA size: %x\n", data_sz);
        
        void *buf2 = malloc(data_sz);
        if (!buf2) {
            printf("\x1b[31mERROR: allocating file buffer!\x1b[39m\n");
            return -1;
        }
        
        memcpy(buf2, buf+0x40, data_sz);
        iboot32patcher(buf2, data_sz);
        memcpy(buf+0x40, buf2, data_sz);
        DEBUG_("patch: done\n");
        free(buf2);
        
        r = boot_client_n(client, (char*)buf, sz);
        if(r != 0) {
            printf("\x1b[31mERROR: Failed to enter Recovery Mode.\x1b[39m\n");
            return -1;
        }
        free(buf);
        
        
        client = NULL;
        usleep(1000);
        irecv_open_with_ecid_and_attempts(&client, 0, 15);
        if(!client) {
            printf("\x1b[31mERROR: Failed to reconnect to device.\x1b[39m\n");
            return -1;
        }
        
        
        devinfo = irecv_get_device_info(client);
        irecv_devices_get_device_by_client(client, &device);
        
        if (devinfo&&device){
            int mode = 0;
            irecv_get_mode(client, &mode);
            if(mode == IRECV_K_RECOVERY_MODE_1||
               mode == IRECV_K_RECOVERY_MODE_2||
               mode == IRECV_K_RECOVERY_MODE_3||
               mode == IRECV_K_RECOVERY_MODE_4) {
                
#include "hooker.h"
                
                r = irecv_send_buffer(client, (unsigned char*)hooker, hooker_sz, 0);
                if(r != 0) {
                    printf("\x1b[31mERROR: Failed to send payload.\x1b[39m\n");
                    return -1;
                }
                
                printf("\x1b[31;1mBooting...\x1b[39;0m\n");
                
                r = irecv_send_command(client, "go");
                if(r != 0) {
                    printf("\x1b[31mERROR: Failed to execute payload.\x1b[39m\n");
                    return -1;
                }
                
                DEBUG_("Well Done??\n");
                
            } else {
                printf("\x1b[31mERROR: Failed to enter Recovery Mode.\x1b[39m\n");
                irecv_close(client);
                client = NULL;
                return -1;
            }
            
        } else {
            printf("\x1b[31mERROR: Failed to get device info.\x1b[39m\n");
        }
        
    }
#endif
    // end
    //irecv_close(client);
    return 0;
}
