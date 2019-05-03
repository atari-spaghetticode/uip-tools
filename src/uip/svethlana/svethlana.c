
#include <string.h>

#include "svethlana.h"
#include "svregs.h"
#include "logging.h"

//global MAC address in longword format for RX use
static unsigned long mac_addr[2];

//next TX slot to be used
static volatile uint32_t slot_index = 0UL;

//We keep track of which slot we checked last time this function was called.
//Since the MAC controller probably does round robin on the RX slots, we
//shouldn't require Mintnet to reorganise our packets, if we always started
//with checking slot 0.
static uint32_t cur_rx_slot = 0;

//This is the pointer to the whole memory block for all the packet buffers needed.
//It's allocated in init() and deallocated at shutdown. The allocation is and
//must be done in SuperVidel video RAM (DDR RAM).
static int8_t* packets_base;

/* helper functions */
void delayMicrosec(long delay);

/* Convert single hex char to short */
int16_t ch2i(int8_t c);

/* Convert long to hex ascii */
void hex2ascii(uint32_t l, uint8_t* c);

int32_t check_rx_buffers();

extern uint32_t get_cookie(const uint32_t cookie, uint32_t *value);

void beginPacketSend(const uint32_t packetLength){

}


void sendPacketData(uint8_t* localBuffer, const uint32_t length){

}


void endPacketSend(){

}


int32_t init(uint8_t* macaddr, const uint32_t cpu_type){

uint8_t	macbuf[13] = {0};
uint8_t hwAddrBytes[10]={0};

LOG_TRACE("SVEthLANa adapter init..\n\r");

if(get_cookie(C_SupV, NULL) == 0) {
 LOG_WARN("Driver needs Supervidel/Svethlana hardware.\n\r");
 return -1; 
}

const uint32_t sv_fw_version = SV_VERSION & 0x3FFUL;

if (sv_fw_version < SV_MINIMAL_FW_VERSION){
	LOG_WARN("Svethlana adapter driver requires at last firmware ver. %u!\n\r", SV_MINIMAL_FW_VERSION);
	return -1;		
 }

 if(macaddr == NULL){
 	INFO("Using default ethernet address 01:02:03:04:05:07\n\r");
	macbuf[0] = '0';
	macbuf[1] = '1';
	macbuf[2] = '0';
	macbuf[3] = '2';
	macbuf[4] = '0';
	macbuf[5] = '3';
	macbuf[6] = '0';
	macbuf[7] = '4';
	macbuf[8] = '0';
	macbuf[9] = '5';
	macbuf[10] = '0';
	macbuf[11] = '7';
}else{
	INFO("Using preset ethernet address\n\r");
	
	macbuf[0] = macaddr[0];
	macbuf[1] = macaddr[1];
	macbuf[2] = macaddr[2];
	macbuf[3] = macaddr[3];
	macbuf[4] = macaddr[4];
	macbuf[5] = macaddr[5];
	macbuf[6] = macaddr[6];
	macbuf[7] = macaddr[7];
	macbuf[8] = macaddr[8];
	macbuf[9] = macaddr[9];
	macbuf[10] = macaddr[10];
	macbuf[11] = macaddr[11];
}

macbuf[12] = '0';

printf("MAC: %x:%x:%x:%x:%x:%x\r\n", 
	macbuf[0], 
	macbuf[1], 
	macbuf[2], 
	macbuf[3], 
	macbuf[4], 
	macbuf[5]
);

// Extract MAC address from macbuf
hwAddrBytes[0] = (uint8_t)(ch2i(macbuf[0]) * 16 + ch2i(macbuf[1]));
hwAddrBytes[1] = (uint8_t)(ch2i(macbuf[2]) * 16 + ch2i(macbuf[3]));
hwAddrBytes[2] = (uint8_t)(ch2i(macbuf[4]) * 16 + ch2i(macbuf[5]));
hwAddrBytes[3] = (uint8_t)(ch2i(macbuf[6]) * 16 + ch2i(macbuf[7]));
hwAddrBytes[4] = (uint8_t)(ch2i(macbuf[8]) * 16 + ch2i(macbuf[9]));
hwAddrBytes[5] = (uint8_t)(ch2i(macbuf[10]) * 16 + ch2i(macbuf[11]));

 /*
 * The actual init of the hardware
 *
 */
//Then write to the MODER register to reset the MAC controller
ETH_MODER = ETH_MODER_RST;

//Then release reset of the MAC controller
ETH_MODER = 0L;

// Set MAC address in MAC controller	
ETH_MAC_ADDR1 = (((uint32_t)(hwAddrBytes[0])) <<  8) + 
				(((uint32_t)(hwAddrBytes[1])) <<  0);
ETH_MAC_ADDR0 = (((uint32_t)(hwAddrBytes[2])) << 24) + 
				(((uint32_t)(hwAddrBytes[3])) << 16) +
				(((uint32_t)(hwAddrBytes[4])) <<  8) + 
				(((uint32_t)(hwAddrBytes[5])) <<  0);

//Inter packet gap
ETH_IPGT = 0x15UL;

//Create longword MAC address for RX use
mac_addr[0] = (((unsigned long)(hwAddrBytes[0])) << 24) +
			  (((unsigned long)(hwAddrBytes[1])) << 16) +
			  (((unsigned long)(hwAddrBytes[2])) <<  8) +
			  (((unsigned long)(hwAddrBytes[3])));

mac_addr[1] = (((unsigned long)(hwAddrBytes[4])) << 24) +
			  (((unsigned long)(hwAddrBytes[5])) << 16);


// allocate memory for internal buffers
int8_t *even_packet_base;
const size_t memSize = ETH_PKT_BUFFS * 2UL * 2048UL + 2048UL;
packets_base = (int8_t*)vmalloc(VMALLOC_MODE_ALLOC, memSize);
even_packet_base = (char*)((((unsigned long)packets_base) + 2047UL) & 0xFFFFF800UL);	//Make it all start at even 2048 bytes

if(packets_base == 0){
 LOG_WARN("Cannot allocate video memory %lu bytes for internal buffer!\n\r", packets_base);
 return -1;		
}

memset((void *)packets_base,0,memSize);

//Set number of TX packet BDs and RX BDs
ETH_TX_BD_NUM = ETH_PKT_BUFFS;

//Init each BD ctrl longword
//Init all RX slots to empty, so they can receive a packet
//Init all data pointers, so they get 2048 bytes each.
for (uint32_t i = 0; i < ETH_PKT_BUFFS; i++){
	if (i != (ETH_PKT_BUFFS-1)) {
		eth_tx_bd[i].len_ctrl = ETH_TX_BD_CRC;																	// no wrap
		eth_rx_bd[i].len_ctrl = ETH_RX_BD_EMPTY | ETH_RX_BD_IRQ;								// no wrap
	}else {
		eth_tx_bd[i].len_ctrl = ETH_TX_BD_CRC   | ETH_TX_BD_WRAP;									// wrap on last TX BD
		eth_rx_bd[i].len_ctrl = ETH_RX_BD_EMPTY | ETH_RX_BD_IRQ | ETH_RX_BD_WRAP;	// wrap on last RX BD
	}

	eth_tx_bd[i].data_pnt = ((uint32_t)even_packet_base) + (i*2048UL);		//TX buffers come first
	eth_rx_bd[i].data_pnt = ((uint32_t)even_packet_base) + ((ETH_PKT_BUFFS+i)*2048UL);		//then all RX buffers
}	

 // open
 ETH_INT_MASK = ETH_INT_MASK_RXF | ETH_INT_MASK_RXE | ETH_INT_MASK_TXE | ETH_INT_MASK_TXB | ETH_INT_MASK_BUSY;

 //Enable Transmit, full duplex, and CRC appending
 ETH_MODER = ETH_MODER_TXEN | ETH_MODER_RXEN | ETH_MODER_FULLD | ETH_MODER_CRCEN | ETH_MODER_RECSMALL | ETH_MODER_PAD;

 return 0;
}


