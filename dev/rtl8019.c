#include "rtl8019.h"
#include "delay.h"
#include "debug.h"
#include "rtlregs.h"
#include <stdint.h>
#include <stdbool.h>

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
*  November 15, 2002 - Louis Beaudoin
*    processRTL8019Interrupt() - bit mask mistake fixed
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


/*****************************************************************************
*  writeRTL( RTL_ADDRESS, RTL_DATA )
*  Args:        1. unsigned char RTL_ADDRESS - register offset of RTL register
*               2. unsigned char RTL_DATA - data to write to register
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Writes byte to RTL8019 register.
*
*  Notes - If using the External SRAM Interface, performs a write to
*            address MEMORY_MAPPED_RTL8019_OFFSET + (RTL_ADDRESS<<8)
*            The address is sent in the non-multiplxed upper address port so
*            no latch is required.
*
*          If using general I/O ports, the data port is left in the input
*            state with pullups enabled
*
*****************************************************************************/

inline void writeRTL(const uint16_t address, const unsigned char data)
{
    // asm volatile
    // (   "   move.l  #0xfb0000,a0         ;"
    //     "   move.w %0,D0                ;"
    //     "   lsl.w #8,d0                 ;"
    //     "   move.b %1,d0                ;"
    //     "   tst.b (a0,d0.w*2)           ;"
    //    :    
    //    :    "o"(address),
    //         "o"(data)
    //    : "a0", "d0"
    // );

    ((uint8_t volatile*) 0xfb0000 )[ ((address<<8) | data) << 1] ? 1:0;
}

/*****************************************************************************
*  readRTL(RTL_ADDRESS)
*  Args:        unsigned char RTL_ADDRESS - register offset of RTL register
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Reads byte from RTL8019 register
*
*  Notes - If using the External SRAM Interface, performs a read from
*            address MEMORY_MAPPED_RTL8019_OFFSET + (RTL_ADDRESS<<8)
*            The address is sent in the non-multiplxed upper address port so
*            no latch is required.
*
*          If using general I/O ports, the data port is assumed to already be
*            an input, and is left as an input port when done
*
*****************************************************************************/

inline const unsigned char readRTL(const uint16_t address)
{
  // unsigned char byte=0;  
  //   asm volatile
  //   (   "   move.l  #0xfa0000,a0         ;"
  //       "   move.w %1,d0                ;"
  //       "   lsl.w  #8,d0                ;"
  //       "   move.b (a0,d0.w*2),%0       ;"
  //      :    "=r"(byte)
  //      :    "o"(address)
  //      : "a0", "d0"
  //   );
  //  return byte;

   return ((uint8_t volatile*) 0xfa0000 )[ address<<9 ];

}

/*****************************************************************************
*  RTL8019setupPorts(void);
*
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Sets up the ports used for communication with the RTL8019 NIC
*                 (data bus, address bus, read, write, and reset)
*****************************************************************************/
void RTL8019setupPorts(void)
{

}



/*****************************************************************************
*  HARD_RESET_RTL8019()
*
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: Simply toggles the pin that resets the NIC
*****************************************************************************/
/*#define HARD_RESET_RTL8019() do{ sbi(RTL8019_RESET_PORT, RTL8019_RESET_PIN); \
                                Delay_10ms(1); \
                                cbi(RTL8019_RESET_PORT, RTL8019_RESET_PIN);} \
                                while(0)*/



/*****************************************************************************
*  overrun(void);
*
*  Created By:  Louis Beaudoin
*  Date:        September 21, 2002
*  Description: "Canned" receive buffer overrun function originally from
*                 a National Semiconductor appnote
*  Notes:       This function must be called before retreiving packets from
*                 the NIC if there is a buffer overrun
*****************************************************************************/
void overrun(void);

