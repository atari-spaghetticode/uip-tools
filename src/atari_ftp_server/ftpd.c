/*
 * Copyright (c) 2004, Adam Dunkels.
 * Copyright (c) 2015-20, Mariusz Buras.
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
#include "ftpd.h"
#include "../logging.h"
#include "../common.h"

#include "psock.h"

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


#define ISO_nl      0x0a
#define ISO_space   0x20
#define ISO_bang    0x21
#define ISO_percent 0x25
#define ISO_period  0x2e
#define ISO_slash   0x2f
#define ISO_colon   0x3a

#define HEAP_SIZE 4096
#define INPUTBUF_SIZE (0x8000)

#define FTPD_PATH_LENGTH (1024)
#define FTPD_RESPONSE_LENGTH (256)
// This hardcoded value is a bit dodgy...
#define FTPD_MAX_DATA_CONNECTIONS (1)


/*---------------------------------------------------------------------------*/

enum data_state_request {Wait, List, Retr, Stor, Quit};
struct ftpd_data_state;

static int ftd_data_get_port(struct ftpd_data_state* s);
static struct ftpd_data_state* ftpd_get_data_state(int port);
static int ftpd_alloc_data_state();
static void ftpd_free_data_state(struct ftpd_data_state* s);
static bool ftpd_data_is_busy(struct ftpd_data_state* s);
static bool ftpd_data_is_connected(struct ftpd_data_state* s);
static void ftpd_data_list(struct ftpd_data_state* s, const char* path);
static void ftpd_data_stor(struct ftpd_data_state* s, const char* path);
static void ftpd_data_retr(struct ftpd_data_state* s, const char* path);

static void ftpd_mark_closed_data_state(struct ftpd_data_state* s);
static void ftpd_garbage_collest_data_states();
static bool ftpd_wait_free_data_connection();

/*---------------------------------------------------------------------------*/

struct ftpd_control_state {

  int ftp_result_code;
  int data_port;
  const char* ftp_result_string;
  char cwd[FTPD_PATH_LENGTH];
  char* args;         // points to text after the request string eg. "LIST dir" points to "dir"
  struct psock sin;
  uint8_t* inputbuf;
  uint32_t inputbuf_size;

  char rnfr_name[FTPD_PATH_LENGTH]; // temp variable with a name of the file we want to rename

  char response_string[FTPD_RESPONSE_LENGTH]; // temp variable with a name of the file we want to rename

  struct pt worker[4];

  uip_ipaddr_t remote_ipaddr;

  char(*handle_command_func)(struct pt* worker,struct ftpd_control_state *s);

  int16_t  fd;

  uint8_t heap[HEAP_SIZE];
};

/*---------------------------------------------------------------------------*/

static void file_stat_single(struct Repsonse* response)
{
  fstrcat(response, "%s 1 None None %d Jan 10 2019 %s\r\n",
    dta.dta_attribute&FA_DIR ? "drwxrwxrwx" : "-rwxrwxrwx",
    !dta.dta_attribute&FA_DIR ? 0 : dta.dta_size,
    dta.dta_name);
}

