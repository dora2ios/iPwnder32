#ifndef usb_H
#define usb_H

#include <libusb-1.0/libusb.h>

#define APPLE_VID 0x5AC
#define DFU_MAX_PACKET_SIZE 0x800
#define RECVOVERY_MAX_PACKET_SIZE 0x8000

#define DFU_MODE 0x1227

#define RECOVERY_MODE_1 0x1280
#define RECOVERY_MODE_2 0x1281
#define RECOVERY_MODE_3 0x1282
#define RECOVERY_MODE_4 0x1283

#define NORMAL_MODE 0x1290

#define USB_TIMEOUT 1000

typedef struct usb_device_t{
	libusb_device_handle* handle;
	int mode;
	unsigned char serial[256];
}* usb_device_t;

void usb_init();
void usb_exit();

int i3gs_bootrom(usb_device_t device);
int usb_is_dfu(usb_device_t device);
int usb_send_cmd(usb_device_t device, char* cmd);
int usb_bulk_transfer(usb_device_t device, unsigned char endpoint, unsigned char* data, int length, int* transferred, unsigned int timeout);
int usb_get_status(usb_device_t device);
int usb_request_image_validation(usb_device_t device);
int usb_is_checkm8_dfu(usb_device_t device);
int usb_send_buffer(usb_device_t device, unsigned char* buf, size_t len);
int usb_is_pwned_dfu(usb_device_t device);
uint16_t usb_get_cpid(usb_device_t device);
int usb_get_bdid(usb_device_t device);
char* usb_get_identifier(usb_device_t device);
usb_device_t usb_get_device_handle();
void usb_close(usb_device_t device);
int usb_reset(usb_device_t device);
int usb_ctrl_transfer(usb_device_t device, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value, uint16_t w_index, unsigned char *data, uint16_t w_length, unsigned int timeout);
int usb_async_ctrl_transfer(usb_device_t device, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value, uint16_t w_index, unsigned char *data, uint16_t w_length, float timeout);
usb_device_t usb_reconnect(usb_device_t device, float wait);

#endif
