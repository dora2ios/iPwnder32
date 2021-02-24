/*
 * libirecovery.c
 * Communication to iBoot/iBSS on Apple iOS devices via USB
 *
 * Copyright (c) 2011-2020 Nikias Bassen <nikias@gmx.li>
 * Copyright (c) 2012-2020 Martin Szulecki <martin.szulecki@libimobiledevice.org>
 * Copyright (c) 2010 Chronic-Dev Team
 * Copyright (c) 2010 Joshua Hill
 * Copyright (c) 2008-2011 Nicolas Haunold
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

/* original: https://github.com/libimobiledevice/libirecovery */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>

#include <ircv.h>

#define IRECV_API __attribute__((visibility("default")))

#define debug(...) if(libirecovery_debug) fprintf(stderr, __VA_ARGS__)

static int libirecovery_debug = 0;

struct irecv_client_private {
    int debug;
    int usb_config;
    int usb_interface;
    int usb_alt_interface;
    unsigned int mode;
    struct irecv_device_info device_info;
    IOUSBDeviceInterface320 **handle;
    IOUSBInterfaceInterface300 **usbInterface;
    CFRunLoopSourceRef async_event_source;
    irecv_event_cb_t progress_callback;
    irecv_event_cb_t received_callback;
    irecv_event_cb_t connected_callback;
    irecv_event_cb_t precommand_callback;
    irecv_event_cb_t postcommand_callback;
    irecv_event_cb_t disconnected_callback;
};



