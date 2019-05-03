#ifndef __SVETHLANA_H__
#define __SVETHLANA_H__

/* SuperVidel Ethernet LAN adapter driver */
/* adapted from FreeMiNT driver */
/* Copyright (c) 2007-2016 Torbjorn and Henrik Gilda */
/* SuperVidel FW v10 is required by this driver */ 

/* UIP integration (c)2019 Pawel Goralski */

#include "compiler.h"
#include <stdint.h>
#include <stdbool.h>

/* UIP adapter interface */
#include "../adapterint.h"

// ct60 video ram malloc
#include <osbind.h>

#define VMALLOC_MODE_ALLOC 0
#define VMALLOC_MODE_FREE 1
#define vmalloc(mode, value) (long)trap_14_wwl((short int)0xc60e, (short int)(mode), (long)(value))

#define SV_MINIMAL_FW_VERSION 10

#endif