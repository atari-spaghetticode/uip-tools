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
#include "common.h"

#include "uip.h"
#include "uip_arp.h"
#include "uip-split.h"
#include "drivers/rtl8019.h"

#include "timer.h"
#include "dhcpc.h"

#include <osbind.h>
#include <stdio.h>
#include <string.h>

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

#ifndef NULL
#define NULL ((void *)0)
#endif /* NULL */

#define KEY_CHECK_VALUE (20)

/*---------------------------------------------------------------------------*/

void net_send()
{
  uip_split_output();
}

void ip_packet_output()
{
  uip_arp_out();
  RTL8019dev_send();
}

/*---------------------------------------------------------------------------*/

void
uip_log(char *m)
{
  INFO("uIP: %s\n", m);
}

/*---------------------------------------------------------------------------*/

void
uip_configure_ip(
  const uip_ipaddr_t ipaddr,
  const uip_ipaddr_t netmask,
  const uip_ipaddr_t default_router)
{
  INFO("\33p%d.%d.%d.%d\33q\r\n",
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
    INFO("DHCP Hostname: %s\r\n", s->hostname);
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

void save_config();

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
    strcat(config_path, ".cfg");
  } else {
    /* if argc[0] is empty store config on c: */
    uint32_t drv_map = Drvmap();
    if (drv_map&0x4) {
      strcat(config_path, "c:\\uip.cfg");
    } else if (drv_map&0x1) {
      strcat(config_path, "a:\\uip.cfg");
    } else {
      config_path[0] = 0;
    }
  }
  LOG("config path: %s\r\n", config_path);
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
    configure_ip ();
    save_config ();
    return;
  }

  config_data = malloc (size);

  if (!config_data) {
    LOG_WARN ("Couldn't open/create the config file: %s\r\n", config_path);
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
      LOG_WARN("Configuration file malformed! %d\r\n", num_values);
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
    INFO("DHCP IP: ");
    dhcpc_init(uip_ethaddr.addr, 6);
  } else {
    dhcp_stop();
    INFO("STATIC IP: ");
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

bool get_cookie(uint32_t cookie, uint32_t *value)
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
          *value = cookiejar[i].value;
        return true;
      }
    }
  }

  return false;
}

/*---------------------------------------------------------------------------*/

static void
config_cpu_options(int cpu_type)
{
  /* Tune the stack based on the cpu type.
   * These values were chosen experimentally.
   */
  if (cpu_type == 60) {
    /* For 68060 smaller window size yields better overall throughput */
    uip_receive_window = UIP_CONF_BUFFER_SIZE;
  } else {
    /* For slower CPUs larger window size yields better overall throughput */
    uip_receive_window = UIP_CONF_BUFFER_SIZE * 2;
  }
}

/*---------------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
  uip_ipaddr_t ipaddr;
  struct timer periodic_timer, arp_timer;
  uint32_t cpu_type = 0;  // assume MC68000
  int key_check_counter = KEY_CHECK_VALUE;

  INFO("\33puIP tool, version %d\33q\r\n", VERSION);

  create_config_path(argv[0]);

  Super(0);

  if(get_cookie('MiNT', NULL)) {
    LOG_WARN("uiptool doesn't work under MiNT, sorry!\r\n");
    return 1;
  }

  if(get_cookie('STiK', NULL)) {
    LOG_WARN("uiptool doesn't work with STiK, sorry!\r\n");
    return 1;
  }

  get_cookie('_CPU', &cpu_type);
  config_cpu_options(cpu_type);

  timer_set(&periodic_timer, CLOCK_SECOND/10);
  timer_set(&arp_timer, CLOCK_SECOND * 10);

  INFO("RTL8019 init ... ");
  if (!RTL8019dev_init(uip_ethaddr.addr, cpu_type) ) {
    LOG_WARN("driver initialisation failed!\r\n");
    return 1;
  }

  uip_init();
  read_config();
  httpd_init();
  ftpd_init();

  while( -1 == Cconis() ) Cconin ();

  while(1) {

    if (0 == --key_check_counter) {
      if( -1 == Cconis() ) {
        uint32_t code = Cconin ();
        /* Check if F1 was pressed  */
        if(code == 0x3b0000) {
          toggle_ip_config();
        } else {
          break;
        }
      }
      key_check_counter = KEY_CHECK_VALUE;
    }

    uip_len = RTL8019dev_poll();

    if(uip_len > 0) {
      if(BUF->type == htons(UIP_ETHTYPE_IP)) {
        uip_arp_ipin();
        uip_input();
        if(uip_len > 0) {
          net_send();
        }
      } else if(BUF->type == htons(UIP_ETHTYPE_ARP)) {
        uip_arp_arpin();
        if(uip_len > 0) {
          RTL8019dev_send();
        }
      }

    } else if(timer_expired(&periodic_timer)) {
      timer_reset(&periodic_timer);
      for(int i = 0; i < UIP_CONNS; i++) {
        uip_periodic(i);
        if(uip_len > 0) {
          net_send();
        }
      }
    } else {
      for(int i = 0; i < UIP_CONNS; i++) {
        uip_poll_conn(&uip_conns[i]);
        if(uip_len > 0) {
          net_send();
        }
      }
    }

    #if UIP_UDP
    for(int i = 0; i < UIP_UDP_CONNS; i++) {
      uip_udp_periodic(i);
      if(uip_len > 0) {
        ip_packet_output();
      }
    }
    #endif /* UIP_UDP */

    /* Call the ARP timer function every 10 seconds. */
    if(timer_expired(&arp_timer)) {
      timer_reset(&arp_timer);
      uip_arp_timer();
    }
  }

  return 0;
}
/*---------------------------------------------------------------------------*/

void udp_appcall (void)
{
  switch(uip_udp_conn->lport) {
    case HTONS(68):
      dhcpc_appcall();
      break;
    case HTONS(33334):
    case HTONS(33332):
      ioredirect_appcall();
      break;
    default:
      /* pass */;
  }
}

/*---------------------------------------------------------------------------*/

void uip_appcall ()
{
  switch(uip_conn->lport) {
    case HTONS(21): ftpd_appcall();       break;
    case HTONS(80): httpd_appcall();      break;
    default: ftpd_data_appcall();         break;
  }
}