static struct irecv_device irecv_devices[] = {
    /* iPhone */
    { "iPhone1,1",   "m68ap",    0x00, 0x8900, "iPhone 2G" },
    { "iPhone1,2",   "n82ap",    0x04, 0x8900, "iPhone 3G" },
    { "iPhone2,1",   "n88ap",    0x00, 0x8920, "iPhone 3Gs" },
    { "iPhone3,1",   "n90ap",    0x00, 0x8930, "iPhone 4 (GSM)" },
    { "iPhone3,2",   "n90bap",   0x04, 0x8930, "iPhone 4 (GSM) R2 2012" },
    { "iPhone3,3",   "n92ap",    0x06, 0x8930, "iPhone 4 (CDMA)" },
    { "iPhone4,1",   "n94ap",    0x08, 0x8940, "iPhone 4s" },
    { "iPhone5,1",   "n41ap",    0x00, 0x8950, "iPhone 5 (GSM)" },
    { "iPhone5,2",   "n42ap",    0x02, 0x8950, "iPhone 5 (Global)" },
    { "iPhone5,3",   "n48ap",    0x0a, 0x8950, "iPhone 5c (GSM)" },
    { "iPhone5,4",   "n49ap",    0x0e, 0x8950, "iPhone 5c (Global)" },
    { "iPhone6,1",   "n51ap",    0x00, 0x8960, "iPhone 5s (GSM)" },
    { "iPhone6,2",   "n53ap",    0x02, 0x8960, "iPhone 5s (Global)" },
    { "iPhone7,1",   "n56ap",    0x04, 0x7000, "iPhone 6 Plus" },
    { "iPhone7,2",   "n61ap",    0x06, 0x7000, "iPhone 6" },
    { "iPhone8,1",   "n71ap",    0x04, 0x8000, "iPhone 6s" },
    { "iPhone8,1",   "n71map",   0x04, 0x8003, "iPhone 6s" },
    { "iPhone8,2",   "n66ap",    0x06, 0x8000, "iPhone 6s Plus" },
    { "iPhone8,2",   "n66map",   0x06, 0x8003, "iPhone 6s Plus" },
    { "iPhone8,4",   "n69ap",    0x02, 0x8003, "iPhone SE" },
    { "iPhone8,4",   "n69uap",   0x02, 0x8000, "iPhone SE" },
    { "iPhone9,1",   "d10ap",    0x08, 0x8010, "iPhone 7 (Global)" },
    { "iPhone9,2",   "d11ap",    0x0a, 0x8010, "iPhone 7 Plus (Global)" },
    { "iPhone9,3",   "d101ap",   0x0c, 0x8010, "iPhone 7 (GSM)" },
    { "iPhone9,4",   "d111ap",   0x0e, 0x8010, "iPhone 7 Plus (GSM)" },
    { "iPhone10,1",  "d20ap",    0x02, 0x8015, "iPhone 8 (Global)" },
    { "iPhone10,2",  "d21ap",    0x04, 0x8015, "iPhone 8 Plus (Global)" },
    { "iPhone10,3",  "d22ap",    0x06, 0x8015, "iPhone X (Global)" },
    { "iPhone10,4",  "d201ap",   0x0a, 0x8015, "iPhone 8 (GSM)" },
    { "iPhone10,5",  "d211ap",   0x0c, 0x8015, "iPhone 8 Plus (GSM)" },
    { "iPhone10,6",  "d221ap",   0x0e, 0x8015, "iPhone X (GSM)" },
    { "iPhone11,2",  "d321ap",   0x0e, 0x8020, "iPhone XS" },
    { "iPhone11,4",  "d331ap",   0x0a, 0x8020, "iPhone XS Max (China)" },
    { "iPhone11,6",  "d331pap",  0x1a, 0x8020, "iPhone XS Max" },
    { "iPhone11,8",  "n841ap",   0x0c, 0x8020, "iPhone XR" },
    { "iPhone12,1",  "n104ap",   0x04, 0x8030, "iPhone 11" },
    { "iPhone12,3",  "d421ap",   0x06, 0x8030, "iPhone 11 Pro" },
    { "iPhone12,5",  "d431ap",   0x02, 0x8030, "iPhone 11 Pro Max" },
    /* iPod */
    { "iPod1,1",     "n45ap",    0x02, 0x8900, "iPod Touch (1st gen)" },
    { "iPod2,1",     "n72ap",    0x00, 0x8720, "iPod Touch (2nd gen)" },
    { "iPod3,1",     "n18ap",    0x02, 0x8922, "iPod Touch (3rd gen)" },
    { "iPod4,1",     "n81ap",    0x08, 0x8930, "iPod Touch (4th gen)" },
    { "iPod5,1",     "n78ap",    0x00, 0x8942, "iPod Touch (5th gen)" },
    { "iPod7,1",     "n102ap",   0x10, 0x7000, "iPod Touch (6th gen)" },
    { "iPod9,1",     "n112ap",   0x16, 0x8010, "iPod Touch (7th gen)" },
    /* iPad */
    { "iPad1,1",     "k48ap",    0x02, 0x8930, "iPad" },
    { "iPad2,1",     "k93ap",    0x04, 0x8940, "iPad 2 (WiFi)" },
    { "iPad2,2",     "k94ap",    0x06, 0x8940, "iPad 2 (GSM)" },
    { "iPad2,3",     "k95ap",    0x02, 0x8940, "iPad 2 (CDMA)" },
    { "iPad2,4",     "k93aap",   0x06, 0x8942, "iPad 2 (WiFi) R2 2012" },
    { "iPad2,5",     "p105ap",   0x0a, 0x8942, "iPad Mini (WiFi)" },
    { "iPad2,6",     "p106ap",   0x0c, 0x8942, "iPad Mini (GSM)" },
    { "iPad2,7",     "p107ap",   0x0e, 0x8942, "iPad Mini (Global)" },
    { "iPad3,1",     "j1ap",     0x00, 0x8945, "iPad 3 (WiFi)" },
    { "iPad3,2",     "j2ap",     0x02, 0x8945, "iPad 3 (CDMA)" },
    { "iPad3,3",     "j2aap",    0x04, 0x8945, "iPad 3 (GSM)" },
    { "iPad3,4",     "p101ap",   0x00, 0x8955, "iPad 4 (WiFi)" },
    { "iPad3,5",     "p102ap",   0x02, 0x8955, "iPad 4 (GSM)" },
    { "iPad3,6",     "p103ap",   0x04, 0x8955, "iPad 4 (Global)" },
    { "iPad4,1",     "j71ap",    0x10, 0x8960, "iPad Air (WiFi)" },
    { "iPad4,2",     "j72ap",    0x12, 0x8960, "iPad Air (Cellular)" },
    { "iPad4,3",     "j73ap",    0x14, 0x8960, "iPad Air (China)" },
    { "iPad4,4",     "j85ap",    0x0a, 0x8960, "iPad Mini 2 (WiFi)" },
    { "iPad4,5",     "j86ap",    0x0c, 0x8960, "iPad Mini 2 (Cellular)" },
    { "iPad4,6",     "j87ap",    0x0e, 0x8960, "iPad Mini 2 (China)" },
    { "iPad4,7",     "j85map",   0x32, 0x8960, "iPad Mini 3 (WiFi)" },
    { "iPad4,8",     "j86map",   0x34, 0x8960, "iPad Mini 3 (Cellular)" },
    { "iPad4,9",     "j87map",   0x36, 0x8960, "iPad Mini 3 (China)" },
    { "iPad5,1",     "j96ap",    0x08, 0x7000, "iPad Mini 4 (WiFi)" },
    { "iPad5,2",     "j97ap",    0x0A, 0x7000, "iPad Mini 4 (Cellular)" },
    { "iPad5,3",     "j81ap",    0x06, 0x7001, "iPad Air 2 (WiFi)" },
    { "iPad5,4",     "j82ap",    0x02, 0x7001, "iPad Air 2 (Cellular)" },
    { "iPad6,3",     "j127ap",   0x08, 0x8001, "iPad Pro 9.7in (WiFi)" },
    { "iPad6,4",     "j128ap",   0x0a, 0x8001, "iPad Pro 9.7in (Cellular)" },
    { "iPad6,7",     "j98aap",   0x10, 0x8001, "iPad Pro 12.9in (WiFi)" },
    { "iPad6,8",     "j99aap",   0x12, 0x8001, "iPad Pro 12.9in (Cellular)" },
    { "iPad6,11",    "j71sap",   0x10, 0x8000, "iPad 5 (WiFi)" },
    { "iPad6,11",    "j71tap",   0x10, 0x8003, "iPad 5 (WiFi)" },
    { "iPad6,12",    "j72sap",   0x12, 0x8000, "iPad 5 (Cellular)" },
    { "iPad6,12",    "j72tap",   0x12, 0x8003, "iPad 5 (Cellular)" },
    { "iPad7,1",     "j120ap",   0x0C, 0x8011, "iPad Pro 2 12.9in (WiFi)" },
    { "iPad7,2",     "j121ap",   0x0E, 0x8011, "iPad Pro 2 12.9in (Cellular)" },
    { "iPad7,3",     "j207ap",   0x04, 0x8011, "iPad Pro 10.5in (WiFi)" },
    { "iPad7,4",     "j208ap",   0x06, 0x8011, "iPad Pro 10.5in (Cellular)" },
    { "iPad7,5",     "j71bap",   0x18, 0x8010, "iPad 6 (WiFi)" },
    { "iPad7,6",     "j72bap",   0x1A, 0x8010, "iPad 6 (Cellular)" },
    { "iPad7,11",    "j172ap",   0x1E, 0x8010, "iPad 7 (WiFi)" },
    { "iPad7,12",    "j171ap",   0x1C, 0x8010, "iPad 7 (Cellular)" },
    { "iPad8,1",     "j317ap",   0x0C, 0x8027, "iPad Pro 3 11in (WiFi)" },
    { "iPad8,2",     "j317xap",  0x1C, 0x8027, "iPad Pro 3 11in (WiFi, 1TB)" },
    { "iPad8,3",     "j318ap",   0x0E, 0x8027, "iPad Pro 3 11in (Cellular)" },
    { "iPad8,4",     "j318xap",  0x1E, 0x8027, "iPad Pro 3 11in (Cellular, 1TB)" },
    { "iPad8,5",     "j320ap",   0x08, 0x8027, "iPad Pro 3 12.9in (WiFi)" },
    { "iPad8,6",     "j320xap",  0x18, 0x8027, "iPad Pro 3 12.9in (WiFi, 1TB)" },
    { "iPad8,7",     "j321ap",   0x0A, 0x8027, "iPad Pro 3 12.9in (Cellular)" },
    { "iPad8,8",     "j321xap",  0x1A, 0x8027, "iPad Pro 3 12.9in (Cellular, 1TB)" },
    { "iPad8,9",     "j417ap",   0x3C, 0x8027, "iPad Pro 4 11in (WiFi)" },
    { "iPad8,10",    "j418ap",   0x3E, 0x8027, "iPad Pro 4 11in (Cellular)" },
    { "iPad8,11",    "j420ap",   0x38, 0x8027, "iPad Pro 4 12.9in (WiFi)" },
    { "iPad8,12",    "j421ap",   0x3A, 0x8027, "iPad Pro 4 12.9in (Cellular)" },
    { "iPad11,1",    "j210ap",   0x14, 0x8020, "iPad Mini 5 (WiFi)" },
    { "iPad11,2",    "j211ap",   0x16, 0x8020, "iPad Mini 5 (Cellular)" },
    { "iPad11,3",    "j217ap",   0x1C, 0x8020, "iPad Air 3 (WiFi)" },
    { "iPad11,4",    "j218ap",   0x1E, 0x8020, "iPad Air 3 (Celluar)" },
    /* Apple TV */
    { "AppleTV2,1",  "k66ap",    0x10, 0x8930, "Apple TV 2" },
    { "AppleTV3,1",  "j33ap",    0x08, 0x8942, "Apple TV 3" },
    { "AppleTV3,2",  "j33iap",   0x00, 0x8947, "Apple TV 3 (2013)" },
    { "AppleTV5,3",  "j42dap",   0x34, 0x7000, "Apple TV 4" },
    { "AppleTV6,2",  "j105aap",  0x02, 0x8011, "Apple TV 4K" },
    /* Apple T2 Coprocessor */
    { "iBridge2,1",     "j137ap",   0x0A, 0x8012, "Apple T2 iMacPro1,1 (j137)" },
    { "iBridge2,3",     "j680ap",   0x0B, 0x8012, "Apple T2 MacBookPro15,1 (j680)" },
    { "iBridge2,4",     "j132ap",   0x0C, 0x8012, "Apple T2 MacBookPro15,2 (j132)" },
    { "iBridge2,5",     "j174ap",   0x0E, 0x8012, "Apple T2 Macmini8,1 (j174)" },
    { "iBridge2,6",     "j160ap",   0x0F, 0x8012, "Apple T2 MacPro7,1 (j160)" },
    { "iBridge2,7",     "j780ap",   0x07, 0x8012, "Apple T2 MacBookPro15,3 (j780)" },
    { "iBridge2,8",     "j140kap",  0x17, 0x8012, "Apple T2 MacBookAir8,1 (j140k)" },
    { "iBridge2,10", "j213ap",   0x18, 0x8012, "Apple T2 MacBookPro15,4 (j213)" },
    { "iBridge2,12", "j140aap",  0x37, 0x8012, "Apple T2 MacBookAir8,2 (j140a)" },
    { "iBridge2,14", "j152f",    0x3A, 0x8012, "Apple T2 MacBookPro16,1 (j152f)" },
    { NULL,          NULL,      -1,   -1,     NULL }
};