//******************************************************************
//* REALTEK CONTROL REGISTER OFFSETS
//*   All offsets in Page 0 unless otherwise specified
//*   All functions accessing CR must leave CR in page 0 upon exit
//******************************************************************
#define CR          0x00
#define PSTART      0x01
#define PAR0        0x01    // Page 1
#define CR9346      0x01    // Page 3
#define PSTOP       0x02
#define BNRY        0x03
#define TSR         0x04
#define TPSR        0x04
#define TBCR0       0x05
#define NCR         0x05
#define TBCR1       0x06
#define ISR         0x07
#define CURR        0x07   // Page 1
#define RSAR0       0x08
#define CRDA0       0x08
#define RSAR1       0x09
#define CRDA1       0x09
#define RBCR0       0x0A
#define RBCR1       0x0B
#define RSR         0x0C
#define RCR         0x0C
#define TCR         0x0D
#define CNTR0       0x0D
#define DCR         0x0E
#define CNTR1       0x0E
#define IMR         0x0F
#define CNTR2       0x0F
#define RDMAPORT    0x10
#define RSTPORT     0x18


/*****************************************************************************
*
* RTL ISR Register Bits
*
*****************************************************************************/
#define ISR_RST 7
#define ISR_OVW 4
#define ISR_PRX 0
#define ISR_RDC 6
#define ISR_PTX 1


/*****************************************************************************
*
*  RTL Register Initialization Values
*
*****************************************************************************/
// RCR : accept broadcast packets and packets destined to this MAC
//         drop short frames and receive errors
#define RCR_INIT        0x04

// TCR : default transmit operation - CRC is generated 
#define TCR_INIT        0x00

// DCR : allows send packet to be used for packet retreival
//         FIFO threshold: 8-bits (works)
//         8-bit transfer mode
#define DCR_INIT        0x58

// IMR : interrupt enabled for receive and overrun events
#define IMR_INIT        0x11

// buffer boundaries - transmit has 6 256-byte pages
//   receive has 26 256-byte pages
//   entire available packet buffer space is allocated
#define TXSTART_INIT    0x40
#define RXSTART_INIT    0x46
#define RXSTOP_INIT     0x60

void RTL8019beginPacketSend(unsigned int packetLength)
{
    unsigned int sendPacketLength;
    sendPacketLength = (packetLength>=ETHERNET_MIN_PACKET_LENGTH) ?
                     packetLength : ETHERNET_MIN_PACKET_LENGTH ;
    
    //start the NIC
    writeRTL(CR,0x22);
    
    // still transmitting a packet - wait for it to finish
    while( readRTL(CR) & 0x04 );

    //load beginning page for transmit buffer
    writeRTL(TPSR,TXSTART_INIT);
    
    //set start address for remote DMA operation
    writeRTL(RSAR0,0x00);
    writeRTL(RSAR1,0x40);
    
    //clear the packet stored interrupt
    writeRTL(ISR,(1<<ISR_PTX));

    //load data byte count for remote DMA
    writeRTL(RBCR0, (unsigned char)(packetLength));
    writeRTL(RBCR1, (unsigned char)(packetLength>>8));

    writeRTL(TBCR0, (unsigned char)(sendPacketLength));
    writeRTL(TBCR1, (unsigned char)((sendPacketLength)>>8));
    
    //do remote write operation
    writeRTL(CR,0x12);
}

inline void RTL8019sendPacketData(unsigned char * localBuffer, unsigned int length)
{
    unsigned int i;
    for(i=0;i<length;i++)
        writeRTL(RDMAPORT, localBuffer[i]);
}

inline void RTL8019endPacketSend(void)
{
    //send the contents of the transmit buffer onto the network
    writeRTL(CR,0x24);
    
    // clear the remote DMA interrupt
    writeRTL(ISR, (1<<ISR_RDC));
}

// pointers to locations in the RTL8019 receive buffer
static unsigned char nextPage;
static unsigned int currentRetreiveAddress;

// location of items in the RTL8019's page header
#define  enetpacketstatus     0x00
#define  nextblock_ptr        0x01
#define  enetpacketLenL       0x02
#define  enetpacketLenH       0x03

