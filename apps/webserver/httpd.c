/**
 * \addtogroup apps
 * @{
 */

/**
 * \defgroup httpd Web server
 * @{
 * The uIP web server is a very simplistic implementation of an HTTP
 * server. It can serve web pages and files from a read-only ROM
 * filesystem, and provides a very small scripting language.

 */

/**
 * \file
 *         Web server
 * \author
 *         Adam Dunkels <adam@sics.se>
 */


/*
 * Copyright (c) 2004, Adam Dunkels.
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: httpd.c,v 1.2 2006/06/11 21:46:38 adam Exp $
 */

#include "uip.h"
#include "httpd.h"
#include "httpd-fs.h"
#include "httpd-cgi.h"
#include "http-strings.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <osbind.h>

#define STATE_WAITING 0
#define STATE_OUTPUT  1

#define ISO_nl      0x0a
#define ISO_space   0x20
#define ISO_bang    0x21
#define ISO_percent 0x25
#define ISO_period  0x2e
#define ISO_slash   0x2f
#define ISO_colon   0x3a

/*---------------------------------------------------------------------------*/
void mkdir_for_file( const char* path )
{
  char temp_path[256];
  size_t len = strlen( path );
  strncpy( temp_path, path, sizeof(temp_path));

  // remove file name from the path file path base 
  for(size_t i = len; i != 0; --i) {
    if ( temp_path[i] == '\\' ) {
      temp_path[i] = '\0';
      break;
    }
  }
  
  // skip the drive letter in the path
  for ( size_t i = 4; i < len; ++i ) {
    if ( temp_path[i] == '\\' ) {
      temp_path[i] = '\0';
      Dcreate ( temp_path );
      temp_path[i] = '\\';
    }
  }
  Dcreate ( temp_path );

}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(receive_file(struct httpd_state *s, const char* filename, const size_t filelen))
{
  PT_BEGIN(&s->worker2);

  Cconws("recv: ");
  Cconws(filename);

  // make sure folder exists
  mkdir_for_file ( filename );

  s->file = fopen( filename,"wb" );

  if ( !s->file ) {
    PSOCK_SEND_STR2(&s->worker, &s->sin, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n");    
    Cconws(" -> failed to open!\r\n");
    PT_END(&s->worker);
  }

  s->temp_file_len = filelen;

  while ( s->temp_file_len > 0) {

    PSOCK_READBUF_LEN2(&s->worker, &s->sin, 
      s->temp_file_len > s->inputbuf_size ? 
        s->inputbuf_size : s->temp_file_len );

    fwrite ( s->inputbuf,PSOCK_DATALEN(&s->sin), 1, s->file );
    s->temp_file_len-=PSOCK_DATALEN(&s->sin);
  }

  Cconws(" -> OK.\r\n");
  fclose( s->file )

  PT_END(&s->worker2);
}
/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_post(struct httpd_state *s))
{
  PT_BEGIN(&s->worker);

  if ( s->expected_file_length == -1 ) {
    PSOCK_SEND_STR2(&s->worker, &s->sin, "HTTP/1.1 411 Length Required\r\nConnection: close\r\n");        
    PT_END(&s->worker);
  }

  Cconws("recv: ");
  Cconws(s->filename);

  // make sure folder exists
  mkdir_for_file ( s->filename );

  s->file = fopen( s->filename,"wb" );

  if ( !s->file ) {
    PSOCK_SEND_STR2(&s->worker, &s->sin, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n");    
    Cconws(" -> failed to open!\r\n");
    PT_END(&s->worker);
  } 

  while ( s->expected_file_length > 0) {

    PSOCK_READBUF_LEN2(&s->worker, &s->sin, 
      s->expected_file_length > s->inputbuf_size ? 
        s->inputbuf_size : s->expected_file_length );

    fwrite ( s->inputbuf,PSOCK_DATALEN(&s->sin), 1, s->file );
    s->expected_file_length-=PSOCK_DATALEN(&s->sin);
  }

  Cconws(" -> OK.\r\n");
  fclose( s->file );
  
  PSOCK_SEND_STR2(&s->worker, &s->sin, "HTTP/1.1 201 Created\r\nConnection: close\r\n");    

  PT_END(&s->worker);
}

static
PT_THREAD(handle_run(struct httpd_state *s))
{
  PT_BEGIN(&s->worker);
  PSOCK_SEND_STR2(&s->worker, &s->sin, "HTTP/1.1 200 OK\r\nConnection: close\r\n");
  Dsetpath(s->path);
  Pexec(0,s->filename,"","");
  PT_END(&s->worker);
}

void parse_file_path ( struct httpd_state *s )
{
  char* fn_end = s->inputbuf;
  char* fn;

  while (*fn_end++ != '/' );
  
  fn = fn_end;
  
  while (*fn_end != ' ' ) 
    fn_end++;

  *fn_end = 0;

  while ( fn != fn_end-- ) {
    if ( *fn_end == '/' ) {
      *fn_end = '\\';
    }
  }

  s->filename[0] = fn[0]&0x7f;
  s->filename[1] = ':';
  strncpy( &s->filename[2],++fn, sizeof(s->filename));
}

void parse_post( struct httpd_state *s )
{
  parse_file_path(s);
  s->handler_func = handle_post;
}

void parse_run( struct httpd_state *s )
{
  parse_file_path(s);
  s->handler_func = handle_run;
}

void parse_content_len( struct httpd_state *s )
{
  s->expected_file_length = atoi( &s->inputbuf[15] );
}

void parse_expect( struct httpd_state *s )
{
  s->expected_100_continue = 1;
}

void parse_multipart( struct httpd_state *s )
{
}

void parse_urlencoded( struct httpd_state *s )
{
}

#define HeaderEntry(str,func) {str, sizeof(str), func}

struct {
  const char* entry;
  size_t entry_len;
  char(*parse_func)(struct httpd_state *s);
} commands[] = {
  HeaderEntry ( "POST", parse_post ),
  HeaderEntry ( "GET", parse_post ),
  HeaderEntry ( "RUN", parse_run ),
  HeaderEntry ( "DELETE", parse_run ),
  HeaderEntry ( "Content-Length:", parse_content_len ),
  HeaderEntry ( "Expect: 100-continue", parse_expect ),
  HeaderEntry ( "Content-Type: multipart/form-data;", parse_multipart ),
  HeaderEntry ( "Content-Type: application/x-www-form-urlencoded", parse_urlencoded ),
};

static
PT_THREAD(handle_input(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sin);
  s->expected_100_continue = 0;
  s->expected_file_length = -1;
  s->handler_func = NULL;
  s->filename[0] = '\0';

  while(1) {
    // eat away the header
    PSOCK_READTO(&s->sin, ISO_nl);

    if ( s->inputbuf[0] == '\r' ) {
      //got header, now get the data
      break;
    }

    for ( size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i ) {
      if ( 0 == memcmp(s->inputbuf,commands[i].entry,commands[i].entry_len - 1 ) ) {
        commands[i].parse_func(s);
        break;
      }
    }
  }

  if ( s->expected_100_continue ) {
    PSOCK_SEND_STR2(&s->worker, &s->sin, http_continue);
  }

  if ( s->handler_func ) {
    PT_INIT(&s->worker);
    PSOCK_WAIT_THREAD(&s->sin, s->handler_func(s) );
  }

  PSOCK_CLOSE_EXIT(&s->sin);
  PSOCK_END(&s->sin);
}
/*---------------------------------------------------------------------------*/
static void
handle_connection(struct httpd_state *s)
{
  handle_input(s);
}
/*---------------------------------------------------------------------------*/
void
httpd_appcall(void)
{
  struct httpd_state *s = (struct httpd_state *)&(uip_conn->appstate);
  if ( uip_timedout() ) {
    printf("timedout\r\n");
  } else if( uip_aborted() ) {
    printf("aborted\r\n");
  } else if(uip_closed()  ) {
  //  printf("closed\r\n");
  } else if(uip_connected()) {
//    printf("ptr %p\r\n", s);
    s->inputbuf_size = INPUTBUF_SIZE;
    PSOCK_INIT(&s->sin, s->inputbuf, s->inputbuf_size);
    s->timer = 0;
    handle_connection(s);
  } else if(s != NULL) {
    if(uip_poll()) {
      ++s->timer;
      // if(s->timer >= 20) {
       //   uip_abort();
      // }
    } else {
      s->timer = 0;
    }
    handle_connection(s);
  } else {
    printf("abort\r\n");
    uip_abort();
  }
}
/*---------------------------------------------------------------------------*/
/**
 * \brief      Initialize the web server
 *
 *             This function initializes the web server and should be
 *             called at system boot-up.
 */
void
httpd_init(void)
{
  uip_listen(HTONS(80));
}
/*---------------------------------------------------------------------------*/
/** @} */
