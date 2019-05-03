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
#define vmalloc(mode, value) (uint32_t)trap_14_wwl((uint16_t)0xc60e, (int16_t)(mode), (int32_t)(value))

#define SV_MINIMAL_FW_VERSION 10

#endif