char* ftp_file_stat(const char* path)
{
  struct Repsonse response = { NULL, NULL, 8192 };
  char dos_path[FTPD_PATH_LENGTH] = { '\0' };
  char dos_path_helper[2] = { '\0', '\0' };

  Fsetdta (&dta);
  memset(&dta, 0, sizeof(dta));
  response.malloc_block = (char*)malloc (response.size);
  *response.malloc_block = 0;
  response.current = response.malloc_block;

  strncpy (dos_path, path, sizeof(dos_path));

  fstrcat(&response, "drwxr--r--   1 None     None          0 Jan 10 2019 .\r\n");
  fstrcat(&response, "drwxr--r--   1 None     None          0 Jan 10 2019 ..\r\n");

  if (dos_path[0] == '/' && dos_path[1] == '\0') {
    // list drivers

    uint32_t drv_map = Drvmap();
    char i = 0;
    while (drv_map) {
      if (drv_map&1) {
        fstrcat(&response, "drwxr--r--   1 None     None          0 Apr 19 2019 %c\r\n", 'a' + i);
      }
      i++;
      drv_map >>=1;
    }
  }
    /* if we're at the root of the drive or at a folder */
  else {

    strncpy (dos_path, path, sizeof(dos_path));
    char *fn = dos_path;
    /* convert path to dos/atari format */
    for (int i = 0; i < strlen(dos_path); i++) {
      if (dos_path[i] == '/') {
        dos_path[i] = '\\';
      }
    }

    dos_path[0] = dos_path[1]&0x7f;
    dos_path[1] = ':';
    dos_path[2] = '\\';

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

    LOG_TRACE("dos_path: %s\r\n", dos_path);

    if (strlen(dos_path) == 3 || 0 == Fsfirst(dos_path, FA_DIR|FA_HIDDEN|FA_SYSTEM)) {
      // ok so this is a folder
      if (strlen(dos_path) == 3 || (dta.dta_attribute&FA_DIR)) {
        if (dos_path[strlen(dos_path)-1] != '\\') {
          strcat(dos_path, "\\");
        }
        strcat(dos_path, "*.*");
        if (0 == Fsfirst(dos_path, FA_DIR|FA_HIDDEN|FA_SYSTEM)) {
          int first = 1;
          do {
            // skip .. and . pseudo folders
            if (strcmp(dta.dta_name, "..") != 0
                && strcmp(dta.dta_name, ".") != 0
            // && !dta.dta_attribute&FA_SYSTEM
                && !(dta.dta_attribute&FA_LABEL)
             ) {
              file_stat_single(&response);
            }
          } while (0 == Fsnext());
        } else {
          /* Error */
          LOG_TRACE("path not found 2\r\n");
        }
      } else {
        // it's a file
        file_stat_single(&response);
      }
    } else {
        LOG_TRACE("path not found\r\n");
    }
  }

  LOG_TRACE("done!\r\n");

  return response.malloc_block;

error:
  free(response.malloc_block);
  return NULL;
}


/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_user(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG_TRACE("handle_user\r\n");
  s->ftp_result_code = 331;
  PT_END(worker);
}