static unsigned int crc32_lookup_t1[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
};

#define crc32_step(a,b) \
a = (crc32_lookup_t1[(a & 0xFF) ^ ((unsigned char)b)] ^ (a >> 8))

typedef struct {
    uint32_t len;
    kern_return_t ret;
} async_transfer_t;

static int nsleep(long nanoseconds) {
    struct timespec req, rem;
    req.tv_sec = 0;
    req.tv_nsec = nanoseconds;
    return nanosleep(&req, &rem);
}


IRECV_API void irecv_set_debug_level(int level) {
    libirecovery_debug = level;
}

static int check_context(irecv_client_t client) {
    if (client == NULL || client->handle == NULL) {
        return IRECV_E_NO_DEVICE;
    }
    
    return IRECV_E_SUCCESS;
}


IRECV_API const struct irecv_device_info* irecv_get_device_info(irecv_client_t client)
{
    if (check_context(client) != IRECV_E_SUCCESS)
        return NULL;
    
    return &client->device_info;
}

IRECV_API irecv_error_t irecv_get_mode(irecv_client_t client, int* mode) {

    if (check_context(client) != IRECV_E_SUCCESS)
        return IRECV_E_NO_DEVICE;
    
    *mode = client->mode;
    
    return IRECV_E_SUCCESS;
}

static void iokit_cfdictionary_set_short(CFMutableDictionaryRef dict, const void *key, SInt16 value)
{
    CFNumberRef numberRef;
    
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &value);
    if (numberRef) {
        CFDictionarySetValue(dict, key, numberRef);
        CFRelease(numberRef);
    }
}

static int iokit_get_string_descriptor_ascii(irecv_client_t client, uint8_t desc_index, unsigned char * buffer, int size) {
    
    IOReturn result;
    IOUSBDevRequest request;
    unsigned char descriptor[256];
    
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = (kUSBStringDesc << 8); // | desc_index;
    request.wIndex = 0; // All languages 0x409; // language
    request.wLength = sizeof(descriptor) - 1;
    request.pData = descriptor;
    request.wLenDone = 0;
    
    result = (*client->handle)->DeviceRequest(client->handle, &request);
    if (result == kIOReturnNoDevice)
        return IRECV_E_NO_DEVICE;
    if (result == kIOReturnNotOpen)
        return IRECV_E_USB_STATUS;
    if (result != kIOReturnSuccess)
        return IRECV_E_UNKNOWN_ERROR;
    
    if (descriptor[0] >= 4) { // && descriptor[2] == 0x9 && descriptor[3] == 0x4) {
        
        request.wValue = (kUSBStringDesc << 8) | desc_index;
        request.wIndex = descriptor[2] + (descriptor[3] << 8);
        request.wLenDone = 0;
        result = (*client->handle)->DeviceRequest(client->handle, &request);
        
        if (result == kIOReturnNoDevice)
            return IRECV_E_NO_DEVICE;
        if (result == kIOReturnNotOpen)
            return IRECV_E_USB_STATUS;
        if (result != kIOReturnSuccess)
            return IRECV_E_UNKNOWN_ERROR;
        
        int i = 2, j = 0;
        for ( ; i < descriptor[0]; i += 2, j += 1) {
            buffer[j] = descriptor[i];
        }
        buffer[j] = 0;
        
        return request.wLenDone;
    }
    return IRECV_E_UNKNOWN_ERROR;
}

static int irecv_get_string_descriptor_ascii(irecv_client_t client, uint8_t desc_index, unsigned char * buffer, int size) {
    return iokit_get_string_descriptor_ascii(client, desc_index, buffer, size);
}

static void irecv_load_device_info_from_iboot_string(irecv_client_t client, const char* iboot_string)
{
    if (!client || !iboot_string) {
        return;
    }
    
    memset(&client->device_info, '\0', sizeof(struct irecv_device_info));
    
    client->device_info.serial_string = strdup(iboot_string);
    
    char* ptr;
    
    ptr = strstr(iboot_string, "CPID:");
    if (ptr != NULL) {
        sscanf(ptr, "CPID:%x", &client->device_info.cpid);
    }
    
    ptr = strstr(iboot_string, "CPRV:");
    if (ptr != NULL) {
        sscanf(ptr, "CPRV:%x", &client->device_info.cprv);
    }
    
    ptr = strstr(iboot_string, "CPFM:");
    if (ptr != NULL) {
        sscanf(ptr, "CPFM:%x", &client->device_info.cpfm);
    }
    
    ptr = strstr(iboot_string, "SCEP:");
    if (ptr != NULL) {
        sscanf(ptr, "SCEP:%x", &client->device_info.scep);
    }
    
    ptr = strstr(iboot_string, "BDID:");
    if (ptr != NULL) {
        sscanf(ptr, "BDID:%x", &client->device_info.bdid);
    }
    
    ptr = strstr(iboot_string, "ECID:");
    if (ptr != NULL) {
        sscanf(ptr, "ECID:%" SCNx64, &client->device_info.ecid);
    }
    
    ptr = strstr(iboot_string, "IBFL:");
    if (ptr != NULL) {
        sscanf(ptr, "IBFL:%x", &client->device_info.ibfl);
    }
    
    char tmp[256];
    tmp[0] = '\0';
    ptr = strstr(iboot_string, "SRNM:[");
    if(ptr != NULL) {
        sscanf(ptr, "SRNM:[%s]", tmp);
        ptr = strrchr(tmp, ']');
        if(ptr != NULL) {
            *ptr = '\0';
        }
        client->device_info.srnm = strdup(tmp);
    }
    
    tmp[0] = '\0';
    ptr = strstr(iboot_string, "IMEI:[");
    if(ptr != NULL) {
        sscanf(ptr, "IMEI:[%s]", tmp);
        ptr = strrchr(tmp, ']');
        if(ptr != NULL) {
            *ptr = '\0';
        }
        client->device_info.imei = strdup(tmp);
    }
    
    tmp[0] = '\0';
    ptr = strstr(iboot_string, "SRTG:[");
    if(ptr != NULL) {
        sscanf(ptr, "SRTG:[%s]", tmp);
        ptr = strrchr(tmp, ']');
        if(ptr != NULL) {
            *ptr = '\0';
        }
        client->device_info.srtg = strdup(tmp);
    }
}

