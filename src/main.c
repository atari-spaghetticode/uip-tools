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
#include <string.h>

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

#ifndef NULL
#define NULL (void *)0
#endif /* NULL */

struct ProfileProbe {
  uint64_t last;
  uint64_t all;
};

uint64_t getMicroseconds()
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

void initProbe (struct ProfileProbe* p)
{
  p->last = 0;
  p->all = 0;
}

void probeBegin (struct ProfileProbe* p)
{
  p->last = getMicroseconds( );
}

void probeEnd (struct ProfileProbe* p)
{
  p->all +=  getMicroseconds( ) - p->last;
}

void probePrint (struct ProfileProbe* p)
{
  //printf("%llu\r\n",p->all );
}

struct ProfileProbe netSend;
struct ProfileProbe netRecv;
struct ProfileProbe netAll;
struct ProfileProbe netInput;
struct ProfileProbe netOther;

/*---------------------------------------------------------------------------*/

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

void
uip_log(char *m)
{
  LOG_TRACE("uIP: %s\n", m);
}

/*---------------------------------------------------------------------------*/

void
uip_configure_ip(
  const uip_ipaddr_t ipaddr,
  const uip_ipaddr_t netmask,
  const uip_ipaddr_t default_router)
{
  LOG("%d.%d.%d.%d\r\n",
	  uip_ipaddr1(ipaddr), uip_ipaddr2(ipaddr),
	  uip_ipaddr3(ipaddr), uip_ipaddr4(ipaddr));

  uip_sethostaddr(ipaddr);
  uip_setnetmask(netmask);
  uip_setdraddr(default_router);
}

void
dhcpc_configured(const struct dhcpc_state *s)
{
  uip_configure_ip(s->ipaddr, s->netmask, s->default_router);
  if ( s->hostname[0] != 0 ) {
    LOG("DHCP Hostname: %s\r\n", s->hostname);
  }

  /*  resolv_conf(s->dnsaddr); */
}

void configure_ip();

/*---------------------------------------------------------------------------*/

uip_ipaddr_t config_ip;
uip_ipaddr_t config_netmask;
uip_ipaddr_t config_router;
bool config_static_ip;
char config_path[256];

void
toggle_ip_config()
{
  config_static_ip=!config_static_ip;
  save_config();
  configure_ip();
}

void
create_config_path(const char* app_path)
{
  char* file_ext = NULL;
  strncpy(config_path, app_path, sizeof(config_path));
  file_ext = strrchr(config_path, '.');
  if (file_ext) {
    *file_ext = 0;
    strcat(config_path, ".cnf");
    LOG("config path: %s\r\n", config_path);
  } else {
    config_path[0] = 0;
  }
}

void
config_setup_default()
{
  config_static_ip = false;
  uip_ipaddr(&config_ip, 192,168,1,90);
  uip_ipaddr(&config_netmask, 255,255,255,0);
  uip_ipaddr(&config_router, 192,168,1,254);
}

void
save_config()
{
  FILE* fp = fopen (config_path, "w");
  fprintf(fp,
    "%d " /* dhcp enabled? */
    "%d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d",
    config_static_ip ? 1 : 0,
    uip_ipaddr1(config_ip), uip_ipaddr2(config_ip),
	  uip_ipaddr3(config_ip), uip_ipaddr4(config_ip),
    uip_ipaddr1(config_netmask), uip_ipaddr2(config_netmask),
	  uip_ipaddr3(config_netmask), uip_ipaddr4(config_netmask),
    uip_ipaddr1(config_router), uip_ipaddr2(config_router),
	  uip_ipaddr3(config_router), uip_ipaddr4(config_router));
  fclose(fp);
}

