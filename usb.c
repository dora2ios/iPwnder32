#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h> 
#include <sys/time.h> 
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include <include/usb.h>
#include <include/device_identifiers.h>
#include <include/crc32_table.h>

#define crc32_step(a,b) \
	a = (crc32_lookup_t1[(a & 0xFF) ^ ((unsigned char)b)] ^ (a >> 8))

static libusb_context* usb_context;

void usb_init() {
	libusb_init(&usb_context);
}

void usb_exit() {
	libusb_exit(usb_context);
}

int i3gs_bootrom(usb_device_t device) {
    /* 1 .. oldBR *
     * 2 .. newBR */
    char* i3gs_ptr = strstr((char*)device->serial, "SRTG:[iBoot-359.3]");
    if(i3gs_ptr) {
        return 0;
    }
    return 1;
}

int usb_is_checkm8_dfu(usb_device_t device) {
	char* pwnd_ptr = strstr((char*)device->serial, "PWND:[checkm8]");
	if(!pwnd_ptr) {
		return 0;
	}
	return 1;
}

int usb_is_pwned_dfu(usb_device_t device) {
	char* pwnd_ptr = strstr((char*)device->serial, "PWND");
	if(!pwnd_ptr) {
		return 0;
	}
	return 1;
}

uint16_t usb_get_cpid(usb_device_t device) {
	char* cpid_ptr = strstr((char*)device->serial, "CPID:");
	if(!cpid_ptr) {
		return 0;
	}
	uint16_t cpid;
	sscanf(cpid_ptr, "CPID:%hx", &cpid);
	return cpid;
}

int usb_get_bdid(usb_device_t device) {
	char* bdid_ptr = strstr((char*)device->serial, "BDID:");
	if(!bdid_ptr) {
		return -1;
	}
	uint32_t bdid;
	sscanf(bdid_ptr, "BDID:%x", &bdid);
	return (uint8_t)bdid;
}

char* usb_get_identifier(usb_device_t device) {
	int i = 0;
	uint16_t cpid = usb_get_cpid(device);
	if(!cpid) {
		return NULL;
	}
	int bdid = usb_get_bdid(device);
	if(bdid == -1) {
		return NULL;
	}
	while(device_identifiers[i].product_identifier != NULL) {
		if(device_identifiers[i].cpid == cpid && device_identifiers[i].bdid == bdid) {
			return device_identifiers[i].product_identifier;
		}
		i += 1;
	}
	return NULL;
}

int usb_is_dfu(usb_device_t device) {
	return device->mode == DFU_MODE;
}

usb_device_t usb_get_device_handle() {
	int ret;
	usb_device_t device = malloc(sizeof(struct usb_device_t));
	if(!device) {
		return NULL;
	}
	memset(device, '\x00', sizeof(struct usb_device_t));
	libusb_device **list = NULL;
	struct libusb_device_descriptor desc;
	int count = libusb_get_device_list(usb_context, &list);
	libusb_device* usb_device = NULL;
	for(int i = 0; i < count; i++) {
		usb_device = list[i];
		ret = libusb_get_device_descriptor(usb_device, &desc);
		if(ret != 0) {
			return NULL;
		}
		if(desc.idVendor == APPLE_VID && (desc.idProduct == DFU_MODE || desc.idProduct == RECOVERY_MODE_1 || desc.idProduct == RECOVERY_MODE_2 || desc.idProduct == RECOVERY_MODE_3 || desc.idProduct == RECOVERY_MODE_4 || desc.idProduct == NORMAL_MODE)) {
			break;
		}
	}
	if(usb_device == NULL) {
		return NULL;
	}
	device->mode = desc.idProduct;
	ret = libusb_open(usb_device, &device->handle);
	if(ret != 0){
		return NULL;
	}
	libusb_get_string_descriptor_ascii(device->handle, desc.iSerialNumber, device->serial, 255);
	libusb_free_device_list(list, 0);
	if(ret != 0){
        libusb_close(device->handle);
        free(device);
		return NULL;
	}
	ret = libusb_set_configuration(device->handle, 1);
	if(ret != 0){
        libusb_close(device->handle);
		return NULL;
	}
	if(device->mode > RECOVERY_MODE_2) {
		ret = libusb_claim_interface(device->handle, 1);
		if(ret != 0){
            libusb_close(device->handle);
			return NULL;
		}
		ret = libusb_set_interface_alt_setting(device->handle, 1, 1);
		if (ret != 0) {
            libusb_close(device->handle);
			return NULL;
		}
	}
	else {
		ret = libusb_claim_interface(device->handle, 0);
		if(ret != 0){
            libusb_close(device->handle);
			return NULL;
		}
	}
	return device;
}

void usb_close(usb_device_t device) {
	if(!device) {
		return;
	}
	if(device->mode > RECOVERY_MODE_2) {
		libusb_release_interface(device->handle, 1);
	}
	else {
		libusb_release_interface(device->handle, 0);
	}
	libusb_close(device->handle);
	free(device);
}

int usb_reset(usb_device_t device) {
    int ret = libusb_reset_device(device->handle);
	return ret;
}

int usb_get_status(usb_device_t device) {
	unsigned char buf[6];
	int ret = usb_ctrl_transfer(device, 0xA1, 3, 0, 0, buf, 6, USB_TIMEOUT);
	if(ret < 0){
		return -1;
	}
	return 0;
}

int usb_request_image_validation(usb_device_t device) {
	int ret;
	usb_ctrl_transfer(device, 0x21, 1, 0, 0, 0, 0, USB_TIMEOUT);
	for(int i = 0; i < 3; i++) {
		ret = usb_get_status(device);
		if(ret != 0) {
			return -1;
		}
	}
	usb_reset(device);
	return 0;
}