static void irecv_copy_nonce_with_tag(irecv_client_t client, const char* tag, unsigned char** nonce, unsigned int* nonce_size)
{
    if (!client || !tag) {
        return;
    }
    
    char buf[255];
    int len;
    
    *nonce = NULL;
    *nonce_size = 0;
    
    len = irecv_get_string_descriptor_ascii(client, 1, (unsigned char*) buf, 255);
    if (len < 0) {
        debug("%s: got length: %d\n", __func__, len);
        return;
    }
    
    buf[len] = 0;
    
    int taglen = strlen(tag);
    int nlen = 0;
    char* nonce_string = NULL;
    char* p = buf;
    char* colon = NULL;
    do {
        colon = strchr(p, ':');
        if (!colon)
            break;
        if (colon-taglen < p) {
            break;
        }
        char *space = strchr(colon, ' ');
        if (strncmp(colon-taglen, tag, taglen) == 0) {
            p = colon+1;
            if (!space) {
                nlen = strlen(p);
            } else {
                nlen = space-p;
            }
            nonce_string = p;
            nlen/=2;
            break;
        } else {
            if (!space) {
                break;
            } else {
                p = space+1;
            }
        }
    } while (colon);
    
    if (nlen == 0) {
        debug("%s: WARNING: couldn't find tag %s in string %s\n", __func__, tag, buf);
        return;
    }
    
    unsigned char *nn = malloc(nlen);
    if (!nn) {
        return;
    }
    
    int i = 0;
    for (i = 0; i < nlen; i++) {
        int val = 0;
        if (sscanf(nonce_string+(i*2), "%02X", &val) == 1) {
            nn[i] = (unsigned char)val;
        } else {
            debug("%s: ERROR: unexpected data in nonce result (%2s)\n", __func__, nonce_string+(i*2));
            break;
        }
    }
    
    if (i != nlen) {
        debug("%s: ERROR: unable to parse nonce\n", __func__);
        free(nn);
        return;
    }
    
    *nonce = nn;
    *nonce_size = nlen;
}


static irecv_error_t iokit_usb_open_service(irecv_client_t *pclient, io_service_t service) {
    
    IOReturn result;
    irecv_error_t error;
    irecv_client_t client;
    SInt32 score;
    UInt16 mode;
    UInt32 locationID;
    IOCFPlugInInterface **plug = NULL;
    CFStringRef serialString;
    
    client = (irecv_client_t) calloc( 1, sizeof(struct irecv_client_private));
    
    // Create the plug-in
    const int max_retries = 5;
    
    for (int count = 0; count < max_retries; count++) {
        result = IOCreatePlugInInterfaceForService(service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score);
        if (kIOReturnSuccess == result && plug) {
              break;
            }
        nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 1000}, NULL);
    }
        
    if (result != kIOReturnSuccess) {
        IOObjectRelease(service);
        free(client);
        return IRECV_E_UNKNOWN_ERROR;
    }
    
    // Cache the serial string before discarding the service. The service object
    // has a cached copy, so a request to the hardware device is not required.
    char serial_str[256];
    serial_str[0] = '\0';
    serialString = IORegistryEntryCreateCFProperty(service, CFSTR(kUSBSerialNumberString), kCFAllocatorDefault, 0);
    if (serialString) {
        CFStringGetCString(serialString, serial_str, sizeof(serial_str), kCFStringEncodingUTF8);
        CFRelease(serialString);
    }
    
    irecv_load_device_info_from_iboot_string(client, serial_str);
    
    IOObjectRelease(service);
    
    // Create the device interface
    result = (*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID320), (LPVOID *)&(client->handle));
    IODestroyPlugInInterface(plug);
    if (result != kIOReturnSuccess) {
        free(client);
        return IRECV_E_UNKNOWN_ERROR;
    }
    
    (*client->handle)->GetDeviceProduct(client->handle, &mode);
    (*client->handle)->GetLocationID(client->handle, &locationID);
    client->mode = mode;
    debug("opening device %04x:%04x @ %#010x...\n", kAppleVendorID, client->mode, locationID);
    
    result = (*client->handle)->USBDeviceOpenSeize(client->handle);
    if (result != kIOReturnSuccess) {
        (*client->handle)->Release(client->handle);
        free(client);
        return IRECV_E_UNABLE_TO_CONNECT;
    }
    
    irecv_copy_nonce_with_tag(client, "NONC", &client->device_info.ap_nonce, &client->device_info.ap_nonce_size);
    irecv_copy_nonce_with_tag(client, "SNON", &client->device_info.sep_nonce, &client->device_info.sep_nonce_size);
    
    error = irecv_usb_set_configuration(client, 1);
    if (error != IRECV_E_SUCCESS) {
        free(client);
        return error;
    }
    
    error = (*client->handle)->CreateDeviceAsyncEventSource(client->handle, &client->async_event_source);
    if (error != IRECV_E_SUCCESS) {
        free(client);
        return error;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), client->async_event_source, kCFRunLoopDefaultMode);
    
    // DFU mode has no endpoints, so no need to open the interface
    if (client->mode == IRECV_K_DFU_MODE || client->mode == IRECV_K_WTF_MODE) {
        
        error = irecv_usb_set_interface(client, 0, 0);
        if (error != IRECV_E_SUCCESS) {
            free(client);
            return error;
        }
    }
    else {
        error = irecv_usb_set_interface(client, 0, 0);
        if (error != IRECV_E_SUCCESS) {
            free(client);
            return error;
        }
        if (client->mode > IRECV_K_RECOVERY_MODE_2) {
            error = irecv_usb_set_interface(client, 1, 1);
            if (error != IRECV_E_SUCCESS) {
                free(client);
                return error;
            }
        }
    }
    
    *pclient = client;
    return IRECV_E_SUCCESS;
}


