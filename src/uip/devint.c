#include "uip.h"
#include "devint.h"

// interface for used ethernet adapter
#define EXTERN extern
#include "adapterint.h"

/*****************************************************************************
*  Module Name:       Driver Interface for uIP m68k Atari TOS Port
*  
*  Created By:        Louis Beaudoin (www.embedded-creations.com)
*
*  Original Release:  September 21, 2002 
*
*  Module Description:  
*  Provides three functions to interface with the Realtek 8019AS driver
*  These functions can be called directly from the main uIP control loop
*  to send packets from uip_buf and uip_appbuf, and store incoming packets to
*  uip_buf
*
*  September 30, 2002 - Louis Beaudoin
*    Modifications required to handle the packet receive function changes in
*      rtl8019.c.  There is no longer a need to poll for an empty buffer or
*      an overflow.
*    Added support for the Imagecraft Compiler
*
*   May 2, 2019 - Pawel Goralski
*  changed interfaces, so more devices can be supported
*
*****************************************************************************/

#define TOTAL_HEADER_LENGTH (UIP_TCPIP_HLEN + UIP_LLH_LEN)


bool dev_init(const uint8_t* macaddr, const uint32_t cpu_type){
    return init(macaddr, cpu_type);
}

void dev_send(void) {

  beginPacketSend(uip_len);

  // send packet, using data in uip_appdata if over the IP+TCP header size
  if (uip_len <= TOTAL_HEADER_LENGTH) {
        sendPacketData(uip_buf, uip_len);
  } else {
        sendPacketData(uip_buf, TOTAL_HEADER_LENGTH);
        sendPacketData((unsigned char *)uip_appdata, uip_len-TOTAL_HEADER_LENGTH);
  }

    endPacketSend();
}

unsigned int dev_poll(void) {

    const unsigned int packetLength = beginPacketRetrieve();

    // if there's no packet or an error - exit without ending the operation
    if (!packetLength)
      return 0;

    // drop anything too big for the buffer
    if (packetLength > UIP_BUFSIZE)
    {
        endPacketRetrieve();
        return 0;
    }

    // copy the packet data into the uIP packet buffer
    retrievePacketData(uip_buf, packetLength);
    endPacketRetrieve();

    return packetLength;
}
