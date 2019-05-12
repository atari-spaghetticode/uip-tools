
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

volatile uint32_t ethIntSrcFlag;
volatile int32_t RxBufferSlotId;
static uint32_t inErrors;
static uint32_t outErrors;

/* helper functions */
void delayMicrosec(long delay);

/* Convert single hex char to short */
int16_t ch2i(int8_t c);

/* Convert long to hex ascii */
void hex2ascii(uint32_t l, uint8_t* c);

const int32_t check_rx_buffers();
static int32_t send_packet (uint32_t *buffer, const uint32_t bufferLength, const uint32_t slot, const uint32_t last_packet);
extern uint32_t get_cookie(const uint32_t cookie, uint32_t *value);

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

INFO("Using default MAC address: [01:02:03:04:05:07]\n\r");

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
macbuf[12] = '0';

// Extract MAC address from macbuf
hwAddrBytes[0] = (uint8_t)(ch2i(macbuf[0]) * 16 + ch2i(macbuf[1]));
hwAddrBytes[1] = (uint8_t)(ch2i(macbuf[2]) * 16 + ch2i(macbuf[3]));
hwAddrBytes[2] = (uint8_t)(ch2i(macbuf[4]) * 16 + ch2i(macbuf[5]));
hwAddrBytes[3] = (uint8_t)(ch2i(macbuf[6]) * 16 + ch2i(macbuf[7]));
hwAddrBytes[4] = (uint8_t)(ch2i(macbuf[8]) * 16 + ch2i(macbuf[9]));
hwAddrBytes[5] = (uint8_t)(ch2i(macbuf[10]) * 16 + ch2i(macbuf[11]));

INFO("MAC: %x:%x:%x:%x:%x:%x\r\n", 
	hwAddrBytes[0], 
	hwAddrBytes[1], 
	hwAddrBytes[2], 
	hwAddrBytes[3], 
	hwAddrBytes[4], 
	hwAddrBytes[5]
);

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
const size_t memSize = ETH_PKT_BUFFS * 2UL * 2048UL + 2048UL;
packets_base = (int8_t*)vmalloc(VMALLOC_MODE_ALLOC, memSize);

if(packets_base == 0){
 LOG_WARN("Cannot allocate video memory %lu bytes for internal buffer!\n\r", packets_base);
 return -1;		
}

memset((void *)packets_base,0UL,memSize);

int8_t *even_packet_base = (char*)((((unsigned long)packets_base) + 2047UL) & 0xFFFFF800UL);	//Make it all start at even 2048 bytes

//Set number of TX packet BDs and RX BDs
ETH_TX_BD_NUM = ETH_PKT_BUFFS;

//Init each BD ctrl longword
//Init all RX slots to empty, so they can receive a packet
//Init all data pointers, so they get 2048 bytes each.
for (uint32_t i = 0; i < ETH_PKT_BUFFS; ++i){

	if (i != (ETH_PKT_BUFFS-1)) {
		eth_tx_bd[i].len_ctrl = ETH_TX_BD_CRC;												// no wrap
		eth_rx_bd[i].len_ctrl = ETH_RX_BD_EMPTY | ETH_RX_BD_IRQ;							// no wrap
	}else {
		eth_tx_bd[i].len_ctrl = ETH_TX_BD_CRC   | ETH_TX_BD_WRAP;							// wrap on last TX BD
		eth_rx_bd[i].len_ctrl = ETH_RX_BD_EMPTY | ETH_RX_BD_IRQ | ETH_RX_BD_WRAP;			// wrap on last RX BD
	}

	eth_tx_bd[i].data_pnt = ((uint32_t)even_packet_base) + (i*2048UL);						//TX buffers come first
	eth_rx_bd[i].data_pnt = ((uint32_t)even_packet_base) + ((ETH_PKT_BUFFS+i)*2048UL);		//then all RX buffers
}	

 // open
 ETH_INT_MASK = ETH_INT_MASK_RXF | ETH_INT_MASK_RXE | ETH_INT_MASK_TXE | ETH_INT_MASK_TXB | ETH_INT_MASK_BUSY;

 //Enable Transmit, full duplex, and CRC appending
 ETH_MODER = ETH_MODER_TXEN | ETH_MODER_RXEN | ETH_MODER_FULLD | ETH_MODER_CRCEN | ETH_MODER_RECSMALL | ETH_MODER_PAD;

 ethIntSrcFlag = 0;
 RxBufferSlotId = 0;
 inErrors = 0;
 outErrors = 0;

 return 0;
}



void processInterrupt(){
	//LOG_TRACE("processInterrupt()..\n\r"); // not used?
}

void beginPacketSend(const uint32_t packetLength){
	//LOG_TRACE("beginPacketSend(), lenght:[%d].\n\r",packetLength);
}