unsigned int RTL8019beginPacketRetreive(void)
{
    unsigned char i;
    unsigned char bnry;
    
    unsigned char pageheader[4];
    unsigned int rxlen;
    
    // check for and handle an overflow
    processRTL8019Interrupt();
    
    // read CURR from page 1
    writeRTL(CR,0x62);
    i = readRTL(CURR);
    
    // return to page 0
    writeRTL(CR,0x22);
    
    // read the boundary register - pointing to the beginning of the packet
    bnry = readRTL(BNRY) ;

    /*  debug_print(("bnry: "));
        debug_print8(bnry);*/

    /*  debug_print(("RXSTOP_INIT: "));
    debug_print8(RXSTOP_INIT);
    debug_print(("RXSTART_INIT: "));
    debug_print8(RXSTART_INIT);*/
    // return if there is no packet in the buffer
    if( bnry == i ) {
      return 0;
    }
    

    // clear the packet received interrupt flag
    writeRTL(ISR, (1<<ISR_PRX));
    
    
    // the boundary pointer is invalid, reset the contents of the buffer and exit
    if( (bnry >= RXSTOP_INIT) || (bnry < RXSTART_INIT) )
    {
        writeRTL(BNRY, RXSTART_INIT);
        writeRTL(CR, 0x62);
        writeRTL(CURR, RXSTART_INIT);
        writeRTL(CR, 0x22);
        return 0;
    }

    // initiate DMA to transfer the RTL8019 packet header
    writeRTL(RBCR0, 4);
    writeRTL(RBCR1, 0);
    writeRTL(RSAR0, 0);
    writeRTL(RSAR1, bnry);
    writeRTL(CR, 0x0A);
    /*      debug_print(("Page header: "));*/

    for(i=0;i<4;i++) {
      pageheader[i] = readRTL(RDMAPORT);
      /*      debug_print8(pageheader[i]);*/
    }
    
    // end the DMA operation
    writeRTL(CR, 0x22);
    for(i = 0; i <= 20; i++) {
      if(readRTL(ISR) & 1<<6) {
    break;
      }
    }
    writeRTL(ISR, 1<<6);
    
    rxlen = (pageheader[enetpacketLenH]<<8) + pageheader[enetpacketLenL];
    nextPage = pageheader[nextblock_ptr] ;
    
    currentRetreiveAddress = (bnry<<8) + 4;

    /*  debug_print(("nextPage: "));
        debug_print8(nextPage);*/
    
    // if the nextPage pointer is invalid, the packet is not ready yet - exit
    if( (nextPage >= RXSTOP_INIT) || (nextPage < RXSTART_INIT) ) {
      /*      UDR0 = '0';*/
      return 0;
    }
    
    return rxlen-4;
}

void RTL8019retreivePacketData(unsigned char * localBuffer, unsigned int length)
{
    unsigned int i;
    // initiate DMA to transfer the data
    writeRTL(RBCR0, (unsigned char)length);
    writeRTL(RBCR1, (unsigned char)(length>>8));
    writeRTL(RSAR0, (unsigned char)currentRetreiveAddress);
    writeRTL(RSAR1, (unsigned char)(currentRetreiveAddress>>8));
    writeRTL(CR, 0x0A);
//    for(i=0;i<length;i++)
//        localBuffer[i] = readRTL(RDMAPORT);

    asm volatile
    (   
        "   move.l  %0,a0               \n"             // stack pointer
        "   move.l  #0xfa2000,a1               \n"             // stack pointer
        "   move.l  %1,d0               \n"             // push Run func argument
        "   subq.l  #1,d0               \n"             // push Run func argument
        "1: move.b  (a1),(a0)+          \n"             // push Run func argument
        "   dbf     d0,1b              \n"             // push Run func argument
        
       :    
       :    "g"( localBuffer ),
            "g"( length )
       :    "a0","a1","d0"
    );


    // end the DMA operation
    writeRTL(CR, 0x22);
    for(i = 0; i <= 20; i++)
        if(readRTL(ISR) & 1<<6)
            break;
    writeRTL(ISR, 1<<6);
    
    currentRetreiveAddress += length;
    if( currentRetreiveAddress >= 0x6000 )
        currentRetreiveAddress = currentRetreiveAddress - (0x6000-0x4600) ;
}

void RTL8019endPacketRetreive(void)
{
    unsigned char i;

    // end the DMA operation
    writeRTL(CR, 0x22);
    for(i = 0; i <= 20; i++)
        if(readRTL(ISR) & 1<<6)
            break;
    writeRTL(ISR, 1<<6);

    // set the boundary register to point to the start of the next packet
    writeRTL(BNRY, nextPage);
}