static
PT_THREAD(handle_pass(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG("handle_pass\r\n");
  s->ftp_result_code = 230;
  PT_END(worker);
}

static
PT_THREAD(handle_syst(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG("handle_syst\r\n");
  s->ftp_result_code = 215;
  PT_END(worker);
}

static
PT_THREAD(handle_feat(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG("handle_feat\r\n");
  s->ftp_result_code = 211;
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_pwd(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG("handle_pwd\r\n");

  strcpy(s->response_string, "");
  char* name = s->cwd;
  if (strlen(s->cwd) < 1) {
    name = strrchr(s->cwd,'/') + 1;
  }

  snprintf(s->response_string, sizeof(s->response_string), "257 \"%s\"\r\n", name);
  PSOCK_SEND_STR2(worker, &s->sin, s->response_string);
  s->ftp_result_code = 0;

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_type(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG("handle_type\r\n");
  s->ftp_result_code = 200;
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/
#if 0
static
PT_THREAD(handle_port(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG_TRACE("handle_port\r\n");
  PT_END(worker);
}
#endif
/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_pasv(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG_TRACE("handle_pasv\r\n");

  PSOCK_WAIT_UNTIL2(worker, &s->sin, ftpd_wait_free_data_connection());

  strcpy(s->response_string, "");
  uip_ipaddr_t host_ip;

  uip_gethostaddr(&host_ip);

  s->data_port = ftpd_alloc_data_state();

  snprintf(s->response_string, sizeof(s->response_string),
    "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
    uip_ipaddr1(host_ip), uip_ipaddr2(host_ip),
    uip_ipaddr3(host_ip), uip_ipaddr4(host_ip),
    (s->data_port>>8), s->data_port&0xff);

  LOG_TRACE("port %d\r\n", s->data_port);

  s->ftp_result_code = 0;

  PSOCK_SEND_STR2(worker, &s->sin, s->response_string);

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_list(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG("handle_list\r\n");

  PSOCK_WAIT_UNTIL2(worker, &s->sin, ftpd_data_is_connected(ftpd_get_data_state(s->data_port)));
  PSOCK_SEND_STR2(worker, &s->sin, "150 Data connection open; transfer starting\r\n");
  ftpd_data_list(ftpd_get_data_state(s->data_port), s->cwd);
  PSOCK_WAIT_UNTIL2(worker, &s->sin, !ftpd_data_is_busy(ftpd_get_data_state(s->data_port)));
  PSOCK_WAIT_UNTIL2(worker, &s->sin, !ftpd_data_is_connected(ftpd_get_data_state(s->data_port)));
  ftpd_free_data_state(ftpd_get_data_state(s->data_port));
  s->ftp_result_code = 226;

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_command_reject(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  LOG("Reject command: %s", s->inputbuf);
  s->ftp_result_code = 202;
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_cdup(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  if (strlen(s->cwd) > 1) {
    joinpath(s->cwd, "../");
  }

  LOG("handle_cdup: %s\r\n", s->cwd);
  strcpy(s->response_string, "");
  snprintf(s->response_string, sizeof(s->response_string), "250 \"%s\"\r\n", s->cwd);
  PSOCK_SEND_STR2(worker, &s->sin, s->response_string);
  s->ftp_result_code = 0;

  PT_END(worker);
}

static
PT_THREAD(handle_cwd(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  joinpath(s->cwd, s->args);

  LOG("handle_cwd: \"%s\"\r\n", s->args);

  strcpy(s->response_string, "");
  snprintf(s->response_string, sizeof(s->response_string), "250 \"%s\"\r\n", s->cwd);
  PSOCK_SEND_STR2(worker, &s->sin, s->response_string);
  s->ftp_result_code = 0;

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_stor(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  char path[FTPD_PATH_LENGTH];
  strncpy(path, s->cwd, FTPD_PATH_LENGTH);
  joinpath(path, s->args);
  convertToGemDosPath(path);
  ensureFolderExists(path, true);
  LOG_TRACE("STOR: %s\r\n", path);

  ftpd_data_stor(ftpd_get_data_state(s->data_port), path);
  PSOCK_SEND_STR2(worker, &s->sin, "150 Data connection open; transfer starting\r\n");
  PSOCK_WAIT_UNTIL2(worker, &s->sin, !ftpd_data_is_connected(ftpd_get_data_state(s->data_port)));
  LOG_TRACE("stor 3\r\n");
  ftpd_free_data_state(ftpd_get_data_state(s->data_port));
  s->ftp_result_code = 226;

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_retr(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  char path[FTPD_PATH_LENGTH];
  strncpy(path, s->cwd, FTPD_PATH_LENGTH);
  joinpath(path, s->args);
  convertToGemDosPath(path);
  ensureFolderExists(path, true);
  LOG_TRACE("RETR: %s\r\n", path);

  ftpd_data_retr(ftpd_get_data_state(s->data_port), path);
  PSOCK_WAIT_UNTIL2(worker, &s->sin, ftpd_data_is_connected(ftpd_get_data_state(s->data_port)));
  PSOCK_SEND_STR2(worker, &s->sin, "125 Data connection open; transfer starting\r\n");
  PSOCK_WAIT_UNTIL2(worker, &s->sin, !ftpd_data_is_busy(ftpd_get_data_state(s->data_port)));
  PSOCK_WAIT_UNTIL2(worker, &s->sin, !ftpd_data_is_connected(ftpd_get_data_state(s->data_port)));
  ftpd_free_data_state(ftpd_get_data_state(s->data_port));
  s->ftp_result_code = 226;

  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_mkd(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  char path[FTPD_PATH_LENGTH];
  strncpy(path, s->cwd, FTPD_PATH_LENGTH);
  joinpath(path, s->args);
  convertToGemDosPath(path);
  LOG_TRACE("MKD: %s\r\n", path);
  if (ensureFolderExists(path, false)) {
    s->ftp_result_code = 257;
  } else {
    s->ftp_result_code = 400;
  }
  PT_END(worker);
}

static
PT_THREAD(handle_dele(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  char path[FTPD_PATH_LENGTH];
  strncpy(path, s->cwd, FTPD_PATH_LENGTH);
  joinpath(path, s->args);
  convertToGemDosPath(path);
  LOG_TRACE("DELE: %s\r\n", path);
  if (!Fdelete(path)) {
    s->ftp_result_code = 250;
  } else {
    s->ftp_result_code = 400;
  }
  PT_END(worker);
}

static
PT_THREAD(handle_rmd(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  char path[FTPD_PATH_LENGTH];
  strncpy(path, s->cwd, FTPD_PATH_LENGTH);
  joinpath(path, s->args);
  convertToGemDosPath(path);
  LOG_TRACE("RMD: %s\r\n", path);
  if (!Ddelete(path)) {
    s->ftp_result_code = 250;
  } else {
    s->ftp_result_code = 400;
  }
  PT_END(worker);
}

static
PT_THREAD(handle_quit(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  s->ftp_result_code = 1000;
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_rnfr(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  strncpy(s->rnfr_name, s->cwd, FTPD_PATH_LENGTH);
  sanitiseGemDosName(s->args);
  joinpath(s->rnfr_name, s->args);
  convertToGemDosPath(s->rnfr_name);
  sanitiseGemDosPath(s->rnfr_name);
  s->ftp_result_code = 350;
  LOG_TRACE("RNFR: %s\r\n", s->rnfr_name);
  PT_END(worker);
}

static
PT_THREAD(handle_rnto(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  char rnto[FTPD_PATH_LENGTH];

  if (strlen(s->rnfr_name) > 0) {
    strncpy(rnto, s->cwd, FTPD_PATH_LENGTH);
    sanitiseGemDosName(s->args);
    joinpath(rnto, s->args);
    convertToGemDosPath(rnto);
    sanitiseGemDosPath(rnto);

    LOG_TRACE("RNTO: %s\r\n", rnto);

    if (!Frename(0, s->rnfr_name, rnto)) {
      s->ftp_result_code = 250;
      PT_EXIT(worker);
    }
    // make sure we exit rename mode anyway
    strcpy(s->rnfr_name, "");
  }
  s->ftp_result_code = 400;
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_size(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  char path[FTPD_PATH_LENGTH];
  int size;
  Fsetdta (&dta);
  strncpy(path, s->cwd, FTPD_PATH_LENGTH);
  joinpath(path, s->args);
  convertToGemDosPath(path);

  LOG_TRACE("SIZE: %s\r\n", path);
  if (getFileSize(path, &size)) {
    LOG_TRACE("SIZE: %d\r\n", size);
    snprintf(s->response_string, sizeof(s->response_string),"213 %d\r\n", size);
    PSOCK_SEND_STR2(worker, &s->sin, s->response_string);
  } else {
    s->ftp_result_code = 400;
  }
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_site(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);

  if (0 == strcmp(s->args, "DIRSTYLE")) {
    LOG_TRACE("DIRSTYLE\r\n");
    PSOCK_SEND_STR2(worker, &s->sin, "200 Unix-like.\r\n");
  } else {
    s->ftp_result_code = 202;
  }
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(handle_noop(struct pt* worker, struct ftpd_control_state *s))
{
  PT_BEGIN(worker);
  s->ftp_result_code = 200;
  PT_END(worker);
}

/*---------------------------------------------------------------------------*/

#define FTPCommand(str,func) {str, sizeof(str), func}

struct {
  const char* entry;
  const size_t entry_len;
  char(*handle_command_func)(struct pt* worker,struct ftpd_control_state *s)
} commands[] = {
    FTPCommand ("CWD", handle_cwd),
    FTPCommand ("USER", handle_user),
    FTPCommand ("LIST", handle_list),
    FTPCommand ("PASS", handle_pass),
    FTPCommand ("SYST", handle_syst),
    FTPCommand ("FEAT", handle_feat),
    FTPCommand ("PWD", handle_pwd),
    FTPCommand ("PASV", handle_pasv),
//    FTPCommand ("PORT", handle_port),   // unimplemented yet!
    FTPCommand ("TYPE", handle_type),
    FTPCommand ("CDUP", handle_cdup),

    FTPCommand ("STOR", handle_stor),
    FTPCommand ("MKD", handle_mkd),
    FTPCommand ("SIZE", handle_size),
    FTPCommand ("DELE", handle_dele),
    FTPCommand ("RMD", handle_rmd),
    FTPCommand ("RETR", handle_retr),
    FTPCommand ("SITE", handle_site),

    FTPCommand ("NOOP", handle_noop),

    FTPCommand ("RNFR", handle_rnfr),
    FTPCommand ("RNTO", handle_rnto),

    FTPCommand ("QUIT", handle_quit),
};

struct {
  const int ftp_result_code;
  const char* http_response_string;
} ftp_responses[] = {
  { 200, "200 Command successful.\r\n" },
  { 202, "202 Command unimplemented.\r\n" },
  { 211, "211 No features.\r\n" },
  { 215, "215 UIPTool.\r\n" },
  { 230, "230 Logged in.\r\n" },
  { 231, "231 Password required.\r\n" },
  { 226, "226 Transfer complete.\r\n" },
  { 257, "257 Created.\r\n" },
  { 250, "250 Operation completed.\r\n" },
  { 350, "350 Awaiting new name.\r\n" },

  { 331, "331 User login OK, waiting for password\r\n" },

  { 400, "400 Operation rejected.\r\n" },
};

static
PT_THREAD(ftpd_control_connection(struct ftpd_control_state *s))
{
  PSOCK_BEGIN(&s->sin);

  s->fd = -1;
  s->ftp_result_code = 0;
  s->data_port = 0;
  s->args = NULL;

  PSOCK_SEND_STR(&s->sin, "220 Welcome to uiptool FTP server\r\n");

  do {

    // eat away the header
    PSOCK_READTO(&s->sin, ISO_nl);
    {
      size_t readlen = psock_datalen(&s->sin);
      s->inputbuf[readlen] = 0;

      // strip trailing control characters
      char* first_control_chr = strchr(s->inputbuf, '\r');
      if (first_control_chr) {
        *first_control_chr = '\0';
      }

      s->args = strchr(s->inputbuf, ' '); // skip command
      if(s->args) s->args++;

      char* case_convert = s->inputbuf;
      while (case_convert < s->args) {
        if (*case_convert >= 'a' && *case_convert <= 'z') {
          *case_convert &= 0x5f;
        }
        case_convert++;
      }
    }

    bool method_found = false;
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
      if (0 == memcmp(s->inputbuf, commands[i].entry, commands[i].entry_len - 1)) {
        LOG_TRACE("method: %s\r\n", s->inputbuf);
        method_found = true;
        s->handle_command_func = commands[i].handle_command_func;
        PT_INIT(&s->worker[0]);
        PSOCK_WAIT_THREAD(&s->sin, s->handle_command_func(&s->worker[0], s));
        break;
      }
    }

    if (!method_found) {
      LOG_TRACE("unimplemented request: %s\r\n", s->inputbuf);
      PT_INIT(&s->worker[0]);
      PSOCK_WAIT_THREAD(&s->sin, handle_command_reject(&s->worker[0], s));
      //handle_command_reject(s);
    }

    if (s->ftp_result_code != 0) {
      // send result code
      for (size_t i = 0; i < sizeof(ftp_responses) / sizeof(ftp_responses[0]); ++i) {
        if (s->ftp_result_code == ftp_responses[i].ftp_result_code) {
          s->ftp_result_string = ftp_responses[i].http_response_string;
          break;
        }
      }

      if (s->ftp_result_string) {
        PSOCK_SEND_STR(&s->sin, s->ftp_result_string);
      } else {
        LOG_WARN("Error: no result string for the code: %d\r\n", s->ftp_result_code);
      }
      /* Keep the main loop spinning for anything other then 500 */
      if (s->ftp_result_code < 500) {
        s->ftp_result_code = 0;
      }
    }
  } while (s->ftp_result_code < 299);

  PSOCK_CLOSE_EXIT(&s->sin);
  PSOCK_END(&s->sin);
}

/*---------------------------------------------------------------------------*/
struct ftpd_control_state* ftpd_get_state()
{
  return (struct ftpd_control_state *)(uip_conn->appstate);
}

struct ftpd_control_state* ftpd_alloc_state()
{
  struct ftpd_control_state *s = ftpd_get_state();

  LOG_TRACE("ftpd_alloc_state\r\n");

  if (s) {
    LOG_WARN("connection already allocated!");
    return s;
  }

  s = (struct ftpd_control_state *)malloc(sizeof(struct ftpd_control_state));
  uip_conn->appstate = s;

  memset(s, 0, sizeof(struct ftpd_control_state));
  s->inputbuf_size = INPUTBUF_SIZE;
  s->inputbuf = malloc(INPUTBUF_SIZE);
  s->data_port = 0;
  s->cwd[0] = '/'; s->cwd[1] = '\0';
  s->args = NULL;

  LOG_TRACE("%p, buf: %p\r\n", s, s->inputbuf);

  PSOCK_INIT(&s->sin, s->inputbuf, s->inputbuf_size);

  LOG_TRACE("done\r\n");
  return s;
}

static void ftpd_free_state()
{
  struct ftpd_control_state *s = ftpd_get_state();

  LOG_TRACE("ftpd_free_state\r\n");

  if (!s) {
    LOG_WARN("connection already freed!");
    return;
  }

  ftpd_free_data_state(ftpd_get_data_state(s->data_port));

  free(s->inputbuf);
  free(s);
  uip_conn->appstate = NULL;
}

/*---------------------------------------------------------------------------*/

void
ftpd_appcall(void)
{
  struct ftpd_control_state *s = ftpd_get_state();

  if (!s && uip_connected()) {
    /* new connection */
    LOG_TRACE("connected!\r\n");
    s = ftpd_alloc_state();
  }

  if (s) {
      /* connection is already established */
      if (uip_timedout()) {
        LOG_TRACE("Connection timeout\r\n");
      } else if(uip_aborted()) {
        LOG_TRACE("Connection aborted\r\n");
      } else if(uip_closed()) {
        LOG_TRACE("Connection closed\r\n");
        /* allow connection handler to do it's cleanup if connection was closed while
          calling into UIP which would result in this code being executed and thead
          never resumed again so that it would have no chance of cleaning up after itself.
          */
        ftpd_control_connection(s);
      } else {
        /* connection is active, service the connection*/
        ftpd_control_connection(s);
        return;
      }

      ftpd_free_state();
      return;
  }
}

/*---------------------------------------------------------------------------*/

struct ftpd_data_state {

  int port;
  bool busy;
  enum data_state_request state;
  const char* path;
  char* list_data;
  bool closed;
  bool connected;
  int send_buffer_size;
  int buffer_start_offset;
  int bytes_read;
  int ftp_result_code;

  const char* http_request_type;
  const char* ftp_result_string;
  struct psock sin;
  uint8_t* inputbuf;
  uint32_t inputbuf_size;
  char query[256];
  char filename[256];
  char original_filename[256];

  int16_t  fd;

  uint8_t heap[HEAP_SIZE];
};

struct ftpd_data_state* ftpd_data_connection_states[FTPD_MAX_DATA_CONNECTIONS];

static bool ftpd_data_is_busy(struct ftpd_data_state* s)
{
  if (s) {
    return s->busy;
  } else {
    return false;
  }
}

static bool ftpd_data_is_connected(struct ftpd_data_state* s)
{
  if (s) {
    return s->connected;
  } else {
    return false;
  }
}

static void ftpd_data_list(struct ftpd_data_state* s, const char* path)
{
  s->state = List;
  s->busy = true;
  s->path = path;
  s->list_data = NULL;
  LOG_TRACE("Listing requested for: %s\r\n", path);
}

static void ftpd_data_stor(struct ftpd_data_state* s, const char* path)
{
  s->state = Stor;
  s->busy = true;
  s->path = path;
  s->list_data = NULL;
  LOG_TRACE("Stor requested for: %s\r\n", path);
}

static void ftpd_data_retr(struct ftpd_data_state* s, const char* path)
{
  s->state = Retr;
  s->busy = true;
  s->path = path;
  s->list_data = NULL;
  LOG_TRACE("Retr requested for: %s\r\n", path);
}

static int ftd_data_get_port(struct ftpd_data_state* s)
{
  return s->port;
}

static struct ftpd_data_state* ftpd_get_data_state(int port)
{
  for(size_t i = 0; i < FTPD_MAX_DATA_CONNECTIONS; i++) {
    if(ftpd_data_connection_states[i] != NULL &&
      port == ftpd_data_connection_states[i]->port) {
      return ftpd_data_connection_states[i];
    }
  }

  LOG_TRACE("ftpd_get_data_state: ftpd_data_state not found for port: %d!\r\n", port);
  return NULL;
}

static void ftpd_data_list_states()
{
  for(size_t i = 0; i < FTPD_MAX_DATA_CONNECTIONS; i++) {
    if(ftpd_data_connection_states[i] != NULL) {
      LOG_TRACE("port:: %d -> %p\r\n", ftpd_data_connection_states[i]->port, ftpd_data_connection_states[i]);
    }
  }
}

static int ftpd_alloc_data_state()
{
  struct ftpd_data_state *s;
  int slot = -1;

  LOG_TRACE("start\r\n");

  ftpd_garbage_collest_data_states();

  for(size_t i = 0; i < FTPD_MAX_DATA_CONNECTIONS; i++) {
    if(ftpd_data_connection_states[i] == NULL) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    LOG_TRACE("ftpd_alloc_data_state:: Too many active data connections!?\r\n");
    ftpd_data_list_states();
    exit(1);
    return -1;
  }

  int port = FTPD_BASE_DATA_PORT + slot;
  LOG_TRACE("data connection port: %x (%d)\r\n", port, slot);

  s = (struct ftpd_data_state *)malloc(sizeof(struct ftpd_data_state));

  memset(s, 0, sizeof(struct ftpd_data_state));
  s->inputbuf_size = INPUTBUF_SIZE;
  s->inputbuf = malloc(INPUTBUF_SIZE);
  s->state = Wait;
  s->port = port;
  s->busy = false;
  s->closed = false;
  s->connected = false;

  LOG_TRACE("ftpd_alloc_data_state: %p, buf: %p, port: %x\r\n", s, s->inputbuf, s->port);

  ftpd_data_connection_states[slot] = s;

  PSOCK_INIT(&s->sin, s->inputbuf, s->inputbuf_size);

  uip_listen(HTONS(port));

  LOG_TRACE("ftpd_alloc_data_state: done\r\n");
  return port;
}

static void ftpd_free_data_state(struct ftpd_data_state* s)
{
  LOG_TRACE("ftpd_free_data_state %p\r\n", s);

  if (!s) {
    /* It's OK because we may disconnect control connection while there
       was no data connection present. */
    LOG_TRACE("ftpd_free_data_state: s == NULL!!\r\n");
    return;
  }

  for(size_t i = 0; i < FTPD_MAX_DATA_CONNECTIONS; i++) {
    if(ftpd_data_connection_states[i] == s) {
      // erase the connection from the list
      LOG_TRACE("freed for port:: %d -> %p\r\n", ftpd_data_connection_states[i]->port, ftpd_data_connection_states[i]);
      ftpd_data_connection_states[i] = NULL;
      break;
    }
  }

  free(s->inputbuf);
  free(s);
}

static void ftpd_mark_closed_data_state(struct ftpd_data_state* s)
{
  LOG_TRACE("ftpd_mark_closed_data_state %p\r\n", s);
  s->closed = true;
  s->connected = false;
}

static void ftpd_garbage_collest_data_states()
{
  for(size_t i = 0; i < FTPD_MAX_DATA_CONNECTIONS; i++) {
    if(ftpd_data_connection_states[i] != NULL &&
        ftpd_data_connection_states[i]->closed) {
      ftpd_free_data_state(ftpd_data_connection_states[i]);
    }
  }
}

static
bool ftpd_wait_free_data_connection()
{
  ftpd_garbage_collest_data_states();

  for(size_t i = 0; i < FTPD_MAX_DATA_CONNECTIONS; i++) {
    if(ftpd_data_connection_states[i] == NULL) {
      return true;
    }
  }
  return false;
}

/*---------------------------------------------------------------------------*/

static
PT_THREAD(ftpd_data_connection(struct ftpd_data_state *s))
{
  PSOCK_BEGIN(&s->sin);

  LOG_TRACE("entered.. waiting %p\r\n", s);

  PSOCK_WAIT_UNTIL(&s->sin, s->state != Wait);
  LOG_TRACE("got new state! \r\n");

  if (s->state == List) {

      /* LIST */

      s->list_data = ftp_file_stat(s->path);
      LOG_TRACE("list.. %d, flags: %d\r\n", strlen(s->list_data), uip_flags);
      PSOCK_SEND(&s->sin, s->list_data, strlen(s->list_data));
      free(s->list_data);
      s->list_data = NULL;
      LOG_TRACE("list..done\r\n");
  } else if(s->state == Stor) {

      /* STOR */

    LOG_TRACE("stor: %s\r\n", s->path);

    s->fd = Fcreate(s->path, 0);

    if (s->fd < 0) {
      s->ftp_result_code = 400;
      LOG_TRACE(" -> failed to open!\r\n");
      break;
    }

    while (true) {
      int32_t write_ret = 0;
      PSOCK_READBUF(&s->sin);

      write_ret = Fwrite(s->fd, PSOCK_DATALEN(&s->sin), s->inputbuf);

      if (write_ret < 0 || write_ret != PSOCK_DATALEN(&s->sin)) {
        // TODO: pass any errors back on the control connection
        LOG_TRACE("Fwrite failed!\r\n");
        break;
      }

      if (uip_closed()) {
        LOG_TRACE("CLOSED!");
        break;
      }
    }
  } else if(s->state == Retr) {

    /* RETR */

    LOG_TRACE("retr: %s\r\n", s->path);

    s->fd = Fopen(s->path, 0);

    if (s->fd < 0) {
      s->ftp_result_code = 400;
      LOG_TRACE(" -> failed to open!\r\n");
      break;
    }

    s->send_buffer_size = UIP_TCP_MSS * 1;
    s->buffer_start_offset = 0;

    while (1) {
      s->bytes_read = Fread(
        s->fd,
        s->send_buffer_size-s->buffer_start_offset,
        &s->inputbuf[s->buffer_start_offset]);

      if (s->bytes_read == 0) {
        LOG_TRACE("Retr, finished, breaking\r\n");
        break;
      }

      if (s->bytes_read < 0) {
        s->ftp_result_code = 400;
        break;
      }

      PSOCK_SEND(&s->sin, s->inputbuf, s->bytes_read+s->buffer_start_offset);
      s->buffer_start_offset = 0;

      if(s->send_buffer_size != UIP_TCP_MSS * 16) {
        s->send_buffer_size <<= 1;
      }
    }
  } else {
    /* Quit */
  }

  LOG_TRACE("exit..\r\n");

  PSOCK_CLOSE_EXIT(&s->sin);
  PSOCK_END(&s->sin);
}

static void
inline handle_data_error(struct ftpd_data_state *s)
{
  if (!s) {
    return;
  }

  if (Fclose_safe(&s->fd) != -1) {
    LOG_TRACE("FClose -> failed\r\n");
  }
}

void ftpd_data_appcall()
{
  struct ftpd_data_state *s = ftpd_get_data_state(uip_conn->lport);

  if (s) {

    if(uip_connected()) {
      LOG_TRACE("ftpd_data_appcall: connected\r\n");
      uip_unlisten(s->port);
      s->connected = true;
    }

    ftpd_data_connection(s);

    if (uip_timedout() || uip_aborted() || uip_closed()) {
      s->busy = false;
      s->state = Quit;
      Fclose_safe(&s->fd);
      ftpd_mark_closed_data_state(s);
      handle_data_error(s);

      LOG_TRACE("Connection %s\r\n",
        uip_timedout() ? "timeout" :
        uip_aborted() ? "aborted" : "closed");
    }
  } else {
    LOG_TRACE("Emergency close!\r\n");
    uip_unlisten(uip_conn->lport);  // shouldn't be required!?
    uip_close();
  }

}

/*---------------------------------------------------------------------------*/

void
ftpd_init(void)
{
  uip_listen(HTONS(21));

  for(size_t i = 0; i < FTPD_MAX_DATA_CONNECTIONS; i++)
  {
      // clear the connection list
      ftpd_data_connection_states[i] = NULL;
  }
}

/*---------------------------------------------------------------------------*/
/** @} */
