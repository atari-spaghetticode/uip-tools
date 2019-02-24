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
 *         Mariusz Buras <mariusz.buras@gmail.com>
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
#include "../logging.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>

#include <osbind.h>
#include <errno.h>

#include <ctype.h>
#include <stdbool.h>

// static content
/*#include "index.h"
#include "icon-down.h"
#include "icon-up.h"
#include "icon-left.h"
#include "icon-right.h"
#include "loader.h"
#include "close.h"
*/
#define ISO_nl      0x0a
#define ISO_space   0x20
#define ISO_bang    0x21
#define ISO_percent 0x25
#define ISO_period  0x2e
#define ISO_slash   0x2f
#define ISO_colon   0x3a

/*---------------------------------------------------------------------------*/
int ensureFolderExists(const char* path, int stripFileName)
{
  char temp_path[256];
  const size_t len = strlen(path);
  strncpy(temp_path, path, sizeof(temp_path));
  int ret = 0;

  // remove file name from the path file path base
  for (size_t i = len; i != 0; --i) {
    if (stripFileName && temp_path[i] == '\\') {
      temp_path[i] = '\0';
      break;
    }
  }

  // skip the drive letter in the path
  for (size_t i = 4; i < len; ++i) {
    if (temp_path[i] == '\\') {
      temp_path[i] = '\0';
      (void)Dcreate(temp_path);
      temp_path[i] = '\\';
    }
  }
  ret = Dcreate(temp_path);
  // printf("\r\nensureFolderExists: ret=%d\r\npath: %s\r\noriginal: %s", ret, temp_path, path);
  // if we strip file name then we don't want error to be reported
  // because in that case we might be overwriting a file
  return stripFileName ? 0 : ret;
}

/*---------------------------------------------------------------------------*/

int Fclose_safe(int16_t* fd)
{
  int16_t _fd = *fd;

  if (_fd > 0) {
    Fclose(_fd);
    *fd = -1;
  }
  return _fd;
}

_DTA  dta;  /* global! */

size_t file_size(const char* path)
{
  Fsetdta (&dta);
  Fsfirst (path, 0);
  return dta.dta_size;
}