void overrun(void)
{
    unsigned char data_L, resend;   

    data_L = readRTL(CR);
    writeRTL(CR, 0x21);
  //  Delay_1ms(2);
    writeRTL(RBCR0, 0x00);
    writeRTL(RBCR1, 0x00);
    if(!(data_L & 0x04))
        resend = 0;
    else if(data_L & 0x04)
    {
        data_L = readRTL(ISR);
        if((data_L & 0x02) || (data_L & 0x08))
            resend = 0;
        else
            resend = 1;
    }
    
    writeRTL(TCR, 0x02);
    writeRTL(CR, 0x22);
    writeRTL(BNRY, RXSTART_INIT);
    writeRTL(CR, 0x62);
    writeRTL(CURR, RXSTART_INIT);
    writeRTL(CR, 0x22);
    writeRTL(ISR, 0x10);
    writeRTL(TCR, TCR_INIT);
    
    writeRTL(ISR, 0xFF);
}

/*!
 * \brief Size of a single ring buffer page.
 */
#define NIC_PAGE_SIZE   0x100

/*!
 * \brief First ring buffer page address.
 */
#define NIC_START_PAGE  0x40

/*!
 * \brief Last ring buffer page address plus 1.
 */
#define NIC_STOP_PAGE   0x60

/*!
 * \brief Number of pages in a single transmit buffer.
 *
 * This should be at least the MTU size.
 */
#define NIC_TX_PAGES    6

/*!
 * \brief Number of transmit buffers.
 */
#define NIC_TX_BUFFERS  2

/*!
 * \brief Controller memory layout:
 *
 * 0x4000 - 0x4bff  3k bytes transmit buffer
 * 0x4c00 - 0x5fff  5k bytes receive 
 */
#define NIC_FIRST_TX_PAGE   NIC_START_PAGE
#define NIC_FIRST_RX_PAGE   (NIC_FIRST_TX_PAGE + NIC_TX_PAGES * NIC_TX_BUFFERS)

/*!
 * \brief Standard sizing information
 */
#define TX_PAGES 12         /* Allow for 2 back-to-back frames */

static unsigned char mac[6] = {0x00,0x06,0x98,0x01,0x02,0x29};
void Delay(long nops)
{
    volatile long i;

    for(i = 0; i < nops; i++) 
#ifdef __IMAGECRAFT__
        asm("nop\n");
#else
        asm volatile("nop\n\t"::);
#endif
}

static bool NicReset(void)
{
//volatile unsigned char *base = (unsigned char *)0x8300;
    unsigned char i;
    unsigned char j;

    for(j = 0; j < 20; j++) {
        debug_print(("SW-Reset..."));
        i = readRTL(NIC_RESET);
        Delay(500);
        writeRTL(NIC_RESET, i);
        for(i = 0; i < 20; i++) {
            Delay(5000);

            /*
             * ID detection added for version 1.1 boards.
             */
            if((readRTL(NIC_PG0_ISR) & NIC_ISR_RST) != 0 &&
               readRTL(NIC_PG0_RBCR0) == 0x50 && 
               readRTL(NIC_PG0_RBCR1) == 0x70) {
                debug_print(("OK\r\n"));
                return true;
            }
        }
        debug_print(("failed\r\n\x07"));

        /*
         * Toggle the hardware reset line. Since Ethernut version 1.3 the 
         * hardware reset pin of the nic is no longer connected to bit 4 
         * on port E, but wired to the board reset line.
         */
        // if(j == 10) {
        //     debug_print(("Ethernut 1.1 HW-Reset\r\n"));
        //     sbi(DDRE, 4);
        //     sbi(PORTE, 4);
        //     Delay(100000);
        //     cbi(PORTE, 4);
        //     Delay(250000);
        // }
    }
    return false;
}