static const char *darwin_device_class = kIOUSBDeviceClassName;

static io_iterator_t iokit_usb_get_iterator_for_pid(UInt16 pid) {
    
    IOReturn result;
    io_iterator_t iterator;
    CFMutableDictionaryRef matchingDict;
    
#ifdef IPHONEOS_ARM
    darwin_device_class = "IOUSBHostDevice";
#endif
    matchingDict = IOServiceMatching(darwin_device_class);
    iokit_cfdictionary_set_short(matchingDict, CFSTR(kUSBVendorID), kAppleVendorID);
    iokit_cfdictionary_set_short(matchingDict, CFSTR(kUSBProductID), pid);
    
    result = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iterator);
    if (result != kIOReturnSuccess){
        return IO_OBJECT_NULL;
    }
    
    return iterator;
}


IRECV_API irecv_error_t irecv_close(irecv_client_t client) {
    
    if (client != NULL) {
        if(client->disconnected_callback != NULL) {
            irecv_event_t event;
            event.size = 0;
            event.data = NULL;
            event.progress = 0;
            event.type = IRECV_DISCONNECTED;
            client->disconnected_callback(client, &event);
        }

        if (client->usbInterface) {
            (*client->usbInterface)->USBInterfaceClose(client->usbInterface);
            (*client->usbInterface)->Release(client->usbInterface);
            client->usbInterface = NULL;
        }
        if (client->handle) {
            (*client->handle)->USBDeviceClose(client->handle);
            (*client->handle)->Release(client->handle);
            client->handle = NULL;
        }
        if(client->async_event_source) {
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), client->async_event_source, kCFRunLoopDefaultMode);
            CFRelease(client->async_event_source);
        }

        free(client->device_info.srnm);
        free(client->device_info.imei);
        free(client->device_info.srtg);
        free(client->device_info.serial_string);
        free(client->device_info.ap_nonce);
        free(client->device_info.sep_nonce);
        
        free(client);
        client = NULL;
    }
    
    return IRECV_E_SUCCESS;
}

IRECV_API irecv_error_t irecv_usb_set_configuration(irecv_client_t client, int configuration) {

    if (check_context(client) != IRECV_E_SUCCESS)
        return IRECV_E_NO_DEVICE;
    
    debug("Setting to configuration %d\n", configuration);
    
    IOReturn result;
    result = (*client->handle)->SetConfiguration(client->handle, configuration);
    if (result != kIOReturnSuccess) {
        debug("error setting configuration: %#x\n", result);
        return IRECV_E_USB_CONFIGURATION;
    }
 
    return IRECV_E_SUCCESS;
}


static irecv_error_t iokit_open_with_ecid(irecv_client_t* pclient, unsigned long long ecid) {
    
    io_service_t service, ret_service;
    io_iterator_t iterator;
    CFStringRef usbSerial = NULL;
    CFStringRef ecidString = NULL;
    CFRange range;
    
    UInt16 wtf_pids[] = { IRECV_K_WTF_MODE, 0};
    UInt16 all_pids[] = { IRECV_K_WTF_MODE, IRECV_K_DFU_MODE, IRECV_K_RECOVERY_MODE_1, IRECV_K_RECOVERY_MODE_2, IRECV_K_RECOVERY_MODE_3, IRECV_K_RECOVERY_MODE_4, 0 };
    UInt16 *pids = all_pids;
    int i;
    
    if (pclient == NULL) {
        debug("%s: pclient parameter is null\n", __func__);
        return IRECV_E_INVALID_INPUT;
    }
    if (ecid == IRECV_K_WTF_MODE) {
        /* special ecid case, ignore !IRECV_K_WTF_MODE */
        pids = wtf_pids;
        ecid = 0;
    }
    
    if (ecid > 0) {
        ecidString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%llX"), ecid);
        if (ecidString == NULL) {
            debug("%s: failed to create ECID string\n", __func__);
            return IRECV_E_UNABLE_TO_CONNECT;
        }
    }
    
    *pclient = NULL;
    ret_service = IO_OBJECT_NULL;
    
    for (i = 0; (pids[i] > 0 && ret_service == IO_OBJECT_NULL) ; i++) {
        
        iterator = iokit_usb_get_iterator_for_pid(pids[i]);
        
        if (iterator) {
            while ((service = IOIteratorNext(iterator))) {
                
                if (ecid == 0) {
                    ret_service = service;
                    break;
                }
                
                usbSerial = IORegistryEntryCreateCFProperty(service, CFSTR(kUSBSerialNumberString), kCFAllocatorDefault, 0);
                
                if (usbSerial == NULL) {
                    debug("%s: failed to create USB serial string property\n", __func__);
                    IOObjectRelease(service);
                    continue;
                }
                
                range = CFStringFind(usbSerial, ecidString, kCFCompareCaseInsensitive);
                if (range.location == kCFNotFound) {
                    IOObjectRelease(service);
                } else {
                    ret_service = service;
                    break;
                }
            }
            if (usbSerial) {
                CFRelease(usbSerial);
                usbSerial = NULL;
            }
            IOObjectRelease(iterator);
        }
    }
    
    if (ecidString)
        CFRelease(ecidString);
    
    if (ret_service == IO_OBJECT_NULL)
        return IRECV_E_UNABLE_TO_CONNECT;
    
    return iokit_usb_open_service(pclient, ret_service);
}

static IOReturn iokit_usb_get_interface(IOUSBDeviceInterface320 **device, uint8_t ifc, io_service_t *usbInterfacep) {
    
    IOUSBFindInterfaceRequest request;
    uint8_t                   current_interface;
    kern_return_t             kresult;
    io_iterator_t             interface_iterator;
    
    *usbInterfacep = IO_OBJECT_NULL;
    
    request.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting  = kIOUSBFindInterfaceDontCare;
    
    kresult = (*device)->CreateInterfaceIterator(device, &request, &interface_iterator);
    if (kresult)
        return kresult;
    
    for ( current_interface = 0 ; current_interface <= ifc ; current_interface++ ) {
        *usbInterfacep = IOIteratorNext(interface_iterator);
        if (current_interface != ifc)
            (void) IOObjectRelease (*usbInterfacep);
    }
    IOObjectRelease(interface_iterator);
    
    return kIOReturnSuccess;
}


