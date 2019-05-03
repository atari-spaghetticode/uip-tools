
#include "svethlana.h"
#include "svregs.h"
#include "logging.h"

static char message[100];

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
static char* packets_base;

/* helper functions */
void Delay_microsec(long delay);

/* Convert single hex char to short */
short ch2i(char);

/* Convert long to hex ascii */
void hex2ascii(uint32_t l, uint8_t* c);


void beginPacketSend(const unsigned int packetLength){

}


void sendPacketData(unsigned char * localBuffer, const unsigned int length){

}


void endPacketSend(){

}


int32_t init(uint8_t* macaddr, const uint32_t cpu_type){
 return 0;
}


void processInterrupt(void){

}

unsigned int beginPacketRetrieve(void){
	return 0;
}


void retrievePacketData(unsigned char *localBuffer, const unsigned int length){

}

void endPacketRetrieve(void){

}

signed int destroy(void){
 return 0;
}

/* Convert one ASCII char to a hex nibble */
short ch2i(char c){
	if(c >= '0' && c <= '9')
		return (short)(c - '0');
	if(c >= 'A' && c <= 'F')
		return (short)(c - 'A' + 10);
	if(c >= 'a' && c <= 'f')
		return (short)(c - 'a' + 10);
	return 0;
}

// Convert an unsigned long to ASCII
void hex2ascii(uint32_t l, uint8_t* c){
	uint16_t i;
	uint8_t t;
	
	for(i=7; i!=0; i--)
	{
		t = (uint8_t)(l & 0xf);				// Keep only lower nibble
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

void Delay_microsec(long delay){
    uint64_t future = getMicroseconds() + delay;
    while(future > getMicroseconds());
}