static
PT_THREAD(receive_file(struct pt* worker, struct atarid_state *s, const char* filename, const size_t filelen))
{
  PT_BEGIN(worker);

  LOG_TRACE("receive_file: %s\r\n", filename);
  //(void)LOG("\033K");

  // make sure folder exists
  if (ensureFolderExists(filename, 1)) {
    s->http_result_code = 400;
    LOG_TRACE(" -> failed to create folder!\r\n");
    PT_EXIT(worker);
  }

  s->fd = Fcreate(filename, 0);

  if (s->fd < 0) {
    s->http_result_code = 400;
    LOG_TRACE(" -> failed to open!\r\n");
    PT_EXIT(worker);
  }

  s->temp_file_length = filelen;

  while (s->temp_file_length > 0) {
    int32_t write_ret = 0;
    PSOCK_READBUF_LEN2(worker, &s->sin,
      s->temp_file_length > s->inputbuf_size ?
        s->inputbuf_size : s->temp_file_length);

    write_ret = Fwrite(s->fd, PSOCK_DATALEN(&s->sin), s->inputbuf);

    if (write_ret < 0 || write_ret != PSOCK_DATALEN(&s->sin)) {
      s->http_result_code = 500;
      LOG_TRACE("Fwrite failed!\r\n");
      Fclose_safe(&s->fd);
      PT_EXIT(worker);
    }

    s->temp_file_length-=PSOCK_DATALEN(&s->sin);
  }

  if (s->tosFileDateTime) {
    _DOSTIME dostime;
    dostime.time = s->tosFileDateTimeData >> 16;
    dostime.date = s->tosFileDateTimeData & 0xffff;
    Fdatime (&dostime, s->fd, 1); // set date/time
  }

  Fclose_safe(&s->fd);
  s->http_result_code = 201;

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_post(struct pt* worker, struct atarid_state *s))
{
  PT_BEGIN(worker);

  if (s->expected_file_length == -1) {
    s->http_result_code = 411;
    PT_EXIT(worker);
  } else if (s->multipart_encoded == 1) {
    s->http_result_code = 401;
    PT_EXIT(worker);
  } else {
    PT_INIT(&worker[1]);
    PT_WAIT_THREAD(worker, receive_file(&worker[1], s, s->filename, s->expected_file_length));
  }

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

struct Repsonse {
  char* malloc_block;
  char* current;
  size_t size;
};

void fstrcat(struct Repsonse* response, char* format, ...)
{
  char formated[256];
  va_list args;
  size_t formated_len = 0;

  va_start (args, format);
  formated_len = vsnprintf (formated, sizeof(formated), format, args);

  if (response->size  <= (response->current-response->malloc_block) + formated_len) {
    size_t current_offset = response->current-response->malloc_block;
//    printf("realloc: %zu->%zu\r\n", response->size, response->size*2);
    response->size = response->size * 2;
    response->malloc_block = realloc(response->malloc_block,response->size);
    response->current = &response->malloc_block[current_offset];
//    printf("realloc: %p\r\n", response->malloc_block);
  }

  strcat(response->current, formated);
  response->current = &response->current[ formated_len ];
  va_end (args);
}

static void file_stat_single(struct Repsonse* response)
{
  char json_name[256];

  fstrcat(response," {\r\n"
    "    \"type\" : \"%s\",\r\n",
    dta.dta_attribute&FA_DIR ? "d" : "f");

  if (!(dta.dta_attribute&FA_DIR)) {
    fstrcat(response, "    \"size\" : \"%u\",\r\n", dta.dta_size);
  }

  strncpy (json_name, dta.dta_name, sizeof(json_name));

  for(size_t i=0; json_name[i] != 0; i++)
  {
    if (json_name[i] == '\\') {
      json_name[i] = '_';
    }
  }

  fstrcat(response, "    \"name\" : \"%s\"\r\n", json_name);
  fstrcat(response,"  }\r\n");
}

const char* file_stat_json(const char* path)
{
  struct Repsonse response = { NULL, NULL, 8192 };
  char dos_path[512] = { '\0' };
  char dos_path_helper[2] = { '\0', '\0' };

  Fsetdta (&dta);
  memset(&dta,0,sizeof(dta));
  response.malloc_block = (char*)malloc (response.size);
  *response.malloc_block = 0;
  response.current = response.malloc_block;

  strncpy (dos_path,path,sizeof(dos_path));

  fstrcat(&response," [\r\n");

  // this is a bit dodgy, I'm not sure why I need to do this:
  // it turns out that to scan the root of the drive I need to
  // supply a path that ends with a backslash:
  // d:\
  // otherwise it won't work
  // But for forlders it accually can't end with backslash because
  // it won't work.. madness!!!!!
  if (strlen(dos_path) > 3 && dos_path[strlen(dos_path)-1] == '\\') {
    dos_path[strlen(dos_path)-1] = 0;
  }

  if (dos_path[0] == '\0') {
    // list drivers
    uint32_t drv_map = Drvmap();
    char i = 0;
    int first = 1;
    while (drv_map) {
      if (drv_map&1) {
        if (!first){
          fstrcat(&response, ",\r\n");
        }
        first = 0;
        fstrcat(&response, " {\r\n"
          "    \"type\" : \"d\",\r\n");
        fstrcat(&response,
           "    \"name\" : \"%c\"\r\n", 'a' + i);
        fstrcat(&response,"  }\r\n");
      }
      i++;
      drv_map >>=1;
    }
  }
    /* if we're at the root of the drive or at a folder */
  else if (strlen(dos_path) == 3 || 0 == Fsfirst(dos_path, FA_DIR|FA_HIDDEN|FA_SYSTEM)) {
    // ok so this is a folder
    if (strlen(dos_path) == 3 || (dta.dta_attribute&FA_DIR)) {
      if (dos_path[strlen(dos_path)-1] != '\\') {
        strcat(dos_path,"\\");
      }
      strcat(dos_path,"*.*");
      if (0 == Fsfirst(dos_path, FA_DIR|FA_HIDDEN|FA_SYSTEM)) {
        int first = 1;
        do {
          // skip .. and . pseudo folders
          if (strcmp(dta.dta_name,"..") != 0
              && strcmp(dta.dta_name,".") != 0
          // && !dta.dta_attribute&FA_SYSTEM
              && !(dta.dta_attribute&FA_LABEL)
           ) {
            if (!first) {
              fstrcat(&response,",\r\n");
            }
            file_stat_single(&response);
            first = 0;
          }
        } while (0 == Fsnext());
      } else {
        /* Error */
        LOG_TRACE("path not found 2\r\n");
        goto error;
      }
    } else {
      // it's a file
      file_stat_single(&response);
    }
  } else {
      LOG_TRACE("path not found\r\n");
      goto error;
  }

  fstrcat(&response, "]\r\n");

  return response.malloc_block;

error:
  free(response.malloc_block);
  return NULL;
}

/*---------------------------------------------------------------------------*/

struct DataSource
{
  ssize_t (*read)(struct DataSource*, size_t, void*);
  void (*close)(struct DataSource*);
  ssize_t (*size)(struct DataSource*);
  const char* mime_type;
  const char* encoding_type;
};

struct FsSource
{
  struct DataSource src;
  int16_t fd;
  size_t size;
};

ssize_t fileSourceSize(struct DataSource* ds)
{
  struct FsSource* fs = (struct FsSource*) ds;
  return fs->size;
}

ssize_t fileSourceRead(struct DataSource* ds, size_t size, void* ptr)
{
  struct FsSource* fs = (struct FsSource*) ds;
  return (size_t)Fread(fs->fd, size, ptr);
}

void fileSourceClose(struct DataSource* ds)
{
  struct FsSource* fs = (struct FsSource*) ds;
  Fclose_safe(&fs->fd);
  free ((void*) fs);
}

struct DataSource* fileSourceCreate(
  const char* fname,
  const char* mime_type,
  const char* encoding_type)
{
  struct FsSource* src = NULL;
  int16_t fd = Fopen(fname, 0);

  if (fd > 0) {
    src = (struct FsSource*)malloc(sizeof(struct FsSource));
    src->size = file_size(fname);
    src->src.read = fileSourceRead;
    src->src.close = fileSourceClose;
    src->src.size = fileSourceSize;
    src->fd = fd;
    src->src.mime_type = mime_type;
    src->src.encoding_type = encoding_type;
  }

  return (struct DataSource*)src;
}

struct MemSource
{
  struct DataSource src;
  const char* ptr;
  ssize_t size;
  ssize_t current;
  char ownership;
};

ssize_t memSourceRead(struct DataSource* ds, size_t size, void* ptr)
{
  struct MemSource* mem = (struct MemSource*) ds;
  size_t actual_size = size + mem->current > mem->size ? mem->size-mem->current : size;

  if (mem->current >= mem->size) {
    return 0;
  }

  memcpy (ptr, &mem->ptr[mem->current], actual_size);

  mem->current+=actual_size;

  return actual_size;
}

void memSourceClose (struct DataSource* ds)
{
  struct MemSource* mem = (struct MemSource*) ds;
  if (mem->ownership) {
    free ((void*) mem->ptr);
  }
  free ((void*) mem);
}

ssize_t memSourceSize(struct DataSource* ds)
{
  struct MemSource* mem = (struct MemSource*) ds;
  return mem->size;
}

struct DataSource* memSourceCreate (
    const char* ptr,
    size_t size,
    const char* mime_type,
    const char* encoding_type,
    char ownership)
{
  struct MemSource* src = NULL;
  src = (struct MemSource*)malloc(sizeof(struct MemSource));
  src->src.read = memSourceRead;
  src->src.close = memSourceClose;
  src->src.size = memSourceSize;
  src->current = 0;
  src->ptr = ptr;
  src->size = size;
  src->src.mime_type = mime_type;
  src->src.encoding_type = encoding_type;
  src->ownership = ownership;
  return (struct DataSource*)src;
}

struct GetState {
  int bytes_read;
  size_t buffer_start_offset;
  int file_len;
  struct DataSource* source;
};

static
PT_THREAD(handle_get(struct pt* worker, struct atarid_state *s))
{
  struct GetState* this = (struct GetState*)s->heap;
  struct DataSource* src = s->handler_datasrc;

  PT_BEGIN(worker);

  if (!src) {
    s->http_result_code = 400;
    PT_EXIT(worker);
  }

  this->buffer_start_offset = 0;

  if (src) {
    this->file_len = src->size(src);

    this->buffer_start_offset = snprintf(
      s->inputbuf, UIP_TCP_MSS,
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %s\r\n"
      "Content-Encoding: %s\r\n"
      "Connection: Keep-Alive\r\n"
      "Content-Length: %u\r\n\r\n",
      src->mime_type,
      src->encoding_type,
      this->file_len);

  } else {
    s->http_result_code = 404;
    LOG("\r\n");
    PT_EXIT(worker);
  }

  const uint32_t send_buffer_size = UIP_TCP_MSS*16 > INPUTBUF_SIZE ? UIP_TCP_MSS : UIP_TCP_MSS*16;

  while (1) {
    this->bytes_read = src->read(src, send_buffer_size-this->buffer_start_offset,
            &s->inputbuf[this->buffer_start_offset]);

    if (this->bytes_read == 0)
      break;

    if (this->bytes_read < 0) {
      s->http_result_code = 400;
      break;
    }

    PSOCK_SEND2(worker, &s->sin, s->inputbuf, this->bytes_read+this->buffer_start_offset);
    this->buffer_start_offset = 0;
  }

  src->close(src);
  s->handler_datasrc = NULL;
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
void handle_idle_run(struct atarid_state *s)
{
  /* Process arguments */
  s->idle_run_handler = NULL;

  char* args = strchr(s->query, '=');
  if (args)  {
    const size_t max_args_len = 124;
    size_t args_len = strlen(args+1);
    if (args_len > max_args_len) {
      LOG_WARN("Command line too long!");
      args_len = max_args_len;
    }
    *args = (uint8_t) args_len;
  }

  // Bconmap(7);
  void* basepage = (void*)Pexec(PE_LOAD, s->run_path, args, "");
  //Fforce(1, 2);
  int16_t pexec_ret = Pexec(PE_GO_FREE, 0, basepage, 0);

  while( -1 == Cconis() ) Cconin ();
  Dsetdrv (s->storeCurrentDrive);
  Dsetpath(s->storeCurrentPath);
}

static
PT_THREAD(handle_run(struct pt* worker, struct atarid_state *s))
{
  char temp_path[256];
  bool path_ok = false;

  PT_BEGIN(worker);

  strncpy(temp_path, s->filename, sizeof(temp_path));
  // remove file name from the path
  for (size_t i = strlen(s->filename); i != 0; --i) {
    if (temp_path[i] == '\\') {
      temp_path[i] = '\0';
      break;
    }
    temp_path[i] = '\0';
  }

  Dgetpath (s->storeCurrentPath, 0);
  s->storeCurrentDrive = Dgetdrv ();

  /* Set the current drive and path */
  if (tolower(temp_path[0]) >= 'a'
      && tolower(temp_path[0]) <= 'z') {
    uint32_t drv_map = Dsetdrv (Dgetdrv ());
    uint32_t drv_num = tolower(temp_path[0]) - 'a';
    uint32_t drv_bit = 1 << drv_num;

    if (drv_bit&drv_map) {
      char cwd[256];
      Dsetdrv (drv_num);

      Dgetpath (cwd, 0);
      /* We want to convert to lower case and skip first backslash */
      for (int i = 0; i < strlen(cwd); i++) {
        cwd[i] = tolower (cwd[i+1]);
      }

      const char* path_no_drive = &temp_path[3];

      path_ok = true;

      if (strcmp(cwd, path_no_drive) != 0) {
        // set cwd
        int ret = Dsetpath(path_no_drive);
        path_ok = ret == 0 ? true: false;
      }
    }
  }

  if (!path_ok) {
    LOG_WARN("Path set failed: %s\r\n", s->filename);
    s->http_result_code = 400;
    PSOCK_EXIT2(worker, &s->sin);
  }

  /* Close connection first */
  PSOCK_SEND_STR2(worker, &s->sin, "HTTP/1.0 200 OK\r\nConnection: close\r\n");
  PSOCK_CLOSE(&s->sin);
  //PSOCK_WAIT_UNTIL2(worker, &s->sin, !uip_conn_active_current());
  //PT_YIELD(worker);
  s->idle_run_handler = handle_idle_run;
  strcpy(s->run_path, s->filename);
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

void parse_url(struct atarid_state *s)
{
  char* fn_end = s->inputbuf;
  char* fn, *query;

  /* Find the first character of the file path */
  while (*fn_end++ != '/');

  fn = fn_end;

  /* skip to the end of line */
  while (*fn_end != '\r' && *fn_end != '\n') {
    fn_end++;
  }

  /* We want to skip the last word if the line which would usually be something like "HTTP/1.0" */
  while (*fn_end != ' ') {
    --fn_end;
  }

  *fn_end = 0;

  /* store original path */
  strncpy(s->original_filename, fn, sizeof(s->original_filename));

  /* extract query string */
  query=fn;
  s->query[0] = 0;

  /* find if there's a query in the URL */
  while (*query != '?' && query != fn_end) query++;

  /* got query string? */
  if (query != fn_end) {
    /* Now copy the query with the unnecesery bit removed */
    strncpy (s->query, &query[1], sizeof(s->query));
    *query=0;
  }

  /* convert path to dos/atari format */
  while (fn != fn_end--) {
    if (*fn_end == '/') {
      *fn_end = '\\';
    }
  }

  s->filename[0] = fn[0]&0x7f;
  s->filename[1] = ':';
  strncpy(&s->filename[2], ++fn, sizeof(s->filename)-2);
}

static int query_dir(struct atarid_state *s)
{
  const char* dir_json = file_stat_json(s->filename);
  if (dir_json) {
    s->handler_datasrc = memSourceCreate(dir_json, strlen(dir_json), "text/javascript", "identity", 1);
    return 1;
  }
  return 0;
}

static int query_run(struct atarid_state *s)
{
  s->handler_func = handle_run;
}

static int query_newfolder(struct atarid_state *s)
{
  s->http_result_code = 1200;
  s->handler_func = NULL;
  if (0 != ensureFolderExists(s->filename, 0)) {
    s->http_result_code = 400;
  }
}

static int query_setfiledate(struct atarid_state *s)
{
  char* dateIntStart = strchr(s->query, '=');;
  if (dateIntStart) {
    dateIntStart++; // skip the '='
    s->tosFileDateTime = 1;
    s->tosFileDateTimeData = strtoul (dateIntStart, NULL, 0);
  }
}

static int parse_query(struct atarid_state *s)
{
  static struct {
    const char* query_string;
    int (*query_func)(struct atarid_state *s);
  } query_mapping [] = {
    {"dir", query_dir},
    {"run", query_run},
    {"newfolder", query_newfolder},
    {"setfiledate", query_setfiledate},
    {NULL,NULL}
  };
  LOG_TRACE("parse query: ");
  if (s->query[0] != 0) {
    for (size_t i = 0; query_mapping[i].query_string != 0 ; i++) {
      if (strncmp(query_mapping[i].query_string, s->query, strlen(query_mapping[i].query_string)) == 0) {
          LOG_TRACE("%s\r\n", query_mapping[i].query_string);
          return query_mapping[i].query_func(s);
      }
    }
  }
  LOG_TRACE("query not supported: %s\r\n", s->query);
  return 0;
}

static void parse_post(struct atarid_state *s)
{
  parse_url(s);
  parse_query(s);
  s->handler_func = handle_post;
}

struct {
  const char* url;
  const unsigned char* data_ptr;
  const unsigned int data_size;
  const char* content_type;
  const char* encoding_type;
} static static_url_mapping [] = {
/*    { "", index_html_gz,index_html_gz_len,"text/html; charset=UTF-8", "gzip"},
  { "images/icon-down.png", icon_down_png,icon_down_png_len,"image/png", "identity"},
  { "images/icon-up.png", icon_up_png,icon_up_png_len,"image/png", "identity"},
  { "images/icon-left.png", icon_left_png,icon_left_png_len,"image/png", "identity"},
  { "images/icon-right.png", icon_right_png,icon_right_png_len,"image/png", "identity"},
  { "images/close.png", close_png,close_png_len,"image/png", "identity"},
  { "images/loader.gif", loader_gif,loader_gif_len,"image/gif", "identity"},*/
  { NULL,NULL,0 }
};

static void parse_get(struct atarid_state *s)
{
  parse_url(s);

  s->handler_datasrc = NULL;
  s->handler_func = handle_get;

  if (s->query[0] == 0) {
    /* maybe request a static resource */
    for (size_t i = 0; static_url_mapping[i].url!=0 ; i++) {
      if (strcmp(static_url_mapping[i].url, s->original_filename) == 0) {
        s->handler_datasrc = memSourceCreate(
                              static_url_mapping[i].data_ptr,
                              static_url_mapping[i].data_size,
                              static_url_mapping[i].content_type,
                              static_url_mapping[i].encoding_type,
                              0);
        LOG(static_url_mapping[i].url);
        break;
      }
    }
    if (!s->handler_datasrc) {
      LOG(s->filename);
      s->handler_datasrc = fileSourceCreate(s->filename, "application/octet-stream","identity");
      if (!s->handler_datasrc) {
        // Couldn't create file source
        s->http_result_code = 400;
        s->handler_func = NULL;
      }
    }
  } else if (!parse_query(s)) {
    s->http_result_code = 400;
    s->handler_func = NULL;
  }
}

static void parse_content_len(struct atarid_state *s)
{
  s->expected_file_length = atoi(&s->inputbuf[15]);
}

static void parse_delete(struct atarid_state *s)
{

}

static void parse_expect(struct atarid_state *s)
{
  s->expected_100_continue = 1;
}

static void parse_urlencoded(struct atarid_state *s)
{
  s->multipart_encoded = 0;
}

#define HeaderEntry(str,func,type) {str, sizeof(str), func, type}

struct {
  const char* entry;
  const size_t entry_len;
  void (*parse_func)(struct atarid_state *s);
  const char request_type;
} static commands[] = {
  HeaderEntry ("POST", parse_post, 1),
  HeaderEntry ("PUT", parse_post, 1),
  HeaderEntry ("GET", parse_get, 1),
  HeaderEntry ("DELETE", parse_delete, 1),
  HeaderEntry ("Content-Length:", parse_content_len, 0),
  HeaderEntry ("Expect: 100-continue", parse_expect, 0),
  HeaderEntry ("Content-Type: application/x-www-form-urlencoded", parse_urlencoded, 0),
};

struct {
  const int http_result_code;
  const char* http_response_string;
} static http_responses[] = {
  { 200, "HTTP/1.1 200 OK" },
  { 201, "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n" },
  { 400, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n" },
  { 404, "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n" },
  { 411, "HTTP/1.1 411 Length Required\r\nConnection: close\r\n\r\n" },
  { 1200, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n" },
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
    s->original_filename[0]= '\0';
    s->multipart_encoded = 0;
    s->fd = -1;
    s->http_result_code = 0;
    s->tosFileDateTime = 0;

    while (1) {
      // eat away the header
      PSOCK_READTO(&s->sin, ISO_nl);

      if (s->inputbuf[0] == '\r') {
        //got header, now get the data
        break;
      }

      for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (0 == memcmp(s->inputbuf, commands[i].entry, commands[i].entry_len - 1)) {
          LOG_TRACE("method: %s\r\n", commands[i].entry);
          commands[i].parse_func(s);
          break;
        }
      }
    }

    if (s->expected_100_continue) {
      PSOCK_SEND_STR(&s->sin, "HTTP/1.1 100 Continue\r\n");
    }

    /* if handler function was set execute it */
    if (s->handler_func) {
      PT_INIT(&s->worker[0]);
      PSOCK_WAIT_THREAD(&s->sin, s->handler_func(&s->worker[0], s));
    }

    LOG("\r\n");

    if (s->http_result_code != 0) {
      // send result code
      for (size_t i = 0; i < sizeof(http_responses) / sizeof(http_responses[0]); ++i) {
        if (s->http_result_code == http_responses[i].http_result_code) {
          s->http_result_string = http_responses[i].http_response_string;
          break;
        }
      }
      if (s->http_result_string) {
        PSOCK_SEND_STR(&s->sin, s->http_result_string);
          /* this wont work, connection needs to be closed first */
      } else {
        LOG_WARN("Error: no result string for the code");
      }
    }
  } while (s->http_result_code < 299);

  PSOCK_CLOSE_EXIT(&s->sin);
  PSOCK_END(&s->sin);
}
/*---------------------------------------------------------------------------*/
static void
handle_error(struct atarid_state *s)
{
  if (Fclose_safe(&s->fd) != -1) {
    LOG_WARN("FClose -> failed\r\n");
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
  if (uip_timedout()) {
    LOG_WARN("Connection timeout\r\n");
    handle_error(s);
  } else if(uip_aborted()) {
    LOG_WARN("Connection aborted\r\n");
  } else if(uip_closed()) {
    LOG_WARN("Connection closed\r\n");
    /* allow connection handler to do it's cleanup if connection was closed while
      calling into UIP which would result in this code being executed and thead
      never resumed again so that it would have no chance of cleaning up after itself.
      */
    handle_connection(s);
    /* now check if there's and outstanding error */
    handle_error(s);
  } else if(uip_connected()) {
    LOG_TRACE("Connection established\r\n");
    s->inputbuf_size = INPUTBUF_SIZE;
    s->inputbuf = &s->inputbuf_data[0];
    PSOCK_INIT(&s->sin, s->inputbuf, s->inputbuf_size);
    s->timer = 0;
  } else if (uip_is_idle()) {
    if (s->idle_run_handler) {
      s->idle_run_handler(s);
    }
  } else if(s != NULL) {
    handle_connection(s);
  } else {
    LOG_WARN("Unknown condition, aborting connection\r\n");
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
  struct atarid_state *s = (struct atarid_state *)&(uip_conn->appstate);
  memset(s, sizeof(struct atarid_state), 0);
  PT_INIT(PSOCK_GET_TREAD(&s->sin));
  uip_listen(HTONS(80));
}
/*---------------------------------------------------------------------------*/
/** @} */
