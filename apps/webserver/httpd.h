/*
 * Copyright (c) 2001-2005, Adam Dunkels.
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
 * 3. The name of the author may not be used to endorse or promote
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
 * $Id: httpd.h,v 1.2 2006/06/11 21:46:38 adam Exp $
 *
 */

#ifndef __HTTPD_H__
#define __HTTPD_H__

#include "psock.h"
#include "httpd-fs.h"
#include "stdio.h"
#define HEAP_SIZE 1024
#define INPUTBUF_SIZE (1500*100)
// add 4 to maxumum boundary size to catter for mandatory
// dashes at the begining and end
#define MULTIPART_BOUNDARY_SIZE (70+4)

struct httpd_state {
  unsigned char timer;
  struct psock sin;
  char* inputbuf;
  char inputbuf_data[INPUTBUF_SIZE+MULTIPART_BOUNDARY_SIZE];
  uint32_t inputbuf_size;
  char filename[256];
  FILE* file;
  size_t expected_file_length;
  uint32_t expected_100_continue;
  
  char multipart_encoded;
  char boundary[70];
  size_t boundary_len;
  char part_header[1024];

  struct pt worker[4];
  size_t temp_file_length;
  
  char(*handler_func)(struct pt* worker,struct httpd_state *s);

  uint8_t heap[HEAP_SIZE];
};

void httpd_init(void);
void httpd_appcall(void);

void httpd_log(char *msg);
void httpd_log_file(u16_t *requester, char *file);

#endif /* __HTTPD_H__ */
