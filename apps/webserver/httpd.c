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
      (void)Dcreate ( temp_path );
      temp_path[i] = '\\';
    }
  }
  (void)Dcreate ( temp_path );
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(receive_file(struct pt* worker,struct httpd_state *s,const char* filename,const size_t filelen))
{
  PT_BEGIN(worker);

  (void)Cconws("recv: ");
  (void)Cconws(filename);

  // make sure folder exists
  mkdir_for_file ( filename );

  s->file = fopen( filename,"wb" );

  if ( !s->file ) {
    PSOCK_SEND_STR2(worker,&s->sin,"HTTP/1.1 400 Bad Request\r\nConnection: close\r\n");    
    (void)Cconws(" -> failed to open!\r\n");
    PT_EXIT(worker);
  }

  s->temp_file_length = filelen;

  while ( s->temp_file_length > 0) {

    PSOCK_READBUF_LEN2(worker, &s->sin, 
      s->temp_file_length > s->inputbuf_size ? 
        s->inputbuf_size : s->temp_file_length );

    fwrite ( s->inputbuf,PSOCK_DATALEN(&s->sin), 1, s->file );
    s->temp_file_length-=PSOCK_DATALEN(&s->sin);
  }

  Cconws(" -> OK.\r\n");
  fclose( s->file );

  PT_END(worker);
}

static
PT_THREAD(receive_multipart(struct pt* worker,struct httpd_state *s,const char* filename,const size_t filelen))
{
  PT_BEGIN(worker);
  s->temp_file_length = filelen;
  memset( s->inputbuf_data,0,MULTIPART_BOUNDARY_SIZE);

  size_t data_to_store;
  size_t new_header;
  
  while (1) {
    size_t data_len = s->temp_file_length > s->inputbuf_size ? s->inputbuf_size : s->temp_file_length;
    PSOCK_READBUF_LEN2(worker, &s->sin, data_len);
    data_len = s->temp_file_length > s->inputbuf_size ? s->inputbuf_size : s->temp_file_length;
    s->temp_file_length-=PSOCK_DATALEN(&s->sin);

    //Cconws( s->inputbuf);
    
    data_to_store = 0;
    new_header = 0;

    for( size_t i = 0; i < (data_len + MULTIPART_BOUNDARY_SIZE - 1); ++i ) {
      if ( s->inputbuf_data[i] == '-' && s->inputbuf_data[i+1] == '-' ) {
        // there's a chance we've encountered a boundary
        if ( 0 == memcmp(s->boundary,&s->inputbuf_data[i+2],s->boundary_len ) ) {

          // figure out if this is last boundary
          if ( s->inputbuf_data[i+2+s->boundary_len] == '-' && 
              s->inputbuf_data[i+3+s->boundary_len] == '-') {
            // final boundary
            new_header = 0;
            Cconws("final\r\n");
            break;
          } else {
            // intermediate boundary
            // parse header
            data_to_store = i - 2;          //save up to the boundary minus "\r\n"
            i += s->boundary_len;
            new_header = i+2;
            printf("boundary %u %u\r\n", new_header, data_len);
            break;
          }
        }
      }
    }

    if ( data_to_store ) {
      // store before the boundary
    }

    if ( new_header ) {
      size_t i = new_header;
      size_t header_end = data_len;
      while ( i++ < data_len ) {
        if ( s->inputbuf_data[i] == '\r' && s->inputbuf_data[i+1] == '\n' 
          && s->inputbuf_data[i+2] == '\r' && s->inputbuf_data[i+3] == '\n' ) {
          header_end = i + 4;
          break;
        }
      }

      memcpy ( s->part_header, &s->inputbuf_data[new_header], header_end - new_header );
      s->part_header[ header_end - new_header ] = 0;

      if ( header_end == 0 ) {
        // we need more data
        while(1) {
          size_t len;
          PSOCK_READTO2(worker,&s->sin, ISO_nl);
          len = PSOCK_DATALEN(&s->sin);
          strncat( s->part_header, s->inputbuf, len );
          if ( len == 2 ) {
            header_end = len;
            break;
          }
        }
      }
      Cconws ("header:\r\n");
      Cconws ( s->part_header );
      // parse header
    }

    if ( data_len == s->inputbuf_size ) {
      memcpy( s->inputbuf_data, 
        s->inputbuf[ data_len - MULTIPART_BOUNDARY_SIZE ], 
        MULTIPART_BOUNDARY_SIZE);
    }

  }

  PT_END(worker);
}
/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_post(struct pt* worker,struct httpd_state *s))
{
  PT_BEGIN(worker);

  if ( s->expected_file_length == -1 ) {
    PSOCK_SEND_STR2(worker, &s->sin, "HTTP/1.1 411 Length Required\r\nConnection: close\r\n");        
    PT_EXIT(worker);
  }

  if ( s->multipart_encoded == 1 ) {
    PT_INIT(&worker[1]);
    PT_WAIT_THREAD(worker, receive_multipart(&worker[1],s,s->filename,s->expected_file_length) );
  } else {
    PT_INIT(&worker[1]);
    PT_WAIT_THREAD(worker, receive_file(&worker[1],s,s->filename,s->expected_file_length) );
  }
  
  PSOCK_SEND_STR2(worker, &s->sin, "HTTP/1.1 201 Created\r\nConnection: close\r\n");    

  PT_END(worker);
}

static
PT_THREAD(handle_run(struct pt* worker,struct httpd_state *s))
{
  char temp_path[256];
  size_t len;

  PT_BEGIN(worker);
  PSOCK_SEND_STR2(worker, &s->sin, "HTTP/1.1 200 OK\r\nConnection: close\r\n");

  len = strlen( s->filename );
  strncpy( temp_path, s->filename, sizeof(temp_path));
  // remove file name from the path file path base 
  for(size_t i = len; i != 0; --i) {
    if ( temp_path[i] == '\\' ) {
      temp_path[i] = '\0';
      break;
    }
  }
  // set cwd
  Dsetpath(temp_path);
  Pexec(0,s->filename,"","");
  PT_END(worker);
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
  s->multipart_encoded = 1;

  char* boundary_start = s->inputbuf;
  char* boundary_end;

  while ( *boundary_start++ != '=' );
  boundary_end = boundary_start;
  while ( *boundary_end != '\r' ) boundary_end++;
  *boundary_end = '\0';
  Cconws("Boundary: ");
  Cconws(boundary_start);
  Cconws("\r\n");
  strncpy(s->boundary,boundary_start,sizeof(s->boundary) );
  s->boundary_len = strlen(s->boundary);
}

void parse_urlencoded( struct httpd_state *s )
{
  s->multipart_encoded = 0;
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
  s->multipart_encoded = 0;

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
    PSOCK_SEND_STR( &s->sin, http_continue);
  }

  if ( s->handler_func ) {
    PT_INIT(&s->worker[0]);
    PSOCK_WAIT_THREAD(&s->sin, s->handler_func(&s->worker[0],s) );
  }

  PSOCK_CLOSE_EXIT(&s->sin);
  PSOCK_END(&s->sin);
}
/*---------------------------------------------------------------------------*/
static void 
handle_cleanup(struct httpd_state *s)
{

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
    handle_cleanup(s);
  } else if( uip_aborted() ) {
    handle_cleanup(s);
  } else if(uip_closed()  ) {
    handle_cleanup(s);
  } else if(uip_connected()) {
//    printf("ptr %p\r\n", s);
    s->inputbuf_size = INPUTBUF_SIZE;
    s->inputbuf = &s->inputbuf_data[MULTIPART_BOUNDARY_SIZE];
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
