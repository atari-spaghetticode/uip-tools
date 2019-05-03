#ifndef __DEVINT_H__
#define __DEVINT_H__

/*****************************************************************************
*  Module Name:       Driver Interface 
*  Created By:        Louis Beaudoin (www.embedded-creations.com)
*  Original Release:  September 21, 2002 
*  UIPTool modifications: Pawel Goralski (https://nokturnal.pl)

*  Module Description:  
*  Provides three functions to interface with device driver
*  These functions can be called directly from the main uIP control loop
*  to send packets from uip_buf and uip_appbuf, and store incoming packets to
*  uip_buf
*
*  September 30, 2002 - Louis Beaudoin
*    Modifications required to handle the packet receive function changes in
*      rtl8019.c.  There is no longer a need to poll for an empty buffer or
*      an overflow.
*    Added support for the Imagecraft Compiler
*   May 2, 2019 - Pawel Goralski
*	 changed interfaces, so alternative devices can be supported
*****************************************************************************/

#include "uip.h"

/*****************************************************************************
*  int32_t dev_init()
*  Returns:     0 if function suceeded, 0 > on error
*  Created By:  Louis Beaudoin
*  Date:        May 3, 2019
*  Description: Power-up initialization of device
*****************************************************************************/
int32_t dev_init(uint8_t* macaddr, const uint32_t cpu_type);

/*****************************************************************************
*  dev_send()
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Sends the packet contained in uip_buf and uip_appdata over
*                 the network
*****************************************************************************/
void dev_send(void);

/*****************************************************************************
*  uint32_t dev_poll()
*  Returns:     Length of the packet retreived, or zero if no packet retreived
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Polls the device looking for an overflow condition or a new
*                 packet in the receive buffer.  If a new packet exists and
*                 will fit in uip_buf, it is retreived, and the length is
*                 returned.  A packet bigger than the buffer is discarded
*****************************************************************************/
uint32_t dev_poll(void);

/*****************************************************************************
*  int32_t dev_destroy()
*  Returns:     0 if function suceeded, 0 > on error
*  Created By:  Pawel Goralski
*  Date:        May 3, 2019
*  Description: Cleanup resources used by device driver, returns S_OK on success 
*******************************************************************************/
int32_t dev_destroy();

#endif /* __DEVINT_H__ */