/* The EEPROM commands include the alway-set leading bit. */
enum EEPROM_Cmds { EE_WriteCmd=5, EE_ReadCmd=6, EE_EraseCmd=7, };
/* The description of EEPROM access. */
struct ee_ctrl_bits {
    int offset;
    unsigned char shift_clk,    /* Bit that drives SK (shift clock) pin */
        read_bit,               /* Mask bit for DO pin value */
        write_0, write_1,       /* Enable chip and drive DI pin with 0 / 1 */
        disable;                /* Disable chip. */
};
struct ee_ctrl_bits rtl_ee_tbl = {0x01,  0x04, 0x01, 0x88, 0x8A, 0x00 };

/* This executes a generic EEPROM command, typically a write or write enable.
   It returns the data output from the EEPROM, and thus may also be used for
   reads and EEPROM sizing. */

int debug = 0;

static int do_eeprom_cmd(struct ee_ctrl_bits *ee, int cmd,
                         int cmd_len)
{
    long ee_addr = ee->offset;
    unsigned retval = 0;

    if (debug > 1)
        printf(" EEPROM op 0x%x: ", cmd);

    /* Shift the command bits out. */
    do {
        short dataval = (cmd & (1 << cmd_len)) ? ee->write_1 : ee->write_0;
        writeRTL(ee_addr, dataval);
        if (debug > 2)
            printf("%X", readRTL(ee_addr) & 15);
        writeRTL(ee_addr, dataval |ee->shift_clk);
        retval = (retval << 1) | ((readRTL(ee_addr) & ee->read_bit) ? 1 : 0);
    } while (--cmd_len >= 0);
    writeRTL(ee_addr, ee->write_0);

    /* Terminate the EEPROM access. */
    writeRTL(ee_addr, ee->disable);
    if (debug > 1)
        printf(" EEPROM result is 0x%5.5x.\n", retval);
    return retval;
}

/* Wait for the EEPROM to finish what it is doing. */
static int eeprom_busy_poll(struct ee_ctrl_bits *ee)
{
    int ee_addr = ee->offset;
    int i;

    writeRTL(ee_addr, ee->write_0);
    for (i = 0; i < 10000; i++)         /* Typical 2000 ticks */
        if (readRTL(ee_addr) & ee->read_bit)
            break;
    return i;
}
/* The abstracted functions for EEPROM access. */
static int read_eeprom(struct ee_ctrl_bits *ee, int location)
{
    int addr_len = 6;
    return do_eeprom_cmd(ee, ((EE_ReadCmd << addr_len) | location) << 16,
                         3 + addr_len + 16) & 0xffff;
}


void RTL8019getMac(uint8_t* macaddr)
{
    uint8_t tempCR;
    // switch register pages
    //tempCR = readRTL(NIC_CR);
    //writeRTL(CR,tempCR|NIC_CR_PS0);

    //writeRTL(NIC_CR, E8390_NODMA+E8390_PAGE3);
    writeRTL(NIC_CR,  NIC_CR_RD2 | NIC_CR_PS0 | NIC_CR_PS1);
    // read MAC address registers
    for ( int i = 0 ; i < 3; ++i ) {
        uint16_t val = read_eeprom(&rtl_ee_tbl, 2 + i );
        macaddr[i*2] = val&0xff;
        macaddr[i*2+1] = val>>8;
    }
    // switch register pages back
    writeRTL(CR,tempCR);
}

