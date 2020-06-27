#include <stdbool.h>

typedef struct iboot32_pacther {
    bool rsa;
    bool debug;
    bool ticket;
    bool setenv;
    bool boot_partition;
    int boot_partition_9;
    bool boot_ramdisk;
    bool local_boot;
    bool remote_boot;
    bool disable_kaslr;
    bool logo4;
    bool i433_hook;
    bool env_boot_args;
    char* custom_boot_args;
    char* cmd_handler_str;
    uint32_t cmd_handler_ptr;
    char* custom_color;
} iboot32_pacther_t;

void iboot32pacher_init(iboot32_pacther_t* conf);
int iBoot32Patcher(void* in, size_t sz, iboot32_pacther_t* conf);

