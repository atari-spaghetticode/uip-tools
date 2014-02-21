/**
 * \addtogroup apps
 * @{
 */

/**
 * \defgroup atarid Web server
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
 * $Id: atarid.c,v 1.2 2006/06/11 21:46:38 adam Exp $
 */

#include "uip.h"
#include "atarid.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <osbind.h>
#include <errno.h>

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

Fclose_safe( int16_t* fd )
{
  int16_t _fd = *fd;

  if ( _fd > 0 ) {
    Fclose(_fd);
    *fd = -1;
  }
  return _fd;
}

_DTA  dta;  /* global! */

file_size( const char* path )
{
  Fsetdta (&dta);
  Fsfirst ( path, 0 );
  return dta.dta_size;
}

static
PT_THREAD(receive_file(struct pt* worker,struct atarid_state *s,const char* filename,const size_t filelen))
{
  PT_BEGIN(worker);

  (void)Cconws(filename);
  (void)Cconws("\033K");

  // make sure folder exists
  mkdir_for_file ( filename );

  s->fd = Fcreate(filename,0);
  
  if ( s->fd < 0 ) {
    s->http_result_code = 400;
    (void)Cconws(" -> failed to open!\r\n");
    PT_EXIT(worker);
  }

  s->temp_file_length = filelen;

  while ( s->temp_file_length > 0) {

    PSOCK_READBUF_LEN2(worker, &s->sin, 
      s->temp_file_length > s->inputbuf_size ? 
        s->inputbuf_size : s->temp_file_length );

    Fwrite( s->fd,PSOCK_DATALEN(&s->sin), s->inputbuf );
    s->temp_file_length-=PSOCK_DATALEN(&s->sin);
  }

  Cconws(" -> OK.\r\n");

  Fclose_safe(&s->fd);
  s->http_result_code = 201;

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_post(struct pt* worker,struct atarid_state *s))
{
  PT_BEGIN(worker);

  if ( s->expected_file_length == -1 ) {
    s->http_result_code = 411;
    PT_EXIT(worker);
  } else if ( s->multipart_encoded == 1 ) {
    s->http_result_code = 401;
    PT_EXIT(worker);
  } else {
    PT_INIT(&worker[1]);
    PT_WAIT_THREAD(worker, receive_file(&worker[1],s,s->filename,s->expected_file_length) );
  }

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

void fstrcat( char* string, size_t* size, char* format, ... )
{
  char formated[256];
  va_list args;
  size_t formated_len = 0;

  va_start (args, format);
  formated_len = vsnprintf (formated, sizeof(formated), format, args );

  if ( *size  < strlen(string) + formated_len ) {
    *size<<=1;
    string = realloc(string,*size);
  }  

  strcat( string, formated);
  va_end (args);
} 

static void file_stat_single(char* str, size_t* size)
{
    fstrcat(str,size," {\r\n" 
      "    \"type:\" : \"%s\",\r\n",
      dta.dta_attribute&FA_DIR ? "dir" : "file");
    if (!(dta.dta_attribute&FA_DIR)) {
      fstrcat(str,size, "    \"size:\" : \"%u\",\r\n", dta.dta_size );
    }
    fstrcat(str,size, "    \"name:\" : %s\r\n", dta.dta_name );
    fstrcat(str,size,"  },\r\n"); 
}

const char* file_stat_json(const char* path)
{
  char* response;
  size_t response_size = 1024;
  Fsetdta (&dta);

  response = malloc (response_size);
  *response = 0;

  fstrcat(response,&response_size,"[\r\n");

  if ( path[0] == '/' && path[1] == '\0' ) {
    // list drivers
    uint32_t drv_map = Drvmap();
    char i = 0;
    while ( drv_map ) {
      if ( drv_map&1 ) {
        fstrcat(response,&response_size," {\r\n" 
          "    \"type:\" : \"drive\",\r\n" );
        fstrcat(response,&response_size,
           "    \"name:\" : %c\r\n", 'a' + i );
        fstrcat(response, &response_size,"  },\r\n");
      }
      i++;
      drv_map >>=1;
    }
  } else if ( 0 == Fsfirst( path,0x3f )  ) {
    // ok so this is a file
    if ( dta.dta_attribute&FA_DIR ) {
      char _path[512];
      strncpy(_path,path,sizeof(_path));
      if ( _path[strlen(_path)-1] != '\\' ) {
        strcat(_path,"\\");
      }
      strcat(_path,"*.*");
      printf("%s\r\n", _path);
      if ( 0 == Fsfirst( _path,0x3f ) ) {         
        do {
          file_stat_single(response,&response_size);
        } while ( 0 == Fsnext( ) );
      } else {
        /* Error */
        Cconws("path not found 2\r\n");
      }
    } else {
      file_stat_single(response,&response_size);
    }
  } else {
      Cconws("path not found\r\n");
  }

  fstrcat(response,&response_size,"]\r\n");

  return response;
}

/*---------------------------------------------------------------------------*/

struct GetState {
  int bytes_read;
  size_t buffer_start_offset;
  int file_len;
};

static
PT_THREAD(handle_get(struct pt* worker,struct atarid_state *s))
{
  struct GetState* this = (struct GetState*)s->heap;

  PT_BEGIN(worker);

  (void)Cconws(s->filename);

  this->file_len = file_size( s->filename );
  this->buffer_start_offset = 0;

  s->fd = Fopen(s->filename,0);
  
  if ( s->fd > 0 ) {

    this->buffer_start_offset = snprintf(
      s->inputbuf,UIP_TCP_MSS,
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %u\r\n\r\n",
      "application/octet-stream",
      this->file_len );
  
  } else {
    PSOCK_SEND_STR2(worker, &s->sin, "HTTP/1.1 404 Not Found\r\n");
    PT_EXIT(worker); 
  }
  
  while ( 1 ) {
    this->bytes_read = Fread(s->fd, 
          UIP_TCP_MSS-this->buffer_start_offset, 
          &s->inputbuf[this->buffer_start_offset]);

    if ( this->bytes_read == 0 ) 
      break;
    
    PSOCK_SEND2(worker, &s->sin,  s->inputbuf, this->bytes_read+this->buffer_start_offset );
    this->buffer_start_offset = 0;
  }
  
  Cconws(" -> OK.\r\n");
  Fclose_safe(&s->fd);

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_run(struct pt* worker,struct atarid_state *s))
{
  char temp_path[256];
  size_t len;

  (void)Cconws(s->filename);

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
  Pexec(PE_LOADGO,s->filename,"","");
  PT_END(worker);
}

void parse_url( struct atarid_state *s )
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

void parse_post( struct atarid_state *s )
{
  parse_url(s);
  s->handler_func = handle_post;
}

void parse_get( struct atarid_state *s )
{
  parse_url(s);
  s->handler_func = handle_get;
}

void parse_run( struct atarid_state *s )
{
  parse_url(s);
  s->handler_func = handle_run;
}

void parse_content_len( struct atarid_state *s )
{
  s->expected_file_length = atoi( &s->inputbuf[15] );
}

void parse_expect( struct atarid_state *s )
{
  s->expected_100_continue = 1;
}

void parse_multipart( struct atarid_state *s )
{
  s->multipart_encoded = 1;
}

void parse_urlencoded( struct atarid_state *s )
{
  s->multipart_encoded = 0;
}

#define HeaderEntry(str,func,type) {str, sizeof(str), func, type}

struct {
  const char* entry;
  size_t entry_len;
  char(*parse_func)(struct atarid_state *s);
  char request_type;
} commands[] = {
  HeaderEntry ( "POST", parse_post, 1 ),
  HeaderEntry ( "PUT", parse_post, 1 ),
  HeaderEntry ( "GET", parse_get, 1 ),
  HeaderEntry ( "RUN", parse_run, 1 ),
  HeaderEntry ( "\r\nDELETE", parse_run, 1 ),
  HeaderEntry ( "Content-Length:", parse_content_len, 0 ),
  HeaderEntry ( "Expect: 100-continue", parse_expect, 0 ),
  HeaderEntry ( "Content-Type: multipart/form-data;", parse_multipart, 0 ),
  HeaderEntry ( "Content-Type: application/x-www-form-urlencoded", parse_urlencoded, 0 ),
};

struct {
  int http_result_code;
  const char* http_response_string;
} http_responses[] = {
  { 200, "OK" },
  { 201, "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n" },
  { 400, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n" },
  { 404, "HTTP/1.1 404 Not Found\r\n\r\n" },
  { 411, "HTTP/1.1 411 Length Required\r\nConnection: close\r\n\r\n" },
};

static
PT_THREAD(handle_input(struct atarid_state *s))
{
  PSOCK_BEGIN(&s->sin);

  do {

    s->expected_100_continue = 0;
    s->expected_file_length = -1;
    s->handler_func = NULL;
    s->filename[0] = '\0';
    s->multipart_encoded = 0;
    s->fd = -1;
    s->http_result_code = 0;
    
    while(1) {
      // eat away the header
      PSOCK_READTO(&s->sin, ISO_nl);

      if ( s->inputbuf[0] == '\r' ) {
        //got header, now get the data
        break;
      }

      for ( size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i ) {
        if ( 0 == memcmp(s->inputbuf,commands[i].entry,commands[i].entry_len - 1 ) ) {
          if ( commands[i].request_type ) {
            Cconws("\033Y\x2f\x20");
            Cconws(commands[i].entry);
            Cconws(": ");
          }
          commands[i].parse_func(s);
          break;
        }
      }
    }

    if ( s->expected_100_continue ) {
      PSOCK_SEND_STR( &s->sin, "HTTP/1.1 100 Continue\r\n");
    }

    if ( s->handler_func ) {
      PT_INIT(&s->worker[0]);
      PSOCK_WAIT_THREAD(&s->sin, s->handler_func(&s->worker[0],s) );
    }

    if ( s->http_result_code != 0 ) {
      // send result code
      for ( size_t i = 0; i < sizeof( http_responses ) / sizeof(http_responses[0]); ++i ) {
        if ( s->http_result_code == http_responses[i].http_result_code ) {
          s->http_result_string = http_responses[i].http_response_string;
          break;
        }
      }
      
      PSOCK_SEND_STR( &s->sin, s->http_result_string );
    }

  } while (s->http_result_code < 400); 

  PSOCK_CLOSE_EXIT(&s->sin);
  PSOCK_END(&s->sin);
}
/*---------------------------------------------------------------------------*/
static void 
handle_error(struct atarid_state *s)
{
  if ( Fclose_safe(&s->fd) != -1 ) {
    (void)Cconws(" -> failed\r\n");
  }
}

/*---------------------------------------------------------------------------*/
char
handle_connection(struct atarid_state *s)
{
  return handle_input(s);
}
/*---------------------------------------------------------------------------*/
void
atarid_appcall(void)
{
  struct atarid_state *s = (struct atarid_state *)&(uip_conn->appstate);
  if ( uip_timedout() ) {
    handle_error(s);
//    printf("timeout\r\n");
  } else if( uip_aborted() ) {
 //   handle_error(s);
    printf("abort\r\n");
  } else if(uip_closed()  ) {
//    printf("closed\r\n");
    handle_error(s);
  } else if(uip_connected()) {
//    printf("ptr %p\r\n", s);
//    printf("Connect!\r\n");
    s->inputbuf_size = INPUTBUF_SIZE;
    s->inputbuf = &s->inputbuf_data[MULTIPART_BOUNDARY_SIZE];
    PSOCK_INIT(&s->sin, s->inputbuf, s->inputbuf_size);
    s->timer = 0;
 //   handle_connection(s);
  } else if(s != NULL) {
    // if(uip_poll()) {
    //   ++s->timer;
    //   // if(s->timer >= 20) {
    //    //   uip_abort();
    //   // }
    // } else {
    //   s->timer = 0;
    // }

    handle_connection(s);

  } else {
    printf("abort2\r\n");
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
atarid_init(void)
{
 // Cconws("\r\n\r\n\r\n\r\n\033f\033p\033j");

 // Cconws( file_stat_json( "/" ) );

 // Cconws("\033f");
  printf("val %d\r\n", UIP_TCP_MSS);
  uip_listen(HTONS(80));
}
/*---------------------------------------------------------------------------*/
/** @} */