static irecv_error_t iokit_usb_set_interface(irecv_client_t client, int usb_interface, int usb_alt_interface) {
    IOReturn result;
    io_service_t interface_service = IO_OBJECT_NULL;
    IOCFPlugInInterface **plugInInterface = NULL;
    SInt32 score;
    
    // Close current interface
    if (client->usbInterface) {
        result = (*client->usbInterface)->USBInterfaceClose(client->usbInterface);
        result = (*client->usbInterface)->Release(client->usbInterface);
        client->usbInterface = NULL;
    }
    
    result = iokit_usb_get_interface(client->handle, usb_interface, &interface_service);
    if (result != kIOReturnSuccess) {
        debug("failed to find requested interface: %d\n", usb_interface);
        return IRECV_E_USB_INTERFACE;
    }
    
    result = IOCreatePlugInInterfaceForService(interface_service, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
    IOObjectRelease(interface_service);
    if (result != kIOReturnSuccess) {
        debug("error creating plug-in interface: %#x\n", result);
        return IRECV_E_USB_INTERFACE;
    }
    
    result = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID300), (LPVOID)&client->usbInterface);
    IODestroyPlugInInterface(plugInInterface);
    if (result != kIOReturnSuccess) {
        debug("error creating interface interface: %#x\n", result);
        return IRECV_E_USB_INTERFACE;
    }
    
    result = (*client->usbInterface)->USBInterfaceOpen(client->usbInterface);
    if (result != kIOReturnSuccess) {
        debug("error opening interface: %#x\n", result);
        return IRECV_E_USB_INTERFACE;
    }
    
    if (usb_interface == 1) {
        result = (*client->usbInterface)->SetAlternateInterface(client->usbInterface, usb_alt_interface);
        if (result != kIOReturnSuccess) {
            debug("error setting alternate interface: %#x\n", result);
            return IRECV_E_USB_INTERFACE;
        }
    }
    
    return IRECV_E_SUCCESS;
}

IRECV_API irecv_error_t irecv_usb_set_interface(irecv_client_t client, int usb_interface, int usb_alt_interface) {

    if (check_context(client) != IRECV_E_SUCCESS)
        return IRECV_E_NO_DEVICE;
    
    debug("Setting to interface %d:%d\n", usb_interface, usb_alt_interface);
    if (iokit_usb_set_interface(client, usb_interface, usb_alt_interface) < 0) {
        return IRECV_E_USB_INTERFACE;
    }

    client->usb_interface = usb_interface;
    client->usb_alt_interface = usb_alt_interface;
    
    return IRECV_E_SUCCESS;
}


IRECV_API irecv_error_t irecv_open_with_ecid(irecv_client_t* pclient, unsigned long long ecid) {

    int ret = IRECV_E_UNABLE_TO_CONNECT;
    if(libirecovery_debug) {
        irecv_set_debug_level(libirecovery_debug);
    }
    ret = iokit_open_with_ecid(pclient, ecid);
    if (ret == IRECV_E_SUCCESS) {
        if ((*pclient)->connected_callback != NULL) {
            irecv_event_t event;
            event.size = 0;
            event.data = NULL;
            event.progress = 0;
            event.type = IRECV_CONNECTED;
            (*pclient)->connected_callback(*pclient, &event);
        }
    }
    return ret;
}

IRECV_API irecv_error_t irecv_devices_get_device_by_client(irecv_client_t client, irecv_device_t* device) {

    int i = 0;
    
    *device = NULL;
    
    if (client->device_info.cpid == 0) {
        return IRECV_E_UNKNOWN_ERROR;
    }
    
    for (i = 0; irecv_devices[i].hardware_model != NULL; i++) {
        if (irecv_devices[i].chip_id == client->device_info.cpid && irecv_devices[i].board_id == client->device_info.bdid) {
            *device = &irecv_devices[i];
            return IRECV_E_SUCCESS;
        }
    }
    
    return IRECV_E_NO_DEVICE;
}

static void iokit_async_cb(void *refcon, kern_return_t ret, void *arg_0)
{
    async_transfer_t* transfer = refcon;
    
    if(transfer != NULL) {
        transfer->ret = ret;
        memcpy(&transfer->len, &arg_0, sizeof(transfer->len));
        CFRunLoopStop(CFRunLoopGetCurrent());
    }
}

static int iokit_usb_control_transfer(irecv_client_t client, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value, uint16_t w_index, unsigned char *data, uint16_t w_length, unsigned int timeout)
{
    IOReturn result;
    IOUSBDevRequestTO req;
    
    bzero(&req, sizeof(req));
    req.bmRequestType     = bm_request_type;
    req.bRequest          = b_request;
    req.wValue            = OSSwapLittleToHostInt16(w_value);
    req.wIndex            = OSSwapLittleToHostInt16(w_index);
    req.wLength           = OSSwapLittleToHostInt16(w_length);
    req.pData             = data;
    req.noDataTimeout     = timeout;
    req.completionTimeout = timeout;
    
    result = (*client->handle)->DeviceRequestTO(client->handle, &req);
    switch (result) {
        case kIOReturnSuccess:         return req.wLenDone;
        case kIOUSBPipeStalled:        return IRECV_E_PIPE;
#ifdef IPHONEOS_ARM
        case kUSBHostReturnPipeStalled:return IRECV_E_PIPE;
#endif
        case kIOReturnTimeout:         return IRECV_E_TIMEOUT;
        case kIOUSBTransactionTimeout: return IRECV_E_TIMEOUT;
        case kIOReturnNotResponding:   return IRECV_E_NO_DEVICE;
        case kIOReturnNoDevice:           return IRECV_E_NO_DEVICE;
        default:
            return IRECV_E_UNKNOWN_ERROR;
    }
}

static int iokit_async_usb_control_transfer(irecv_client_t client, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value, uint16_t w_index, unsigned char *data, uint16_t w_length, async_transfer_t* transfer)
{
    IOReturn result;
    IOUSBDevRequest req;
    
    bzero(&req, sizeof(req));
    req.bmRequestType     = bm_request_type;
    req.bRequest          = b_request;
    req.wValue            = OSSwapLittleToHostInt16(w_value);
    req.wIndex            = OSSwapLittleToHostInt16(w_index);
    req.wLength           = OSSwapLittleToHostInt16(w_length);
    req.pData             = data;
    result = (*client->handle)->DeviceRequestAsync(client->handle, &req, iokit_async_cb, transfer);
    switch (result) {
        case kIOReturnSuccess:         return IRECV_E_SUCCESS;
        case kIOUSBPipeStalled:        return IRECV_E_PIPE;
#ifdef IPHONEOS_ARM
        case kUSBHostReturnPipeStalled:return IRECV_E_PIPE;
#endif
        case kIOReturnTimeout:         return IRECV_E_TIMEOUT;
        case kIOUSBTransactionTimeout: return IRECV_E_TIMEOUT;
        case kIOReturnNotResponding:   return IRECV_E_NO_DEVICE;
        case kIOReturnNoDevice:           return IRECV_E_NO_DEVICE;
        default:
            return IRECV_E_UNKNOWN_ERROR;
    }
}


