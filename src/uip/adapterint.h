
/* adapter interface */
#ifndef __ADAPTERINT_H__
#define __ADAPTERINT_H__

#ifndef EXTERN 
#define EXTERN 
#endif

/*****************************************************************************
*  beginPacketSend(uint32_t packetLength)
*  Args:        unsigned int - length of the Ethernet frame (see note)
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Sets up the NIC to send a packet
*  Notes:       The NIC will not send packets less than 60 bytes long (the min
*                 Ethernet frame length.  The transmit length is automatically
*                 increased to 60 bytes if packetLength is < 60
*****************************************************************************/
EXTERN void beginPacketSend(const uint32_t packetLength);


/*****************************************************************************
*  		sendPacketData(uint8_t * localBuffer, const uint32_t length)
*  Args:        1. unsigned char * localBuffer - Pointer to the beginning of
*                    the buffer to load into the NIC
*               2. unsigned char length - number of bytes to copy to
*                    the NIC
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Loads length # of bytes from a local buffer to the transmit
*                 packet buffer
*  Notes:       beginPacketSend() must be called before sending
*                 any data.
*               Several calls to retreivePacketData() may be made to 
*                 copy packet data from different buffers
*****************************************************************************/
EXTERN void sendPacketData(uint8_t * localBuffer, const uint32_t length);


/*****************************************************************************
*  endPacketSend()
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Ends a packet send operation and instructs the NIC to transmit
*                 the frame over the network
*****************************************************************************/
EXTERN void endPacketSend(void);


/*****************************************************************************
*  int32_t init(void);
*
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Sets up the RTL8019 NIC hardware interface, and initializes
*                 the buffers and configuration of the NIC
*****************************************************************************/
EXTERN int32_t init(uint8_t* macaddr, const uint32_t cpu_type);

/*****************************************************************************
*  processInterrupt(void);
*
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Reads the NIC's ISR register looking for a receive buffer
*                 overrun - which is then handled.
*  Notes:       The function does not need to be called in response to an
*                 interrupt.  The function can be polled and the NIC's INT
*                 line not used.  This function should be called before
*                 attempting to retreive a packet from the NIC
*****************************************************************************/
EXTERN void processInterrupt(void);

/*****************************************************************************
*  uint32_t beginPacketRetrieve()
*  Returns:     unsigned int - length of the Ethernet frame (see note)
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Sets up the NIC to retreive a packet
*  Notes:       The size returned is the size of all the data in the Ethernet
*                 frame minus the Ethernet checksum.  This may include unused
*                 trailer bytes appended if data is less than the minimum
*                 Ethernet frame length (60 bytes).  A size of zero indicates
*                 there are no packets available.
*               A call to beginPacketRetrieve() must be followed by a
*                 call to endPacketRetrieve() regardless if data is
*                 retreived, unless 0 is returned.
*****************************************************************************/
EXTERN uint32_t beginPacketRetrieve(void);


/*****************************************************************************
*  		retrievePacketData(uint8_t * localBuffer, const uint32_t length)
*  Args:        1. unsigned char * localBuffer - Pointer to the beginning of
*                    the buffer to store the ethernet frame.
*               2. unsigned char length - number of bytes to copy to
*                    localBuffer
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Loads length # of bytes from the receive packet buffer to
*                 a local buffer
*  Notes:       beginPacketRetrieve() must be called before retreiving
*                 any data.
*               Several calls to retrievePacketData() may be made to 
*                 copy packet data to different buffers
*****************************************************************************/
EXTERN void retrievePacketData(uint8_t *localBuffer, const uint32_t length);

/*****************************************************************************
*  endPacketRetrieve()
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Ends a packet retreive operation begun by calling
*                 beginPacketRetrieve().  The NIC buffer space used by
*                 the retreived packet is freed
*  Notes:       A packet may be removed from the buffer without being read
*                 by calling endPacketRetrieve() after
*                 beginPacketRetreive().
*****************************************************************************/
EXTERN void endPacketRetrieve(void);

/*****************************************************************************
*  int32_t destroy()
*  Returns:     signed int - 0 - success, 0 > error
*  Created By:  Pawel Goralski
*  Date:        May 3rd, 2019
*  Description: Perform internal resources cleanup on error or exit.
*****************************************************************************/
EXTERN int32_t destroy();

#undef EXTERN

#endif
