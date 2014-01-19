/*
 * Copyright (c) 2001, Adam Dunkels.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Adam Dunkels.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.  
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: uip_arch.c,v 1.3 2005/02/24 22:04:19 oliverschmidt Exp $
 *
 */


#include "uip.h"
#include "uip_arch.h"

#define BUF ((struct uip_tcpip_hdr *)&uip_buf[UIP_LLH_LEN])
#define IP_PROTO_TCP    6

/*-----------------------------------------------------------------------------------*/
void
uip_add_rcv_nxt(u16_t n)
{
  uip_conn->rcv_nxt[3] += (n & 0xff);
  uip_conn->rcv_nxt[2] += (n >> 8);

  if(uip_conn->rcv_nxt[2] < (n >> 8)) {
    ++uip_conn->rcv_nxt[1];    
    if(uip_conn->rcv_nxt[1] == 0) {
      ++uip_conn->rcv_nxt[0];
    }
  }
  
  
  if(uip_conn->rcv_nxt[3] < (n & 0xff)) {
    ++uip_conn->rcv_nxt[2];  
    if(uip_conn->rcv_nxt[2] == 0) {
      ++uip_conn->rcv_nxt[1];    
      if(uip_conn->rcv_nxt[1] == 0) {
	++uip_conn->rcv_nxt[0];
      }
    }
  }
}
/*-----------------------------------------------------------------------------------*/
void
uip_add32(u8_t *op32, u16_t op16)
{
  
  uip_acc32[3] = op32[3] + (op16 & 0xff);
  uip_acc32[2] = op32[2] + (op16 >> 8);
  uip_acc32[1] = op32[1];
  uip_acc32[0] = op32[0];
  
  if(uip_acc32[2] < (op16 >> 8)) {
    ++uip_acc32[1];    
    if(uip_acc32[1] == 0) {
      ++uip_acc32[0];
    }
  }
  
  
  if(uip_acc32[3] < (op16 & 0xff)) {
    ++uip_acc32[2];  
    if(uip_acc32[2] == 0) {
      ++uip_acc32[1];    
      if(uip_acc32[1] == 0) {
	++uip_acc32[0];
      }
    }
  }
}
/*-----------------------------------------------------------------------------------*/

static u16_t
chksum(u16_t *sdata, u16_t len)
{ 
  u16_t acc,asm_acc;

  asm volatile
  (   
      "     move.l %1,a0        \n"
      "     move.w %2,d0        \n"
      "     move.w d0,d1        \n"
      "     move #0,ccr         \n"

      "     and.w #0xf,d1       \n"
      "     moveq #0,d6         \n"
      "     lsr.w #4,d0         \n"
      "     subq.w #1,d0        \n"
      "     bmi.b 2f            \n"
      "1:                       \n"
      "     movem.l (a0)+,d2-d5 \n"
      "     addx.l d2,d6        \n"
      "     addx.l d3,d6        \n"
      "     addx.l d4,d6        \n"
      "     addx.l d5,d6        \n"
      "     dbf d0,1b           \n"

      "     moveq #0,d3         \n"
      "     move.l d6,d2        \n"
      "     swap d2             \n"
      "     addx.w d2,d6        \n"
      "     addx.w d3,d6        \n"
      "2:                       \n"
      "     move.w d1,d0        \n"
      "     lsr.w #1,d0         \n"
      "     subq.w #1,d0        \n"
      "     bmi.b 4f            \n"
      "3:                       \n"  
      "     move.w (a0)+,d2     \n"
      "     addx.w d2,d6        \n"
      "     dbf d0,3b           \n"
      "     moveq #0,d3         \n"
      "     addx.w d3,d6        \n"
      "4:                       \n"  
      "     moveq #0,d3         \n"
      "     btst #0,d1          \n"
      "     beq.b 5f            \n"
      "     move.b (a0)+,d3     \n"
      "     lsl.w  #8,d3        \n"
      "     addx.w d3,d6        \n"
      "5:                       \n"
      "     move.w  d6,%0       \n"
      
     :    "=r" ( asm_acc )
     :    "g"( sdata ),
          "g"( len )
     :    "a0","d0","d1","d2","d3","d4","d5","d6"
  );

  return asm_acc;
}
/*-----------------------------------------------------------------------------------*/
u16_t
uip_ipchksum(void)
{
  return chksum((u16_t *)&uip_buf[UIP_LLH_LEN], UIP_IPH_LEN);
}
/*-----------------------------------------------------------------------------------*/
u16_t
uip_tcpchksum(void)
{
  u16_t hsum, sum;

  
  /* Compute the checksum of the TCP header. */
  hsum = uip_chksum((u16_t *)&uip_buf[UIP_LLH_LEN + UIP_IPH_LEN], UIP_TCPH_LEN);

  /* Compute the checksum of the data in the TCP packet and add it to
     the TCP header checksum. */
  sum = uip_chksum((u16_t *)uip_appdata,
		   (u16_t)(((((u16_t)(BUF->len[0]) << 8) + BUF->len[1]) -
		   UIP_IPTCPH_LEN)));

  if((sum += hsum) < hsum) {
    ++sum;
  }
  
  if((sum += BUF->srcipaddr[0]) < BUF->srcipaddr[0]) {
    ++sum;
  }
  if((sum += BUF->srcipaddr[1]) < BUF->srcipaddr[1]) {
    ++sum;
  }
  if((sum += BUF->destipaddr[0]) < BUF->destipaddr[0]) {
    ++sum;
  }
  if((sum += BUF->destipaddr[1]) < BUF->destipaddr[1]) {
    ++sum;
  }
  if((sum += (u16_t)htons((u16_t)IP_PROTO_TCP)) < (u16_t)htons((u16_t)IP_PROTO_TCP)) {
    ++sum;
  }

  hsum = (u16_t)HTONS((((u16_t)(BUF->len[0]) << 8) + BUF->len[1]) - UIP_IPH_LEN);
  
  if((sum += hsum) < hsum) {
    ++sum;
  }
  
  return sum;
}
/*-----------------------------------------------------------------------------------*/
