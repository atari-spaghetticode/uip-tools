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
// static unsigned short
// generate_part_of_file(void *state)
// {
//   struct httpd_state *s = (struct httpd_state *)state;

//   if(s->file.len > uip_mss()) {
//     s->len = uip_mss();
//   } else {
//     s->len = s->file.len;
//   }
//   memcpy(uip_appdata, s->file.data, s->len);
  
//   return s->len;
// }
/*---------------------------------------------------------------------------*/
// static
// PT_THREAD(send_file(struct httpd_state *s))
// {
//   PSOCK_BEGIN(&s->sout);
  
//   do {
//     PSOCK_GENERATOR_SEND(&s->sout, generate_part_of_file, s);
//     s->file.len -= s->len;
//     s->file.data += s->len;
//   } while(s->file.len > 0);
      
//   PSOCK_END(&s->sout);
// }
/*---------------------------------------------------------------------------*/
// static
// PT_THREAD(send_part_of_file(struct httpd_state *s))
// {
//   PSOCK_BEGIN(&s->sout);

//   PSOCK_SEND(&s->sout, s->file.data, s->len);
  
//   PSOCK_END(&s->sout);
// }
/*---------------------------------------------------------------------------*/
// static void
// next_scriptstate(struct httpd_state *s)
// {
//   char *p;
//   p = strchr(s->scriptptr, ISO_nl) + 1;
//   s->scriptlen -= (unsigned short)(p - s->scriptptr);
//   s->scriptptr = p;
// }
/*---------------------------------------------------------------------------*/
// static
// PT_THREAD(handle_script(struct httpd_state *s))
// {
//   char *ptr;
  
//   PT_BEGIN(&s->scriptpt);


//   while(s->file.len > 0) {

//     /* Check if we should start executing a script. */
//     if(*s->file.data == ISO_percent &&
//        *(s->file.data + 1) == ISO_bang) {
//       s->scriptptr = s->file.data + 3;
//       s->scriptlen = s->file.len - 3;
//       if(*(s->scriptptr - 1) == ISO_colon) {
// 	httpd_fs_open(s->scriptptr + 1, &s->file);
// 	PT_WAIT_THREAD(&s->scriptpt, send_file(s));
//       } else {
// 	PT_WAIT_THREAD(&s->scriptpt,
// 		       httpd_cgi(s->scriptptr)(s, s->scriptptr));
//       }
//       next_scriptstate(s);
      
//       /* The script is over, so we reset the pointers and continue
// 	 sending the rest of the file. */
//       s->file.data = s->scriptptr;
//       s->file.len = s->scriptlen;
//     } else {
//       /* See if we find the start of script marker in the block of HTML
// 	 to be sent. */

//       if(s->file.len > uip_mss()) {
// 	s->len = uip_mss();
//       } else {
// 	s->len = s->file.len;
//       }

//       if(*s->file.data == ISO_percent) {
// 	ptr = strchr(s->file.data + 1, ISO_percent);
//       } else {
// 	ptr = strchr(s->file.data, ISO_percent);
//       }
//       if(ptr != NULL &&
// 	 ptr != s->file.data) {
// 	s->len = (int)(ptr - s->file.data);
// 	if(s->len >= uip_mss()) {
// 	  s->len = uip_mss();
// 	}
//       }
//       PT_WAIT_THREAD(&s->scriptpt, send_part_of_file(s));
//       s->file.data += s->len;
//       s->file.len -= s->len;
      
//     }
//   }
  
//   PT_END(&s->scriptpt);
// }
/*---------------------------------------------------------------------------*/
// static
// PT_THREAD(send_headers(struct httpd_state *s, const char *statushdr))
// {
//   char *ptr;

//   PSOCK_BEGIN(&s->sout);

//   PSOCK_SEND_STR(&s->sout, statushdr);

