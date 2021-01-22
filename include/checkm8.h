
#include <stdint.h>

int checkm8_32_exploit(irecv_client_t client, irecv_device_t device, const struct irecv_device_info *devinfo);

typedef struct checkm8_32 {
    uint16_t large_leak;
    int overwrite_offset;
    unsigned char* overwrite;
    size_t overwrite_len;
    unsigned char* payload;
    size_t payload_len;
} checkm8_32_t;

int get_payload_configuration(uint16_t cpid, const char* identifier, checkm8_32_t* config);