IRECV_API int irecv_usb_control_transfer(irecv_client_t client, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value, uint16_t w_index, unsigned char *data, uint16_t w_length, unsigned int timeout) {

    return iokit_usb_control_transfer(client, bm_request_type, b_request, w_value, w_index, data, w_length, timeout);
}

IRECV_API irecv_error_t irecv_async_usb_control_transfer_with_cancel(irecv_client_t client, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value, uint16_t w_index, unsigned char *data, uint16_t w_length, unsigned int ns_time) {
    
    irecv_error_t error;
    async_transfer_t transfer;
    bzero(&transfer, sizeof(async_transfer_t));
    
    error = iokit_async_usb_control_transfer(client, bm_request_type, b_request, w_value, w_index, data, w_length, &transfer);
    if(error != IRECV_E_SUCCESS) {
        return error;
    }
    nsleep(ns_time);
    error = (*client->handle)->USBDeviceAbortPipeZero(client->handle);
    if(error != kIOReturnSuccess) {
        return IRECV_E_UNKNOWN_ERROR;
    }
    while(transfer.ret != kIOReturnAborted){
        CFRunLoopRun();
    }
    return transfer.len;
}

IRECV_API irecv_error_t irecv_reset(irecv_client_t client) {

    if (check_context(client) != IRECV_E_SUCCESS)
        return IRECV_E_NO_DEVICE;

    IOReturn result;
    
    result = (*client->handle)->ResetDevice(client->handle);
    if (result != kIOReturnSuccess && result != kIOReturnNotResponding) {
        debug("error sending device reset: %#x\n", result);
        return IRECV_E_UNKNOWN_ERROR;
    }
    
    result = (*client->handle)->USBDeviceReEnumerate(client->handle, 0);
    if (result != kIOReturnSuccess && result != kIOReturnNotResponding) {
        debug("error re-enumerating device: %#x (ignored)\n", result);
    }
    
    return IRECV_E_SUCCESS;
}

IRECV_API irecv_error_t irecv_open_with_ecid_and_attempts(irecv_client_t* pclient, unsigned long long ecid, int attempts) {

    int i;
    
    for (i = 0; i < attempts; i++) {
        if(*pclient) {
            irecv_close(*pclient);
            *pclient = NULL;
        }
        if (irecv_open_with_ecid(pclient, ecid) != IRECV_E_SUCCESS) {
            debug("Connection failed. Waiting 1 sec before retry.\n");
            sleep(1);
        } else {
            return IRECV_E_SUCCESS;
        }
    }
    
    return IRECV_E_UNABLE_TO_CONNECT;
}

static irecv_error_t irecv_get_status(irecv_client_t client, unsigned int* status) {
    if (check_context(client) != IRECV_E_SUCCESS) {
        *status = 0;
        return IRECV_E_NO_DEVICE;
    }
    
    unsigned char buffer[6];
    memset(buffer, '\0', 6);
    if (irecv_usb_control_transfer(client, 0xA1, 3, 0, 0, buffer, 6, USB_TIMEOUT) != 6) {
        *status = 0;
        return IRECV_E_USB_STATUS;
    }
    
    *status = (unsigned int) buffer[4];
    
    return IRECV_E_SUCCESS;
}

IRECV_API irecv_error_t irecv_finish_transfer(irecv_client_t client) {
    
    int i = 0;
    unsigned int status = 0;
    
    if (check_context(client) != IRECV_E_SUCCESS)
        return IRECV_E_NO_DEVICE;
    
    irecv_usb_control_transfer(client, 0x21, 1, 0, 0, 0, 0, USB_TIMEOUT);
    
    for(i = 0; i < 3; i++){
        irecv_get_status(client, &status);
    }
    
    irecv_reset(client);
    
    return IRECV_E_SUCCESS;
}


static int iokit_usb_bulk_transfer(irecv_client_t client,
                                   unsigned char endpoint,
                                   unsigned char *data,
                                   int length,
                                   int *transferred,
                                   unsigned int timeout) {
    
    IOReturn result;
    IOUSBInterfaceInterface300 **intf = client->usbInterface;
    UInt32 size = length;
    UInt8 transferDirection = endpoint & kUSBbEndpointDirectionMask;
    UInt8 numEndpoints;
    UInt8 pipeRef = 1;
    
    if (!intf) return IRECV_E_USB_INTERFACE;
    
    result = (*intf)->GetNumEndpoints(intf, &numEndpoints);
    
    if (result != kIOReturnSuccess || pipeRef > numEndpoints)
        return IRECV_E_USB_INTERFACE;
    
    // Just because
    result = (*intf)->GetPipeStatus(intf, pipeRef);
    switch (result) {
        case kIOReturnSuccess:  break;
        case kIOReturnNoDevice: return IRECV_E_NO_DEVICE;
        case kIOReturnNotOpen:  return IRECV_E_UNABLE_TO_CONNECT;
        default:                return IRECV_E_USB_STATUS;
    }
    
    // Do the transfer
    if (transferDirection == kUSBEndpointDirectionIn) {
        result = (*intf)->ReadPipeTO(intf, pipeRef, data, &size, timeout, timeout);
        if (result != kIOReturnSuccess)
            return IRECV_E_PIPE;
        *transferred = size;
        
        return IRECV_E_SUCCESS;
    }
    else {
        // IOUSBInterfaceClass::interfaceWritePipe (intf?, pipeRef==1, data, size=0x8000)
        result = (*intf)->WritePipeTO(intf, pipeRef, data, size, timeout, timeout);
        if (result != kIOReturnSuccess)
            return IRECV_E_PIPE;
        *transferred = size;
        
        return IRECV_E_SUCCESS;
    }
    
    return IRECV_E_USB_INTERFACE;
}

IRECV_API int irecv_usb_bulk_transfer(irecv_client_t client,
                                      unsigned char endpoint,
                                      unsigned char *data,
                                      int length,
                                      int *transferred,
                                      unsigned int timeout) {
    int ret;

    return iokit_usb_bulk_transfer(client, endpoint, data, length, transferred, timeout);
}


