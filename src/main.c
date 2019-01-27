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
 * $Id: main.c,v 1.16 2006/06/11 21:55:03 adam Exp $
 *
 */

#include "logging.h"

#include "uip.h"
#include "uip_arp.h"
#include "uip-split.h"
#include "rtl8019dev.h"

#include "timer.h"
#include "dhcpc.h"

#include <osbind.h>
#include <stdio.h>

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

#ifndef NULL
#define NULL (void *)0
#endif /* NULL */

struct ProfileProbe {
  uint64_t last;
  uint64_t all;

};

uint64_t getMicroseconds( )
{
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

void initProbe ( struct ProfileProbe* p ) 
{
  p->last = 0;
  p->all = 0;
}

void probeBegin ( struct ProfileProbe* p ) 
{
  p->last = getMicroseconds( );
}

void probeEnd ( struct ProfileProbe* p ) 
{
  p->all +=  getMicroseconds( ) - p->last;
}

void probePrint ( struct ProfileProbe* p  )
{
  //printf("%llu\r\n",p->all );
}

struct ProfileProbe netSend;
struct ProfileProbe netRecv;
struct ProfileProbe netAll;
struct ProfileProbe netInput;
struct ProfileProbe netOther;

void net_send()
{
  uip_split_output();
  //tcpip_output();
}

void tcpip_output()
{
  probeBegin(&netSend);
  uip_arp_out();
  RTL8019dev_send();
  probeEnd(&netSend);
}

/*---------------------------------------------------------------------------*/
int
main(void)
{
  int i;
  uip_ipaddr_t ipaddr;
  struct timer periodic_timer, arp_timer;
  
  (void)printf("uIP tool, version %d\r\n", VERSION);

  Super(0);

  timer_set(&periodic_timer, CLOCK_SECOND/10);
  timer_set(&arp_timer, CLOCK_SECOND * 10);
  (void)Cconws("RTL8019 init .. ");
  if ( !RTL8019dev_init(uip_ethaddr.addr) ) {
    (void)Cconws("failed!\r\n");
    return 1;
  }

  uip_init();
  dhcpc_init(uip_ethaddr.addr, 6);
  atarid_init();

  initProbe( &netSend);
  initProbe( &netRecv);
  initProbe( &netAll);
  initProbe( &netInput);
  initProbe( &netOther);

  while( -1 == Cconis() ) Cconin ();

  while( -1 != Cconis() ) {
    probeBegin(&netAll);

    probeBegin(&netRecv);
    uip_len = RTL8019dev_poll();
    probeEnd(&netRecv);
    
    if(uip_len > 0) {
      probeBegin(&netInput);
      if(BUF->type == htons(UIP_ETHTYPE_IP)) {
        uip_arp_ipin();
        uip_input();
        if(uip_len > 0) {
          //uip_arp_out();
          net_send();
        }
      } else if(BUF->type == htons(UIP_ETHTYPE_ARP)) {
        uip_arp_arpin();
        if(uip_len > 0) {
          RTL8019dev_send();
          //net_send();
        }
      }
      probeEnd(&netInput);

    } else if(timer_expired(&periodic_timer)) {
      probeBegin(&netOther);
      timer_reset(&periodic_timer);
      for(i = 0; i < UIP_CONNS; i++) {
        uip_periodic(i);
        if(uip_len > 0) {
         // uip_arp_out();
          net_send();
        }
      }
    }

    for(i = 0; i < UIP_CONNS; i++) {
      /* Call apps idle handlers */
      uip_idle(i);
    }

    #if UIP_UDP
    for(i = 0; i < UIP_UDP_CONNS; i++) {
      uip_udp_periodic(i);
      if(uip_len > 0) {
        //uip_arp_out();
        net_send();
      }
    }
    #endif /* UIP_UDP */
      
    /* Call the ARP timer function every 10 seconds. */
    if(timer_expired(&arp_timer)) {
      timer_reset(&arp_timer);
      uip_arp_timer();
    }

    probeEnd(&netOther);
    probeEnd(&netAll);
  }

  #if 0
  LOG("\r\n\r\n");
  LOG("All:     "); probePrint(&netAll);
  LOG("Input:   "); probePrint(&netInput);
  LOG("Other:   "); probePrint(&netOther);
  LOG("DevSend: "); probePrint(&netSend);
  LOG("DevRecv: "); probePrint(&netRecv);
  #endif

  return 0;
}
/*---------------------------------------------------------------------------*/
void
uip_log(char *m)
{
  LOG("uIP: %s\n", m);
}

#ifdef __DHCPC_H__
void
dhcpc_configured(const struct dhcpc_state *s)
{
  uip_sethostaddr(s->ipaddr);
  uip_setnetmask(s->netmask);
  uip_setdraddr(s->default_router);
//  resolv_conf(s->dnsaddr);
}
#endif /* __DHCPC_H__ */

/*---------------------------------------------------------------------------*/