int usb_send_buffer(usb_device_t device, unsigned char* buf, size_t len) {
	int packet_size = usb_is_dfu(device) ? DFU_MAX_PACKET_SIZE : RECVOVERY_MAX_PACKET_SIZE;
	int last = len % packet_size;
	int packets = len / packet_size;
	int ret;
	if (last != 0) {
		packets++;
	} else {
		last = packet_size;
	}
	unsigned int h1 = 0xFFFFFFFF;
	unsigned char dfu_xbuf[12] = {0xff, 0xff, 0xff, 0xff, 0xac, 0x05, 0x00, 0x01, 0x55, 0x46, 0x44, 0x10};
	if(usb_is_dfu(device)){
		for (int i = 0; i < packets; i++) {
			int size = (i + 1) < packets ? packet_size : last;
			int j;
			for (int j = 0; j < size; j++) {
				crc32_step(h1, buf[i * packet_size + j]);
			}
			if (i+1 == packets) {
				if (size+16 > packet_size) {
					ret = usb_ctrl_transfer(device, 0x21, 1, i, 0, &buf[i * packet_size], size, USB_TIMEOUT);
					if (ret != size) {
						return -1;
					}
					size = 0;
				}
				for (j = 0; j < 2; j++) {
					crc32_step(h1, dfu_xbuf[j * 6 + 0]);
					crc32_step(h1, dfu_xbuf[j * 6 + 1]);
					crc32_step(h1, dfu_xbuf[j * 6 + 2]);
					crc32_step(h1, dfu_xbuf[j * 6 + 3]);
					crc32_step(h1, dfu_xbuf[j * 6 + 4]);
					crc32_step(h1, dfu_xbuf[j * 6 + 5]);
				}

				char* newbuf = (char*)malloc(size + 16);
				memcpy(newbuf, &buf[i * packet_size], size);
				memcpy(newbuf+size, dfu_xbuf, 12);
				newbuf[size+12] = h1 & 0xFF;
				newbuf[size+13] = (h1 >> 8) & 0xFF;
				newbuf[size+14] = (h1 >> 16) & 0xFF;
				newbuf[size+15] = (h1 >> 24) & 0xFF;
				size += 16;
				ret = usb_ctrl_transfer(device, 0x21, 1, 0, 0, (unsigned char*) newbuf, size, USB_TIMEOUT);
				if(ret != size) {
					return -1;
				}
				free(newbuf);
			} else {
				ret = usb_ctrl_transfer(device, 0x21, 1, 0, 0, &buf[i * packet_size], size, USB_TIMEOUT);
				if(ret != size) {
					return -1;
				}
			}
		}
	}
	else {
		usb_ctrl_transfer(device, 0x41, 0, 0, 0, NULL, 0, USB_TIMEOUT);
		for (int i = 0; i < packets; i++) {
			int bytes;
			int size = (i + 1) < packets ? packet_size : last;
			ret = usb_bulk_transfer(device, 0x04, &buf[i * packet_size], size, &bytes, USB_TIMEOUT);
			if(ret != 0 || bytes != size) {
				return -1;
			}
		}
	}
	return 0;
}

int usb_send_cmd(usb_device_t device, char* cmd) {
	int ret;
	ret = usb_ctrl_transfer(device, 0x40, 0, 0, 0, (unsigned char*)cmd, strlen(cmd) + 1, 30000);
	if(ret != strlen(cmd) + 1) {
		return -1;
	}
	return 0;
}

int usb_bulk_transfer(usb_device_t device, unsigned char endpoint, unsigned char* data, int length, int* transferred, unsigned int timeout) {
	return libusb_bulk_transfer(device->handle, endpoint, data, length, transferred, timeout);
}

int usb_ctrl_transfer(usb_device_t device, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value, uint16_t w_index, unsigned char *data, uint16_t w_length, unsigned int timeout) {
	return libusb_control_transfer(device->handle, bm_request_type, b_request, w_value, w_index, data, w_length, timeout);
}

 // https://stackoverflow.com/a/36095407 and https://stackoverflow.com/a/6725161

static long get_nanos(void) {
	struct timespec ts;
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts.tv_sec = mts.tv_sec;
	ts.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_REALTIME, &ts);
#endif
	return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}


int usb_async_ctrl_transfer(usb_device_t device, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value, uint16_t w_index, unsigned char *data, uint16_t w_length, float timeout) {
	int ret;
    long start = get_nanos();
	unsigned char* buffer = malloc(w_length + 8);
	struct libusb_transfer* transfer = libusb_alloc_transfer(0);
	if(!transfer) {
		return -1;
	}
	memcpy((buffer + 8), data, w_length);
    libusb_fill_control_setup(buffer, bm_request_type, b_request, w_value, w_index, w_length);
	libusb_fill_control_transfer(transfer, device->handle, buffer, NULL, NULL, 0);
	transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
	ret = libusb_submit_transfer(transfer);
	if(ret != 0) {
		return -1;
	}
	while((get_nanos() - start) < (timeout * (1000000/*10 * 6*/)));
	ret = libusb_cancel_transfer(transfer);
	if(ret != 0) {
		return -1;
	}
	free(buffer);
	return 0;
}

usb_device_t usb_reconnect(usb_device_t device, float wait) {
	usb_close(device);
	if(wait != 0){
		long start = get_nanos();
		while((get_nanos() - start) < (wait * (1000000/*10 * 6*/)));
	}
	usb_device_t new_device = NULL;
	for(int i = 0; i < 5; i++){
		new_device = usb_get_device_handle();
		if(new_device){
			return new_device;
		}
		sleep(1);
	}
	return NULL;
}	