IRECV_API irecv_error_t irecv_send_buffer(irecv_client_t client, unsigned char* buffer, unsigned long length, int dfu_notify_finished) {

    irecv_error_t error = 0;
    int recovery_mode = ((client->mode != IRECV_K_DFU_MODE) && (client->mode != IRECV_K_WTF_MODE));
    
    if (check_context(client) != IRECV_E_SUCCESS)
        return IRECV_E_NO_DEVICE;
    
    unsigned int h1 = 0xFFFFFFFF;
    unsigned char dfu_xbuf[12] = {0xff, 0xff, 0xff, 0xff, 0xac, 0x05, 0x00, 0x01, 0x55, 0x46, 0x44, 0x10};
    int packet_size = recovery_mode ? 0x8000 : 0x800;
    int last = length % packet_size;
    int packets = length / packet_size;
    
    if (last != 0) {
        packets++;
    } else {
        last = packet_size;
    }
    
    /* initiate transfer */
    if (recovery_mode) {
        error = irecv_usb_control_transfer(client, 0x41, 0, 0, 0, NULL, 0, USB_TIMEOUT);
    } else {
        uint8_t state = 0;
        if (irecv_usb_control_transfer(client, 0xa1, 5, 0, 0, (unsigned char*)&state, 1, USB_TIMEOUT) == 1) {
            error = IRECV_E_SUCCESS;
        } else {
            return IRECV_E_USB_UPLOAD;
        }
        switch (state) {
            case 2:
                /* DFU IDLE */
                break;
            case 10:
                debug("DFU ERROR, issuing CLRSTATUS\n");
                irecv_usb_control_transfer(client, 0x21, 4, 0, 0, NULL, 0, USB_TIMEOUT);
                error = IRECV_E_USB_UPLOAD;
                break;
            default:
                debug("Unexpected state %d, issuing ABORT\n", state);
                irecv_usb_control_transfer(client, 0x21, 6, 0, 0, NULL, 0, USB_TIMEOUT);
                error = IRECV_E_USB_UPLOAD;
                break;
        }
    }
    
    if (error != IRECV_E_SUCCESS) {
        return error;
    }
    
    int i = 0;
    unsigned long count = 0;
    unsigned int status = 0;
    int bytes = 0;
    for (i = 0; i < packets; i++) {
        int size = (i + 1) < packets ? packet_size : last;
        
        /* Use bulk transfer for recovery mode and control transfer for DFU and WTF mode */
        if (recovery_mode) {
            error = irecv_usb_bulk_transfer(client, 0x04, &buffer[i * packet_size], size, &bytes, USB_TIMEOUT);
        } else {
            int j;
            for (j = 0; j < size; j++) {
                crc32_step(h1, buffer[i*packet_size + j]);
            }
            if (i+1 == packets) {
                if (size+16 > packet_size) {
                    bytes = irecv_usb_control_transfer(client, 0x21, 1, i, 0, &buffer[i * packet_size], size, USB_TIMEOUT);
                    if (bytes != size) {
                        return IRECV_E_USB_UPLOAD;
                    }
                    count += size;
                    size = 0;
                }
                
                for (j = 0; j < 2; j++) {
                    crc32_step(h1, dfu_xbuf[j*6 + 0]);
                    crc32_step(h1, dfu_xbuf[j*6 + 1]);
                    crc32_step(h1, dfu_xbuf[j*6 + 2]);
                    crc32_step(h1, dfu_xbuf[j*6 + 3]);
                    crc32_step(h1, dfu_xbuf[j*6 + 4]);
                    crc32_step(h1, dfu_xbuf[j*6 + 5]);
                }
                
                char* newbuf = (char*)malloc(size + 16);
                if (size > 0) {
                    memcpy(newbuf, &buffer[i * packet_size], size);
                }
                memcpy(newbuf+size, dfu_xbuf, 12);
                newbuf[size+12] = h1 & 0xFF;
                newbuf[size+13] = (h1 >> 8) & 0xFF;
                newbuf[size+14] = (h1 >> 16) & 0xFF;
                newbuf[size+15] = (h1 >> 24) & 0xFF;
                size += 16;
                bytes = irecv_usb_control_transfer(client, 0x21, 1, i, 0, (unsigned char*)newbuf, size, USB_TIMEOUT);
                free(newbuf);
            } else {
                bytes = irecv_usb_control_transfer(client, 0x21, 1, i, 0, &buffer[i * packet_size], size, USB_TIMEOUT);
            }
        }
        
        if (bytes != size) {
            return IRECV_E_USB_UPLOAD;
        }
        
        if (!recovery_mode) {
            error = irecv_get_status(client, &status);
        }
        
        if (error != IRECV_E_SUCCESS) {
            return error;
        }
        
        if (!recovery_mode && status != 5) {
            int retry = 0;
            
            while (retry++ < 20) {
                irecv_get_status(client, &status);
                if (status == 5) {
                    break;
                }
                sleep(1);
            }
            
            if (status != 5) {
                return IRECV_E_USB_UPLOAD;
            }
        }
        
        count += size;
        if(client->progress_callback != NULL) {
            irecv_event_t event;
            event.progress = ((double) count/ (double) length) * 100.0;
            event.type = IRECV_PROGRESS;
            event.data = (char*)"Uploading";
            event.size = count;
            client->progress_callback(client, &event);
        } else {
            debug("Sent: %d bytes - %lu of %lu\n", bytes, count, length);
        }
    }
    
    if (dfu_notify_finished && !recovery_mode) {
        irecv_usb_control_transfer(client, 0x21, 1, packets, 0, (unsigned char*) buffer, 0, USB_TIMEOUT);
        
        for (i = 0; i < 2; i++) {
            error = irecv_get_status(client, &status);
            if (error != IRECV_E_SUCCESS) {
                return error;
            }
        }
        
        if (dfu_notify_finished == 2) {
            /* we send a pseudo ZLP here just in case */
            irecv_usb_control_transfer(client, 0x21, 1, 0, 0, 0, 0, USB_TIMEOUT);
        }
        
        irecv_reset(client);
    }
    
    return IRECV_E_SUCCESS;
}

static irecv_error_t irecv_send_command_raw(irecv_client_t client, const char* command) {
    unsigned int length = strlen(command);
    if (length >= 0x100) {
        length = 0xFF;
    }
    
    if (length > 0) {
        irecv_usb_control_transfer(client, 0x40, 0, 0, 0, (unsigned char*) command, length + 1, USB_TIMEOUT);
    }
    
    return IRECV_E_SUCCESS;
}

IRECV_API irecv_error_t irecv_send_command(irecv_client_t client, const char* command) {

    irecv_error_t error = 0;
    
    if (check_context(client) != IRECV_E_SUCCESS)
        return IRECV_E_NO_DEVICE;
    
    unsigned int length = strlen(command);
    if (length >= 0x100) {
        length = 0xFF;
    }
    
    irecv_event_t event;
    if(client->precommand_callback != NULL) {
        event.size = length;
        event.data = command;
        event.type = IRECV_PRECOMMAND;
        if(client->precommand_callback(client, &event)) {
            return IRECV_E_SUCCESS;
        }
    }
    
    error = irecv_send_command_raw(client, command);
    if (error != IRECV_E_SUCCESS) {
        debug("Failed to send command %s\n", command);
        if (error != IRECV_E_PIPE)
            return error;
    }
    
    if(client->postcommand_callback != NULL) {
        event.size = length;
        event.data = command;
        event.type = IRECV_POSTCOMMAND;
        if(client->postcommand_callback(client, &event)) {
            return IRECV_E_SUCCESS;
        }
    }
    
    return IRECV_E_SUCCESS;

}
