#ifndef __ETH_DRIVER_H__
#define __ETH_DRIVER_H__

#ifdef USB_DRIVER
	#include "usb/usbeth_dev.h"
	#define driver_init USBETHdev_init
	#define driver_poll USBETHdev_poll
	#define driver_send USBETHdev_send
	#define driver_deinit USBETHdev_done

#else
	#include "netusbee/rtl8019.h"
	#define driver_init RTL8019dev_init
	#define driver_poll RTL8019dev_poll
	#define driver_send RTL8019dev_send
	#define driver_deinit RTL8019dev_done
#endif

#endif // __ETH_DRIVER_H__