//   ptr = strrchr(s->filename, ISO_period);
//   if(ptr == NULL) {
//     PSOCK_SEND_STR(&s->sout, http_content_type_binary);
//   } else if(strncmp(http_html, ptr, 5) == 0 ||
// 	    strncmp(http_shtml, ptr, 6) == 0) {
//     PSOCK_SEND_STR(&s->sout, http_content_type_html);
//   } else if(strncmp(http_css, ptr, 4) == 0) {
//     PSOCK_SEND_STR(&s->sout, http_content_type_css);
//   } else if(strncmp(http_png, ptr, 4) == 0) {
//     PSOCK_SEND_STR(&s->sout, http_content_type_png);
//   } else if(strncmp(http_gif, ptr, 4) == 0) {
//     PSOCK_SEND_STR(&s->sout, http_content_type_gif);
//   } else if(strncmp(http_jpg, ptr, 4) == 0) {
//     PSOCK_SEND_STR(&s->sout, http_content_type_jpg);
//   } else {
//     PSOCK_SEND_STR(&s->sout, http_content_type_plain);
//   }
//   PSOCK_END(&s->sout);
// }
/*---------------------------------------------------------------------------*/

// static
// PT_THREAD(handle_output(struct httpd_state *s))
// {
//   char *ptr;
  
//   PT_BEGIN(&s->outputpt);

//   if ( post == 1)
//   {
//     PT_WAIT_THREAD(&s->outputpt, send_headers(s, http_continue));
//     post = 2;
//     s->state = STATE_WAITING;
//     //PSOCK_CLOSE_EXIT(&s->sout);
//   } //else 
 
//   // if(!httpd_fs_open(s->filename, &s->file)) {
//   //   httpd_fs_open(http_404_html, &s->file);
//   //   strcpy(s->filename, http_404_html);
//   //   PT_WAIT_THREAD(&s->outputpt,
// 		//    send_headers(s,
// 		//    http_header_404));
//   //   PT_WAIT_THREAD(&s->outputpt,
// 		//    send_file(s));
//   // } else {
//   //   PT_WAIT_THREAD(&s->outputpt,
// 		//    send_headers(s,
// 		//    http_header_200));
//   //   ptr = strchr(s->filename, ISO_period);
//   //   if(ptr != NULL && strncmp(ptr, http_shtml, 6) == 0) {
//   //     PT_INIT(&s->scriptpt);
//   //     PT_WAIT_THREAD(&s->outputpt, handle_script(s));
//   //   } else {
//   //     PT_WAIT_THREAD(&s->outputpt,
// 		//      send_file(s));
//   //   }
//   // }
//   //PSOCK_CLOSE(&s->sout);
//   PT_END(&s->outputpt);
// }
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_input(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sin);
  s->expected_100_continue = 0;
  s->req_type = HttpReqNone;

  while(1) {
    // eat away the header
    PSOCK_READTO(&s->sin, ISO_nl);

    if ( s->inputbuf[0] == '\r' ) {
      //got header, now get the data
      break;
    }

    if(strncmp(s->inputbuf, http_get, 4) == 0) {
      s->req_type = HttpReqGet;
    }  else if(strncmp(s->inputbuf, http_post, 4) == 0) {
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

      
      s->req_type = HttpReqPost;
    }

    if(strncmp(s->inputbuf, "Content-Length:",15) == 0) {
      s->expected_file_length = atoi( &s->inputbuf[15] );
      //fprintf(stdout, "expected_file_length: %u\r\n", s->expected_file_length );
    }

    if(strncmp(s->inputbuf, "Expect: 100-continue",20) == 0) {
      s->expected_100_continue = 1;
     // fprintf(stdout, "expected_100_continue:\r\n");
    }
  }

  if ( s->req_type == HttpReqPost ) {
    printf( "post file: %s", s->filename );
    // check if expected?
    if ( s->expected_100_continue ) {
      PSOCK_SEND_STR(&s->sin, http_continue);
    }

    // make sure folder exists
    mkdir ( "d:\\test.1", S_IRWXU );

    s->file = fopen( s->filename,"wb" );

    if ( !s->file ) {
      fprintf(stderr, " -> failed to open!");
    } else {
      while ( s->expected_file_length > 0) {
        PSOCK_READBUF_LEN(&s->sin, 
          s->expected_file_length > s->inputbuf_size ? 
            s->inputbuf_size : s->expected_file_length );

        fwrite ( s->inputbuf,PSOCK_DATALEN(&s->sin), 1, s->file );
        s->expected_file_length-=PSOCK_DATALEN(&s->sin);
      }

      printf(" -> done.\r\n");
      fclose( s->file );      
    }
  } else if ( s->req_type == HttpReqGet ) {
    // do something
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