void sendPacketData(uint8_t* localBuffer, const uint32_t length){
	//LOG_TRACE("sendPacketData(), buffer[0x%lx] lenght:[%d].\n\r",(uint32_t)localBuffer,length);

	if ((ethIntSrcFlag & (ETH_INT_TXB || ETH_INT_TXE)) != 0){
		if(ethIntSrcFlag & ETH_INT_TXE){
			// TX eror set => failed!
			++outErrors;
		}

		//A Transmit Error means that the slot is free to be used.
		//TODO: Check first that the slot that is in turn to be used is free.
		if (((uint32_t)(eth_tx_bd[RxBufferSlotId].len_ctrl & ETH_TX_BD_READY)) != 0UL) return;

			//Possible errors returned are -1 and -2, meaning "no free slot" and "too large packet" respectively.
			//-1 isn't possible because of the check above, and -2 isn't possible either, because
			//we don't enqueue packets in svethlana_output() that are too large.
			const int32_t ret = send_packet((uint32_t *)localBuffer, length, RxBufferSlotId, RxBufferSlotId == (ETH_PKT_BUFFS-1));
			RxBufferSlotId = (RxBufferSlotId + 1) & (ETH_PKT_BUFFS-1);

			if(ret!=0){
				LOG_TRACE("sendPacketData(), failed:[%d].\n\r",(uint32_t)ret);				
			}
	}
}

void endPacketSend(){
	//LOG_TRACE("endPacketSend()..\n\r");
}


uint32_t beginPacketRetrieve(){
	
	//LOG_TRACE("beginPacketRetrieve()..\n\r");
	uint32_t packetLenght = 0UL;

	ethIntSrcFlag = ETH_INT_SOURCE;

	//Clear all flags by writing 1 to them
	ETH_INT_SOURCE = 0x7FUL;
	
	if (ethIntSrcFlag & ETH_INT_BUSY){
		LOG_TRACE ("Busy...\r\n");	
	}
	
	if (ethIntSrcFlag & ETH_INT_RXE)
	{
		//c_conws("RX error!\r\n");
		//printf("E");

		// Check which BD has the error and clear it
		for (uint32_t i = 0; i < ETH_PKT_BUFFS; ++i) {
			LOG_TRACE ("Error clear.....\r\n");	
	
			if((eth_rx_bd[i].len_ctrl & ETH_RX_BD_EMPTY) == 0UL){
				if (eth_rx_bd[i].len_ctrl & (ETH_RX_BD_OVERRUN | ETH_RX_BD_INVSIMB | ETH_RX_BD_DRIBBLE |
											 ETH_RX_BD_TOOLONG | ETH_RX_BD_SHORT | ETH_RX_BD_CRCERR | ETH_RX_BD_LATECOL))
				{
					//At least one of the above error flags was set
					LOG_TRACE ("Slot %hu RX errorflags: 0x%08lx \n\r", i, eth_rx_bd[i].len_ctrl);
					
					//Clear error flags
					eth_rx_bd[i].len_ctrl &= ~(ETH_RX_BD_OVERRUN | ETH_RX_BD_INVSIMB | ETH_RX_BD_DRIBBLE |
																		 ETH_RX_BD_TOOLONG | ETH_RX_BD_SHORT | ETH_RX_BD_CRCERR | ETH_RX_BD_LATECOL);
					//mark the buffer as empty and free to use
					eth_rx_bd[i].len_ctrl |= ETH_RX_BD_EMPTY;
					++inErrors;	
				}
			}
		}
	}


	if (ethIntSrcFlag & ETH_INT_RXB) {
		LOG_TRACE("RX frame!\r\n");
		RxBufferSlotId = check_rx_buffers();

		while (RxBufferSlotId != -1) {
		  packetLenght=((eth_rx_bd[RxBufferSlotId].len_ctrl) >> 16);		
		  return packetLenght; 
		}
	}
	return packetLenght;
}

void retrievePacketData(uint8_t *localBuffer, const uint32_t length){

		// Check for received packets
		//LOG_TRACE("RX frame!\r\n");
		
		while (RxBufferSlotId != -1) {
			
			LOG_TRACE("R%02li 0x%08lx\n\r", RxBufferSlotId, eth_rx_bd[RxBufferSlotId].len_ctrl);
			
			const uint32_t len_longs = (length + 3UL) >> 2;
			uint32_t *src = (uint32_t*)eth_rx_bd[RxBufferSlotId].data_pnt; //source of packets
			uint32_t *dest = (uint32_t *)localBuffer; 
			
			if( ((uint32_t)localBuffer) % sizeof(uint16_t) !=0 ){
				LOG_TRACE("[] align localbuffer to word on allocation!\r\n");
			}

			delayMicrosec(1000);
// Dummy loop to wait for the MAC write FIFO to empty
// is it really needed? ???????
//			for (uint32_t j = 0; j < 1000; ++j) {
//				asm("nop;");
//			}

			// copy src->dst
			for(uint32_t i=0; i < len_longs; ++i){
				*dest++ = *src++;
			}
				
			// now mark the buffer as empty and free to use
			eth_rx_bd[RxBufferSlotId].len_ctrl |= ETH_RX_BD_EMPTY;
			RxBufferSlotId = check_rx_buffers();		
		}
}