void
read_config()
{
  FILE* fp = fopen (config_path, "r");
  size_t size = 0;
  char* config_data = NULL;

  if (fp) {
    fseek (fp, 0L, SEEK_END);
    size = ftell (fp);
    fseek (fp, 0L, SEEK_SET);
  }

  if (size == 0) {
    fclose (fp);
    config_setup_default ();
    save_config ();
    return;
  }

  config_data = malloc (size);

  if (!config_data) {
    LOG ("2 Couldn't open/create the config file: %s\r\n", config_path);
    return;
  }

  fread (config_data, size, 1, fp);
  fclose (fp);

  {
    int ip[4], mask[4], route[4];
    int static_ip_enabled = 0;

    size_t num_values = sscanf(config_data,
      "%u " /* dhcp enabled? */
      "%u.%u.%u.%u %u.%u.%u.%u %u.%u.%u.%u",
      &static_ip_enabled,
      &ip[0], &ip[1], &ip[2], &ip[3],
      &mask[0], &mask[1], &mask[2], &mask[3],
      &route[0], &route[1], &route[2], &route[3]);

    if(num_values != 13) {
      LOG("Configuration file malformed! %d\r\n", num_values);
      config_setup_default ();
    }

    config_static_ip = static_ip_enabled == 1 ? true : false;
    uip_ipaddr(&config_ip, ip[0], ip[1], ip[2], ip[3]);
    uip_ipaddr(&config_netmask, mask[0], mask[1], mask[2], mask[3]);
    uip_ipaddr(&config_router, route[0], route[1], route[2], route[3]);

    configure_ip();
  }

  free (config_data);

}

/*---------------------------------------------------------------------------*/

void
configure_ip()
{
  if (!config_static_ip) {
    LOG("DHCP IP: ");
    dhcpc_init(uip_ethaddr.addr, 6);
  } else {
    dhcp_stop();
    LOG("STATIC IP: ");
    uip_configure_ip (config_ip, config_netmask, config_router);
  }
}

/*---------------------------------------------------------------------------*/
/* Note: Cookiejar code base on toshyp.atari.org source
*/

typedef struct
{
    uint32_t id;             /* Identification code */
    uint32_t value;          /* Value of the cookie */
} CookieJar;

bool get_cookie(uint32_t cookie, void *value )
{
  CookieJar *cookiejar;
  uint32_t    val = -1l;
  uint16_t    i=0;

  /* Get pointer to cookie jar */
  cookiejar = (CookieJar *)(Setexc(0x05A0/4,(const void (*)(void))-1));
  if (cookiejar) {
    for (i=0 ; cookiejar[i].id ; i++) {
      #if 0
      const char* cookie_id_bytes = (const char*)&cookiejar[i].id;
      LOG("Cookie: %c%c%c%c\r\n",
        cookie_id_bytes[0], cookie_id_bytes[1],
        cookie_id_bytes[2], cookie_id_bytes[3]);
      #endif
      if (cookiejar[i].id==cookie) {
        if (value)
          *(uint32_t *)value = cookiejar[i].value;
        return true;
      }
    }
  }

  return false;
}

bool
check_cookiejar()
{
  if(get_cookie('MiNT', NULL)) {
    LOG("uiptool doesn't work under MiNT, sorry!\r\n");
    return false;
  }

  if(get_cookie('STiK', NULL)) {
    LOG("uiptool doesn't work with STiK, sorry!\r\n");
    return false;
  }

  return true;
}

/*---------------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
  int i;
  uip_ipaddr_t ipaddr;
  struct timer periodic_timer, arp_timer;
  
  LOG("uIP tool, version %d\r\n", VERSION);

  create_config_path(argv[0]);

  Super(0);
  if (!check_cookiejar()) {
    return 1;
  }

  timer_set(&periodic_timer, CLOCK_SECOND/10);
  timer_set(&arp_timer, CLOCK_SECOND * 10);
  LOG("RTL8019 init ... ");
  if (!RTL8019dev_init(uip_ethaddr.addr) ) {
    LOG("driver initialisation failed!\r\n");
    return 1;
  }

  uip_init();
  read_config();
  atarid_init();

  initProbe(&netSend);
  initProbe(&netRecv);
  initProbe(&netAll);
  initProbe(&netInput);
  initProbe(&netOther);

  while( -1 == Cconis() ) Cconin ();

  while(1) {

    if( -1 == Cconis() ) {
      uint32_t code = Cconin ();
      /* Check if F1 was pressed  */
      if(code == 0x3b0000) {
        toggle_ip_config();
      } else {
        break;
      }
    }

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
    } else {
      for(i = 0; i < UIP_CONNS; i++) {
        /* Call apps idle handlers */
        uip_idle(i);
      }
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