void processInterrupt(void){

}

unsigned int beginPacketRetrieve(void){
	return 0;
}


void retrievePacketData(uint8_t *localBuffer, const uint32_t length){

}

void endPacketRetrieve(void){

}

int32_t destroy(void){

// close
// disable RX and TX
ETH_MODER = 0;

//disable RX and RX error int sources BEFORE removing I6 handler
ETH_INT_MASK = 0;

if(packets_base!=0) {
  uint32_t ret = vmalloc(VMALLOC_MODE_FREE,packets_base);
  packets_base = 0;
}

 LOG_TRACE("SVEthLANa network adapter destroyed. ");
 return 0;
}

/* Convert one ASCII char to a hex nibble */
int16_t ch2i(int8_t c){
	if(c >= '0' && c <= '9')
		return (int16_t)(c - '0');
	if(c >= 'A' && c <= 'F')
		return (int16_t)(c - 'A' + 10);
	if(c >= 'a' && c <= 'f')
		return (int16_t)(c - 'a' + 10);
	return 0;
}

// Convert an unsigned long to ASCII
void hex2ascii(uint32_t l, uint8_t* c){
	for(uint16_t i=7; i!=0; --i){

		const uint8_t t = (uint8_t)(l & 0xf);				// Keep only lower nibble
		
		if(t < 0xA)
			c[i] = t + '0';					// Output '0' to '9'
		else
			c[i] = t + 'A' - 0xA;			// Output 'A' to 'F'
		c[i] = '0';
		l = l >> 4;
	}
}

static int64_t getMicroseconds() {
  uint64_t timer200hz;
  uint32_t data;
resync:
  timer200hz = *((volatile uint32_t*)0x4BA) ;
  data = *((volatile uint8_t*)0xFFFFFA23);

  if ( *((volatile uint32_t*)0x4BA) != timer200hz )
  {
    goto resync;
  }

  timer200hz*=5000;       // convert to microseconds
  timer200hz+=(uint64_t)(((192-data)*6666)>>8); //26;     // convert data to microseconds
  return timer200hz;
}

void delayMicrosec(long delay){
    uint64_t future = getMicroseconds() + delay;
    while(future > getMicroseconds());
}
