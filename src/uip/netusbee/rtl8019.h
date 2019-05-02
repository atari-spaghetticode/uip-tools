#ifndef __RTL8019_H__
#define __RTL8019_H__

/*****************************************************************************
*  Module Name:       Realtek 8019AS Driver
*  
*  Created By:        Louis Beaudoin (www.embedded-creations.com)
*
*  Original Release:  September 21, 2002 
*
*  Module Description:  
*  Provides functions to initialize the Realtek 8019AS, and send and retreive
*  packets
*
*  September 30, 2002 - Louis Beaudoin
*    Receive functions modified to handle errors encountered when receiving a
*      fast data stream.  Functions now manually retreive data instead of
*      using the send packet command.  Interface improved by checking for
*      overruns and data in the buffer internally.
*    Corrected the overrun function - overrun flag was not reset after overrun
*    Added support for the Imagecraft Compiler
*    Added support to communicate with the NIC using general I/O ports
*
*****************************************************************************/

#include "compiler.h"
#include <stdint.h>
#include <stdbool.h>

#include "../cdevint.h"

/*****************************************************************************
*
*  AVR hardware setup
*
*    External SRAM Interface:
*      The five NIC address lines are taken from A8-A12 (uses the
*      non-multiplexed address port so no latch is required)
*
*    General I/O Interface:
*      Two full ports are required for the address and data buses.  Two pins
*        from another port are used to control the read and write lines
*
*    One output pin is required for hard resetting the NIC
*
*****************************************************************************/

// set to 1 to use the External SRAM Interface - 0 for General I/O
#define MEMORY_MAPPED_NIC 0

#if MEMORY_MAPPED_NIC /*** NIC Interface through External SRAM Interface ****/

// NIC is mapped from address 0x8000 - 0x9F00
#define MEMORY_MAPPED_RTL8019_OFFSET 0x8300

#else /************ NIC Interface through General I/O *******************/

// RTL8019 address port
#define RTL8019_ADDRESS_PORT        PORTC
#define RTL8019_ADDRESS_DDR         DDRC

// RTL8019 data port
#define RTL8019_DATA_PORT           PORTA
#define RTL8019_DATA_DDR            DDRA
#define RTL8019_DATA_PIN            PINA

// RTL8019 control port
#define RTL8019_CONTROL_PORT        PORTD
#define RTL8019_CONTROL_DDR         DDRD
#define RTL8019_CONTROL_READPIN     PD7
#define RTL8019_CONTROL_WRITEPIN    PD6


// macros to control the read and write pins
#define RTL8019_CLEAR_READ   cbi(RTL8019_CONTROL_PORT,\
                                 RTL8019_CONTROL_READPIN)
#define RTL8019_SET_READ     sbi(RTL8019_CONTROL_PORT,\
                                 RTL8019_CONTROL_READPIN) 
#define RTL8019_CLEAR_WRITE  cbi(RTL8019_CONTROL_PORT,\
                                 RTL8019_CONTROL_WRITEPIN)
#define RTL8019_SET_WRITE    sbi(RTL8019_CONTROL_PORT,\
                                 RTL8019_CONTROL_WRITEPIN)

#endif /** NIC Interface **/


// RTL RESET - Port B pin 0
#define RTL8019_RESET_PORT 	PORTE
#define RTL8019_RESET_DDR 	DDRE
#define RTL8019_RESET_PIN 	PORTE0

/*****************************************************************************
*
*  Ethernet constants
*
*****************************************************************************/
#define ETHERNET_MIN_PACKET_LENGTH	0x3C
#define ETHERNET_HEADER_LENGTH		0x0E


/*****************************************************************************
*
* MAC address assigned to the RTL8019
*
*****************************************************************************/

#define MYMAC_0 0x00
#define MYMAC_1 0x06
#define MYMAC_2 0x98
#define MYMAC_3 0x01
#define MYMAC_4 0x02
#define MYMAC_5 0x26

#endif /* __RTL8019_H__ */