bool initRTL8019(uint8_t* macaddr)
{
    unsigned char i, rb;

    if ( !NicReset() ) {
        return false;
    }

    RTL8019getMac( macaddr );

    printf("MAC: %x:%x:%x:%x:%x:%x\r\n", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);


    debug_print(("Init controller..."));
    writeRTL(NIC_PG0_IMR, 0);
    writeRTL(NIC_PG0_ISR, 0xff);
    writeRTL(NIC_CR, NIC_CR_STP | NIC_CR_RD2 | NIC_CR_PS0 | NIC_CR_PS1);
    writeRTL(NIC_PG3_EECR, NIC_EECR_EEM0 | NIC_EECR_EEM1);
    writeRTL(NIC_PG3_CONFIG3, 0);
    writeRTL(NIC_PG3_CONFIG2, NIC_CONFIG2_BSELB);
    writeRTL(NIC_PG3_EECR, 0);
    /*    Delay(50000);*/
    Delay_10ms(200);
    writeRTL(NIC_CR, NIC_CR_STP | NIC_CR_RD2);
    writeRTL(NIC_PG0_DCR, NIC_DCR_LS | NIC_DCR_FT1);
    writeRTL(NIC_PG0_RBCR0, 0);
    writeRTL(NIC_PG0_RBCR1, 0);
    writeRTL(NIC_PG0_RCR, NIC_RCR_MON);
    writeRTL(NIC_PG0_TCR, NIC_TCR_LB0);
    writeRTL(NIC_PG0_TPSR, NIC_FIRST_TX_PAGE);
    writeRTL(NIC_PG0_BNRY, NIC_STOP_PAGE - 1);
    writeRTL(NIC_PG0_PSTART, NIC_FIRST_RX_PAGE);
    writeRTL(NIC_PG0_PSTOP, NIC_STOP_PAGE);
    writeRTL(NIC_PG0_ISR, 0xff);
    writeRTL(NIC_CR, NIC_CR_STP | NIC_CR_RD2 | NIC_CR_PS0);
    for(i = 0; i < 6; i++)
         writeRTL(NIC_PG1_PAR0 + i, macaddr[i]);
    for(i = 0; i < 8; i++)
        writeRTL(NIC_PG1_MAR0 + i, 0);
    writeRTL(NIC_PG1_CURR, NIC_START_PAGE + TX_PAGES);
    writeRTL(NIC_CR, NIC_CR_STP | NIC_CR_RD2);
    writeRTL(NIC_PG0_RCR, NIC_RCR_AB | 2);
    writeRTL(NIC_PG0_ISR, 0xff);
    writeRTL(NIC_PG0_IMR, 0);
    writeRTL(NIC_CR, NIC_CR_STA | NIC_CR_RD2);
    writeRTL(NIC_PG0_TCR, 0);
    /*    Delay(1000000)*/
    Delay_10ms(200);


    writeRTL(NIC_CR, NIC_CR_STA | NIC_CR_RD2 | NIC_CR_PS0 | NIC_CR_PS1);
    rb = readRTL(NIC_PG3_CONFIG0);
    debug_print8(rb);
    switch(rb & 0xC0) {
    case 0x00:
        debug_print(("RTL8019AS "));
        if(rb & 0x08)
            debug_print(("jumper mode: "));
        if(rb & 0x20)
            debug_print(("AUI "));
        if(rb & 0x10)
            debug_print(("PNP "));
        break;
    case 0xC0:
        debug_print(("RTL8019 "));
        if(rb & 0x08)
            debug_print(("jumper mode: "));
        break;
    default:
        debug_print(("Unknown chip "));
    debug_print8(rb);
        break;
    }
    if(rb & 0x04)
        debug_print(("BNC\x07 "));
    if(rb & 0x03)
        debug_print(("Failed\x07 "));

    /*    rb = readRTL(NIC_PG3_CONFIG1);
      debug_print8(rb);*/
    /*    NutPrintFormat(0, "IRQ%u ", (rb >> 4) & 7);*/
    /*    debug_print("IRQ ");
      debug_print8((rb >> 4) & 7);*/

    rb = readRTL(NIC_PG3_CONFIG2);
    debug_print8(rb);
    switch(rb & 0xC0) {
    case 0x00:
        debug_print(("Auto "));
        break;
    case 0x40:
        debug_print(("10BaseT "));
        break;
    case 0x80:
        debug_print(("10Base5 "));
        break;
    case 0xC0:
        debug_print(("10Base2 "));
        break;
    }

    return true;
}


void processRTL8019Interrupt(void)
{
  unsigned char byte = readRTL(ISR);
    
  if( byte & (1<<ISR_OVW) )
    overrun();

}

/*
  unsigned char RTL8019ReceiveEmpty(void)
  {
  unsigned char temp;

  // read CURR from page 1
  writeRTL(CR,0x62);
  temp = readRTL(CURR);
    
  // return to page 0
  writeRTL(CR,0x22);
    
  return ( readRTL(BNRY) == temp );
    
  }*/