void endPacketRetrieve(){
	//LOG_TRACE("endPacketRetrieve()..\n\r");
}

int32_t destroy(){

// close
// disable RX and TX
ETH_MODER = 0;

//disable RX and RX error int sources BEFORE removing I6 handler
ETH_INT_MASK = 0;

ethIntSrcFlag=0;
RxBufferSlotId=0;
inErrors=0;
outErrors=0;

if(packets_base!=0) {
  uint32_t ret = vmalloc(VMALLOC_MODE_FREE,packets_base);
  packets_base = 0;
}

 LOG_TRACE("SVEthLANa network adapter destroyed. ");
 return 0;
}

//Returns 0 if there is a new packet received in slot 0, 1 if in slot 1 and -1 otherwise
const int32_t check_rx_buffers(){
	int32_t  retval = -1L;
	
	//We check only one slot, which is the one not checked the last time
	//we were here. We only move to the other slot if we found a packet in the current slot.
	const uint32_t tmp1 = eth_rx_bd[cur_rx_slot].len_ctrl;
	
	if ((tmp1 & ETH_RX_BD_EMPTY) == 0UL) {
		retval = (int32_t)cur_rx_slot;
		cur_rx_slot = (cur_rx_slot + 1) & (ETH_PKT_BUFFS-1);
	} else {
		//No packet in the expected slot
		//Try again
		const uint32_t tmp2 = eth_rx_bd[cur_rx_slot].len_ctrl;

		if ((tmp2 & ETH_RX_BD_EMPTY) == 0UL){
			LOG_TRACE("Slot %lu RX retried read: 0x%08lx then 0x%08lx\n\r", cur_rx_slot, tmp1, tmp2);
			retval = (int32_t)cur_rx_slot;
			cur_rx_slot = (cur_rx_slot + 1) & (ETH_PKT_BUFFS-1);
		}
	}

	return retval;
}

// This function sends a packet.
// Assumes that a TX interrupt is not missed while the TXIE is disabled (check this in the MAC controller docs).

static int32_t send_packet (uint32_t *datapnt, const uint32_t bufferLength, const uint32_t slot, const uint32_t last_packet){
	
	const uint32_t origlen = bufferLength;
	uint32_t rounded_len = (bufferLength + 3) & 0xFFFC;;

	//If the packet is greater than 1536 we return error
	if (rounded_len > 1536UL) {
		//we have no intention to send this packet, so just free it
		return -2;
	}

	//Check the wanted TX BD slot if it's free
	if (((uint32_t)(eth_tx_bd[slot].len_ctrl & ETH_TX_BD_READY)) != 0UL)
		return -1;

	//Divide rounded len by 4
	rounded_len = rounded_len >> 2;

	//Write packet data
//	eth_dst_pnt = &ETH_DATA_BASE_PNT[slot * (1536/4)];

	volatile uint32_t *eth_dst_pnt = (uint32_t*)(eth_tx_bd[slot].data_pnt);;
	
	for(uint32_t i=0; i < rounded_len; ++i) {
		
		if (eth_dst_pnt == 0UL){
			LOG_TRACE("Send_packet: eth_dst_pnt is 0\r\n");
			//Bconin(2);
		}
		if (datapnt == 0UL){
			LOG_TRACE("Send_packet: datapnt is 0\r\n");
			//Bconin(2);
		}

		*eth_dst_pnt++ = *datapnt++;
	}	


	if ((((uint32_t)eth_dst_pnt) > (ETH_DATA_BASE_ADDR + (1536*2))) ||
		(((uint32_t)eth_dst_pnt) < ETH_DATA_BASE_ADDR))
	{
		LOG_TRACE("Send_packet: eth_dst_pnt outside tx slots!\r\n");
	}
	

	delayMicrosec(1000);

	//Dummy loop to wait for the CT60 write FIFO to empty
	//for (uint32_t j = 0; j < 1000; ++j) {
//		asm("nop;");
//	}
	
	//Write length of data, BD ready, BD CRC enable
	//We set the wrap bit if this packet is the last one in a sequence (may be just this packet too).
	if(last_packet)
		eth_tx_bd[slot].len_ctrl = (uint32_t)(origlen << 16) | ETH_TX_BD_READY | ETH_TX_BD_IRQ | ETH_TX_BD_CRC | ETH_TX_BD_WRAP;
	else
		eth_tx_bd[slot].len_ctrl = (uint32_t)(origlen << 16) | ETH_TX_BD_READY | ETH_TX_BD_IRQ | ETH_TX_BD_CRC;
	
	return 0L;
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
